#pragma once

#include <netlib/execution/detail/task_promise.hpp>
#include <netlib/execution/error.hpp>
#include <netlib/execution/scheduler.hpp>

#include <coroutine>
#include <exception>
#include <optional>
#include <type_traits>
#include <utility>

namespace rrmode::netlib::execution {

template<typename T>
class [[nodiscard]] task;

namespace detail {

template<typename T>
struct task_promise_storage : scheduler_holder {
    std::coroutine_handle<> continuation{};
    sync_wait_context* sync_ctx{nullptr};
    std::exception_ptr exception{};
    std::optional<T> value{};

    void rethrow_if_exception() const {
        if (exception) {
            std::rethrow_exception(exception);
        }
    }

    T take_result() {
        rethrow_if_exception();
        return std::move(*value);
    }
};

template<>
struct task_promise_storage<void> : scheduler_holder {
    std::coroutine_handle<> continuation{};
    sync_wait_context* sync_ctx{nullptr};
    std::exception_ptr exception{};

    void rethrow_if_exception() const {
        if (exception) {
            std::rethrow_exception(exception);
        }
    }
};

}  // namespace detail

template<typename T>
class [[nodiscard]] task {
public:
    using value_type = T;

    struct promise_type : detail::task_promise_storage<T> {
        using storage = detail::task_promise_storage<T>;

        task get_return_object() noexcept {
            return task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept { return {}; }

        struct final_awaiter {
            storage& state;
            bool await_ready() noexcept { return false; }
            void await_suspend(std::coroutine_handle<> self) noexcept {
                detail::resume_on_scheduler(state, state.continuation, state.sync_ctx, self);
            }
            void await_resume() noexcept {}
        };

        final_awaiter final_suspend() noexcept { return final_awaiter{*this}; }

        template<typename U>
            requires std::convertible_to<U, T>
        void return_value(U&& v) {
            this->value.emplace(std::forward<U>(v));
        }

        void unhandled_exception() noexcept { this->exception = std::current_exception(); }

        void set_continuation(std::coroutine_handle<> h) noexcept { this->continuation = h; }
        void set_sync_context(detail::sync_wait_context& ctx) noexcept { this->sync_ctx = &ctx; }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    task() noexcept = default;

    task(task const&) = delete;
    task& operator=(task const&) = delete;

    task(task&& other) noexcept : coro_{std::exchange(other.coro_, {})} {}
    task& operator=(task&& other) noexcept {
        if (this != &other) {
            destroy();
            coro_ = std::exchange(other.coro_, {});
        }
        return *this;
    }

    ~task() { destroy(); }

    [[nodiscard]] bool await_ready() const noexcept { return coro_ && coro_.done(); }

    template<typename OtherPromise>
    void await_suspend(std::coroutine_handle<OtherPromise> parent) const {
        if (!coro_) {
            return;
        }
        auto& prom = coro_.promise();
        prom.inherit_scheduler(parent.promise().get_scheduler());
        prom.set_continuation(parent);
        if (!coro_.done()) {
            coro_.resume();
        }
    }

    T await_resume() const {
        if (!coro_) {
            throw execution_error("task: пустое coroutine handle");
        }
        return coro_.promise().take_result();
    }

    [[nodiscard]] handle_type release() noexcept { return std::exchange(coro_, {}); }

private:
    explicit task(handle_type h) noexcept : coro_{h} {}

    void destroy() noexcept {
        if (coro_) {
            coro_.destroy();
            coro_ = {};
        }
    }

    handle_type coro_{};
};

template<>
class [[nodiscard]] task<void> {
public:
    using value_type = void;

    struct promise_type : detail::task_promise_storage<void> {
        using storage = detail::task_promise_storage<void>;

        task<void> get_return_object() noexcept {
            return task<void>{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept { return {}; }

        struct final_awaiter {
            storage& state;
            bool await_ready() noexcept { return false; }
            void await_suspend(std::coroutine_handle<> self) noexcept {
                detail::resume_on_scheduler(state, state.continuation, state.sync_ctx, self);
            }
            void await_resume() noexcept {}
        };

        final_awaiter final_suspend() noexcept { return final_awaiter{*this}; }

        void return_void() noexcept {}
        void unhandled_exception() noexcept { this->exception = std::current_exception(); }

        void set_continuation(std::coroutine_handle<> h) noexcept { this->continuation = h; }
        void set_sync_context(detail::sync_wait_context& ctx) noexcept { this->sync_ctx = &ctx; }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    task() noexcept = default;

    task(task const&) = delete;
    task& operator=(task const&) = delete;

    task(task&& other) noexcept : coro_{std::exchange(other.coro_, {})} {}
    task& operator=(task&& other) noexcept {
        if (this != &other) {
            destroy();
            coro_ = std::exchange(other.coro_, {});
        }
        return *this;
    }

    ~task() { destroy(); }

    [[nodiscard]] bool await_ready() const noexcept { return coro_ && coro_.done(); }

    template<typename OtherPromise>
    void await_suspend(std::coroutine_handle<OtherPromise> parent) const {
        if (!coro_) {
            return;
        }
        auto& prom = coro_.promise();
        prom.inherit_scheduler(parent.promise().get_scheduler());
        prom.set_continuation(parent);
        if (!coro_.done()) {
            coro_.resume();
        }
    }

    void await_resume() const {
        if (!coro_) {
            throw execution_error("task: пустое coroutine handle");
        }
        coro_.promise().rethrow_if_exception();
    }

    [[nodiscard]] handle_type release() noexcept { return std::exchange(coro_, {}); }

private:
    explicit task(handle_type h) noexcept : coro_{h} {}

    void destroy() noexcept {
        if (coro_) {
            coro_.destroy();
            coro_ = {};
        }
    }

    handle_type coro_{};
};

template<typename T>
T sync_wait(scheduler& sched, task<T>&& t) {
    detail::sync_wait_context ctx;
    auto h = t.release();
    if (!h) {
        throw execution_error("sync_wait: пустая task");
    }
    h.promise().set_scheduler(sched);
    h.promise().set_sync_context(ctx);
    h.resume();
    ctx.wait();
    T result = h.promise().take_result();
    h.destroy();
    return result;
}

inline void sync_wait(scheduler& sched, task<void>&& t) {
    detail::sync_wait_context ctx;
    auto h = t.release();
    if (!h) {
        throw execution_error("sync_wait: пустая task");
    }
    h.promise().set_scheduler(sched);
    h.promise().set_sync_context(ctx);
    h.resume();
    ctx.wait();
    h.promise().rethrow_if_exception();
    h.destroy();
}

}  // namespace rrmode::netlib::execution
