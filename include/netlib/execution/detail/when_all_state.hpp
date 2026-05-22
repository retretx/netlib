#pragma once

#include <netlib/execution/scheduler.hpp>
#include <netlib/execution/task.hpp>

#include <coroutine>
#include <exception>
#include <memory>
#include <mutex>
#include <vector>

namespace rrmode::netlib::execution::detail {

struct when_all_group_state {
    scheduler* sched{nullptr};
    std::mutex mutex;
    int workers_left{0};
    std::exception_ptr exception;
    std::coroutine_handle<> waiter{};

    void worker_finished() {
        std::scoped_lock lock{mutex};
        if (--workers_left == 0 && waiter && sched != nullptr) {
            sched->schedule([h = waiter]() mutable { h.resume(); });
        }
    }

    void set_exception(std::exception_ptr ex) noexcept {
        std::scoped_lock lock{mutex};
        if (!exception) {
            exception = std::move(ex);
        }
    }
};

struct when_all_group_awaitable {
    std::shared_ptr<when_all_group_state> state;

    bool await_ready() const {
        std::scoped_lock lock{state->mutex};
        return state->workers_left == 0;
    }

    void await_suspend(std::coroutine_handle<> h) const {
        std::scoped_lock lock{state->mutex};
        if (state->workers_left == 0) {
            return;
        }
        state->waiter = h;
    }

    void await_resume() const {
        if (state->exception) {
            std::rethrow_exception(state->exception);
        }
    }
};

template<typename T>
task<void> when_all_worker_coro(scheduler& sched, task<T> child,
                                std::shared_ptr<when_all_group_state> group,
                                std::shared_ptr<std::vector<T>> results, std::size_t index) {
    co_await sched;
    try {
        (*results)[index] = co_await std::move(child);
    } catch (...) {
        group->set_exception(std::current_exception());
    }
    group->worker_finished();
}

template<typename T>
void launch_when_all_worker(scheduler& sched, task<T> child,
                            std::shared_ptr<when_all_group_state> group,
                            std::shared_ptr<std::vector<T>> results, std::size_t index) {
    auto h = when_all_worker_coro(sched, std::move(child), group, results, index).release();
    h.promise().set_scheduler(sched);
    h.resume();
}

inline task<void> when_all_void_worker_coro(scheduler& sched, task<void> child,
                                           std::shared_ptr<when_all_group_state> group) {
    co_await sched;
    try {
        co_await std::move(child);
    } catch (...) {
        group->set_exception(std::current_exception());
    }
    group->worker_finished();
}

inline void launch_when_all_void_worker(scheduler& sched, task<void> child,
                                        std::shared_ptr<when_all_group_state> group) {
    auto h = when_all_void_worker_coro(sched, std::move(child), group).release();
    h.promise().set_scheduler(sched);
    h.resume();
}

}  // namespace rrmode::netlib::execution::detail
