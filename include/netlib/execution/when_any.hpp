#pragma once

#include <netlib/execution/delay.hpp>
#include <netlib/execution/error.hpp>
#include <netlib/execution/schedule_awaitable.hpp>
#include <netlib/execution/task.hpp>
#include <netlib/execution/timeout_error.hpp>

#include <chrono>
#include <coroutine>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

namespace rrmode::netlib::execution::detail {

template<typename T>
struct when_any_state {
    scheduler* sched{nullptr};
    std::mutex mutex;
    bool done{false};
    std::optional<T> value;
    std::exception_ptr exception;
    std::coroutine_handle<> waiter{};
    std::function<void()> on_loser_a{};
    std::function<void()> on_loser_b{};

    void resume_waiter() {
        if (waiter && sched != nullptr) {
            sched->schedule([h = waiter]() mutable { h.resume(); });
        }
    }

    void notify_loser(int const winner_side) noexcept {
        if (winner_side == 0 && on_loser_b) {
            on_loser_b();
        } else if (winner_side == 1 && on_loser_a) {
            on_loser_a();
        }
    }

    bool try_finish_value(T v, int const winner_side) {
        std::scoped_lock lock{mutex};
        if (done) {
            return false;
        }
        done = true;
        value = std::move(v);
        notify_loser(winner_side);
        resume_waiter();
        return true;
    }

    bool try_finish_exception(std::exception_ptr ex, int const winner_side) noexcept {
        std::scoped_lock lock{mutex};
        if (done) {
            return false;
        }
        done = true;
        exception = std::move(ex);
        notify_loser(winner_side);
        resume_waiter();
        return true;
    }
};

template<typename T>
struct when_any_awaitable {
    std::shared_ptr<when_any_state<T>> state;

    bool await_ready() const {
        std::scoped_lock lock{state->mutex};
        return state->done;
    }

    void await_suspend(std::coroutine_handle<> h) const {
        std::scoped_lock lock{state->mutex};
        if (state->done) {
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
inline task<void> when_any_run(scheduler& sched, task<T> child, std::shared_ptr<when_any_state<T>> state,
                               int const side) {
    co_await sched;
    try {
        T value = co_await std::move(child);
        (void)state->try_finish_value(std::move(value), side);
    } catch (...) {
        (void)state->try_finish_exception(std::current_exception(), side);
    }
}

template<typename T>
inline void launch_when_any(scheduler& sched, task<T> child, std::shared_ptr<when_any_state<T>> state,
                            int const side) {
    auto h = when_any_run(sched, std::move(child), state, side).release();
    h.promise().set_scheduler(sched);
    h.resume();
}

template<typename T>
inline task<void> when_any_void_competitor_run(scheduler& sched, task<void> child,
                                               std::shared_ptr<when_any_state<T>> state) {
    co_await sched;
    try {
        co_await std::move(child);
        (void)state->try_finish_exception(
            std::make_exception_ptr(execution_error("when_any: competitor завершился без исключения")),
            1);
    } catch (...) {
        (void)state->try_finish_exception(std::current_exception(), 1);
    }
}

template<typename T>
inline void launch_when_any_void_competitor(scheduler& sched, task<void> child,
                                            std::shared_ptr<when_any_state<T>> state) {
    auto h = when_any_void_competitor_run(sched, std::move(child), state).release();
    h.promise().set_scheduler(sched);
    h.resume();
}

struct when_any_void_state {
    scheduler* sched{nullptr};
    std::mutex mutex;
    bool done{false};
    bool primary_won{false};
    std::exception_ptr exception;
    std::coroutine_handle<> waiter{};
    std::function<void()> on_loser_primary{};
    std::function<void()> on_loser_competitor{};

    void resume_waiter() {
        if (waiter && sched != nullptr) {
            sched->schedule([h = waiter]() mutable { h.resume(); });
        }
    }

    void notify_loser(int const winner_side) noexcept {
        if (winner_side == 0 && on_loser_competitor) {
            on_loser_competitor();
        } else if (winner_side == 1 && on_loser_primary) {
            on_loser_primary();
        }
    }

    bool try_primary_done() {
        std::scoped_lock lock{mutex};
        if (done) {
            return false;
        }
        done = true;
        primary_won = true;
        notify_loser(0);
        resume_waiter();
        return true;
    }

    bool try_finish_exception(std::exception_ptr ex, int const winner_side) noexcept {
        std::scoped_lock lock{mutex};
        if (done) {
            return false;
        }
        done = true;
        exception = std::move(ex);
        notify_loser(winner_side);
        resume_waiter();
        return true;
    }
};

struct when_any_void_awaitable {
    std::shared_ptr<when_any_void_state> state;

    bool await_ready() const {
        std::scoped_lock lock{state->mutex};
        return state->done;
    }

    void await_suspend(std::coroutine_handle<> h) const {
        std::scoped_lock lock{state->mutex};
        if (state->done) {
            return;
        }
        state->waiter = h;
    }

    void await_resume() const {
        if (state->exception) {
            std::rethrow_exception(state->exception);
        }
        if (!state->primary_won) {
            throw execution_error("when_any: void primary не завершился");
        }
    }
};

inline task<void> when_any_void_primary_run(scheduler& sched, task<void> child,
                                            std::shared_ptr<when_any_void_state> state) {
    co_await sched;
    try {
        co_await std::move(child);
        (void)state->try_primary_done();
    } catch (...) {
        (void)state->try_finish_exception(std::current_exception(), 0);
    }
}

inline void launch_when_any_void_primary(scheduler& sched, task<void> child,
                                         std::shared_ptr<when_any_void_state> state) {
    auto h = when_any_void_primary_run(sched, std::move(child), state).release();
    h.promise().set_scheduler(sched);
    h.resume();
}

inline task<void> when_any_void_competitor_run(scheduler& sched, task<void> child,
                                               std::shared_ptr<when_any_void_state> state) {
    co_await sched;
    try {
        co_await std::move(child);
        (void)state->try_finish_exception(
            std::make_exception_ptr(execution_error("when_any: competitor завершился без исключения")),
            1);
    } catch (...) {
        (void)state->try_finish_exception(std::current_exception(), 1);
    }
}

inline void launch_when_any_void_competitor(scheduler& sched, task<void> child,
                                            std::shared_ptr<when_any_void_state> state) {
    auto h = when_any_void_competitor_run(sched, std::move(child), state).release();
    h.promise().set_scheduler(sched);
    h.resume();
}

}  // namespace rrmode::netlib::execution::detail

namespace rrmode::netlib::execution {

/// Первый завершившийся task<T> определяет результат; on_loser_* вызывается для проигравшей стороны.
template<typename T>
    requires(!std::is_void_v<T>)
task<T> when_any(scheduler& sched, task<T> a, task<T> b, std::function<void()> on_loser_a = nullptr,
                 std::function<void()> on_loser_b = nullptr) {
    auto state = std::make_shared<detail::when_any_state<T>>();
    state->sched = &sched;
    state->on_loser_a = std::move(on_loser_a);
    state->on_loser_b = std::move(on_loser_b);

    detail::launch_when_any(sched, std::move(a), state, 0);
    detail::launch_when_any(sched, std::move(b), state, 1);

    co_await detail::when_any_awaitable<T>{state};
    co_return std::move(*state->value);
}

/// primary vs competitor (обычно timeout_task, бросающий timeout_error).
template<typename T>
    requires(!std::is_void_v<T>)
task<T> when_any(scheduler& sched, task<T> primary, task<void> competitor,
                 std::function<void()> on_primary_lost = nullptr) {
    auto state = std::make_shared<detail::when_any_state<T>>();
    state->sched = &sched;
    state->on_loser_a = std::move(on_primary_lost);

    detail::launch_when_any(sched, std::move(primary), state, 0);
    detail::launch_when_any_void_competitor(sched, std::move(competitor), state);

    co_await detail::when_any_awaitable<T>{state};
    co_return std::move(*state->value);
}

/// Соревнование task<void> с таймаутом / competitor.
inline task<void> when_any(scheduler& sched, task<void> primary, task<void> competitor,
                           std::function<void()> on_primary_lost = nullptr,
                           std::function<void()> on_competitor_lost = nullptr) {
    auto state = std::make_shared<detail::when_any_void_state>();
    state->sched = &sched;
    state->on_loser_primary = std::move(on_primary_lost);
    state->on_loser_competitor = std::move(on_competitor_lost);

    detail::launch_when_any_void_primary(sched, std::move(primary), state);
    detail::launch_when_any_void_competitor(sched, std::move(competitor), state);

    co_await detail::when_any_void_awaitable{state};
}

/// Соревнование с таймаутом: по истечении срока — on_expired (если задан) и timeout_error.
inline task<void> timeout_after(scheduler& sched, std::chrono::milliseconds limit,
                               std::function<void()> on_expired = nullptr) {
    co_await delay_async(sched, limit);
    if (on_expired) {
        on_expired();
    }
    throw timeout_error("with_timeout: превышен лимит времени");
}

template<typename T>
    requires(!std::is_void_v<T>)
task<T> with_timeout(scheduler& sched, task<T> work, std::chrono::milliseconds limit,
                    std::function<void()> on_expired = nullptr,
                    std::function<void()> on_work_lost = nullptr) {
    co_return co_await when_any(sched, std::move(work), timeout_after(sched, limit, on_expired),
                                std::move(on_work_lost));
}

inline task<void> with_timeout(scheduler& sched, task<void> work, std::chrono::milliseconds limit,
                              std::function<void()> on_expired = nullptr,
                              std::function<void()> on_work_lost = nullptr) {
    co_await when_any(sched, std::move(work), timeout_after(sched, limit, on_expired),
                      std::move(on_work_lost));
}

}  // namespace rrmode::netlib::execution
