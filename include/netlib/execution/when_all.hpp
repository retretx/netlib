#pragma once

#include <netlib/execution/detail/when_all_state.hpp>
#include <netlib/execution/detail/when_all_tuple.hpp>
#include <netlib/execution/schedule_awaitable.hpp>
#include <netlib/execution/task.hpp>

#include <exception>
#include <memory>
#include <optional>
#include <type_traits>
#include <tuple>
#include <utility>
#include <vector>

namespace rrmode::netlib::execution::detail {

template<typename T, typename U>
struct when_all_pair_state {
    scheduler* sched{nullptr};
    std::mutex mutex;
    int workers_left{2};
    std::optional<T> first;
    std::optional<U> second;
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

template<typename T, typename U>
struct when_all_pair_awaitable {
    std::shared_ptr<when_all_pair_state<T, U>> state;

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

template<typename T, typename U>
task<void> when_all_run_left(scheduler& sched, task<T> child,
                             std::shared_ptr<when_all_pair_state<T, U>> state) {
    co_await sched;
    try {
        if constexpr (std::is_void_v<T>) {
            co_await std::move(child);
        } else {
            state->first = co_await std::move(child);
        }
    } catch (...) {
        state->set_exception(std::current_exception());
    }
    state->worker_finished();
}

template<typename T, typename U>
task<void> when_all_run_right(scheduler& sched, task<U> child,
                              std::shared_ptr<when_all_pair_state<T, U>> state) {
    co_await sched;
    try {
        if constexpr (std::is_void_v<U>) {
            co_await std::move(child);
        } else {
            state->second = co_await std::move(child);
        }
    } catch (...) {
        state->set_exception(std::current_exception());
    }
    state->worker_finished();
}

template<typename T, typename U>
void launch_left(scheduler& sched, task<T> child, std::shared_ptr<when_all_pair_state<T, U>> state) {
    auto h = when_all_run_left(sched, std::move(child), state).release();
    h.promise().set_scheduler(sched);
    h.resume();
}

template<typename T, typename U>
void launch_right(scheduler& sched, task<U> child, std::shared_ptr<when_all_pair_state<T, U>> state) {
    auto h = when_all_run_right(sched, std::move(child), state).release();
    h.promise().set_scheduler(sched);
    h.resume();
}

template<typename T>
struct when_all_value_state {
    scheduler* sched{nullptr};
    std::mutex mutex;
    int workers_left{2};
    std::optional<T> value;
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

template<typename T>
struct when_all_value_awaitable {
    std::shared_ptr<when_all_value_state<T>> state;

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
task<void> when_all_run_void_side(scheduler& sched, task<void> child,
                                  std::shared_ptr<when_all_value_state<T>> state) {
    co_await sched;
    try {
        co_await std::move(child);
    } catch (...) {
        state->set_exception(std::current_exception());
    }
    state->worker_finished();
}

template<typename T>
task<void> when_all_run_value_side(scheduler& sched, task<T> child,
                                   std::shared_ptr<when_all_value_state<T>> state) {
    co_await sched;
    try {
        state->value = co_await std::move(child);
    } catch (...) {
        state->set_exception(std::current_exception());
    }
    state->worker_finished();
}

template<typename T>
void launch_void_and_value(scheduler& sched, task<void> void_child, task<T> value_child,
                           std::shared_ptr<when_all_value_state<T>> state) {
    {
        auto h = when_all_run_void_side(sched, std::move(void_child), state).release();
        h.promise().set_scheduler(sched);
        h.resume();
    }
    {
        auto h = when_all_run_value_side(sched, std::move(value_child), state).release();
        h.promise().set_scheduler(sched);
        h.resume();
    }
}

struct when_all_void_pair_state {
    scheduler* sched{nullptr};
    std::mutex mutex;
    int workers_left{2};
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

struct when_all_void_pair_awaitable {
    std::shared_ptr<when_all_void_pair_state> state;

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

inline task<void> when_all_void_pair_run(scheduler& sched, task<void> child,
                                         std::shared_ptr<when_all_void_pair_state> state) {
    co_await sched;
    try {
        co_await std::move(child);
    } catch (...) {
        state->set_exception(std::current_exception());
    }
    state->worker_finished();
}

inline void launch_void_pair_side(scheduler& sched, task<void> child,
                                 std::shared_ptr<when_all_void_pair_state> state) {
    auto h = when_all_void_pair_run(sched, std::move(child), state).release();
    h.promise().set_scheduler(sched);
    h.resume();
}

}  // namespace rrmode::netlib::execution::detail

namespace rrmode::netlib::execution {

/// Параллельно выполнить две task и вернуть pair результатов.
template<typename T, typename U>
    requires(!std::is_void_v<T> && !std::is_void_v<U>)
task<std::pair<T, U>> when_all(scheduler& sched, task<T> a, task<U> b) {
    auto state = std::make_shared<detail::when_all_pair_state<T, U>>();
    state->sched = &sched;

    detail::launch_left(sched, std::move(a), state);
    detail::launch_right(sched, std::move(b), state);

    co_await detail::when_all_pair_awaitable<T, U>{state};
    co_return std::pair<T, U>{std::move(*state->first), std::move(*state->second)};
}

/// void + void.
inline task<void> when_all(scheduler& sched, task<void> a, task<void> b) {
    auto state = std::make_shared<detail::when_all_void_pair_state>();
    state->sched = &sched;

    detail::launch_void_pair_side(sched, std::move(a), state);
    detail::launch_void_pair_side(sched, std::move(b), state);

    co_await detail::when_all_void_pair_awaitable{state};
}

/// void + T → возвращает результат T (типично: фоновый сервер + клиент).
template<typename T>
    requires(!std::is_void_v<T>)
task<T> when_all(scheduler& sched, task<void> a, task<T> b) {
    auto state = std::make_shared<detail::when_all_value_state<T>>();
    state->sched = &sched;

    detail::launch_void_and_value(sched, std::move(a), std::move(b), state);

    co_await detail::when_all_value_awaitable<T>{state};
    co_return std::move(*state->value);
}

/// T + void → возвращает T.
template<typename T>
    requires(!std::is_void_v<T>)
task<T> when_all(scheduler& sched, task<T> a, task<void> b) {
    auto state = std::make_shared<detail::when_all_value_state<T>>();
    state->sched = &sched;

    detail::launch_void_and_value(sched, std::move(b), std::move(a), state);

    co_await detail::when_all_value_awaitable<T>{state};
    co_return std::move(*state->value);
}

/// Параллельно выполнить N однотипных task, вернуть vector результатов в исходном порядке.
template<typename T>
    requires(!std::is_void_v<T>)
task<std::vector<T>> when_all(scheduler& sched, std::vector<task<T>> tasks) {
    if (tasks.empty()) {
        co_return std::vector<T>{};
    }

    auto group = std::make_shared<detail::when_all_group_state>();
    group->sched = &sched;
    group->workers_left = static_cast<int>(tasks.size());

    auto results = std::make_shared<std::vector<T>>(tasks.size());

    for (std::size_t i = 0; i < tasks.size(); ++i) {
        detail::launch_when_all_worker(sched, std::move(tasks[i]), group, results, i);
    }

    co_await detail::when_all_group_awaitable{group};
    co_return std::move(*results);
}

/// Параллельно выполнить N task<void>.
inline task<void> when_all(scheduler& sched, std::vector<task<void>> tasks) {
    if (tasks.empty()) {
        co_return;
    }

    auto group = std::make_shared<detail::when_all_group_state>();
    group->sched = &sched;
    group->workers_left = static_cast<int>(tasks.size());

    for (auto& child : tasks) {
        detail::launch_when_all_void_worker(sched, std::move(child), group);
    }

    co_await detail::when_all_group_awaitable{group};
}

/// Параллельно выполнить 3+ task разных типов, вернуть tuple результатов в порядке аргументов.
template<typename... Tasks>
    requires(sizeof...(Tasks) >= 3 && detail::all_non_void_tasks_v<Tasks...>)
task<std::tuple<detail::task_value_t<Tasks>...>> when_all(scheduler& sched, Tasks... tasks) {
    using results_tuple = std::tuple<detail::task_value_t<Tasks>...>;

    auto group = std::make_shared<detail::when_all_group_state>();
    group->sched = &sched;
    group->workers_left = static_cast<int>(sizeof...(Tasks));

    auto results = std::make_shared<results_tuple>();

    detail::launch_when_all_tuple_pack(sched, group, results, std::index_sequence_for<Tasks...>{},
                                       std::move(tasks)...);

    co_await detail::when_all_group_awaitable{group};
    co_return std::move(*results);
}

}  // namespace rrmode::netlib::execution
