#pragma once

#include <netlib/execution/detail/task_promise.hpp>
#include <netlib/execution/schedule_awaitable.hpp>
#include <netlib/execution/task.hpp>

#include <atomic>
#include <coroutine>
#include <exception>
#include <optional>
#include <type_traits>
#include <utility>

namespace rrmode::netlib::execution {

/// Асинхронный генератор: `while (auto v = co_await gen.next(sched))`.
template<typename T>
class [[nodiscard]] generator {
public:
    struct promise_type : detail::scheduler_holder {
        std::optional<T> current;
        std::exception_ptr exception;
        std::coroutine_handle<> consumer{};
        std::atomic<bool> completion_resume_sent{false};

        generator get_return_object() noexcept {
            return generator{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept { return {}; }

        std::suspend_always final_suspend() noexcept { return {}; }

        struct yield_awaiter {
            promise_type& promise;

            bool await_ready() noexcept { return false; }

            void await_suspend(std::coroutine_handle<>) noexcept {
                if (promise.consumer && promise.sched != nullptr) {
                    promise.sched->schedule(
                        [h = promise.consumer]() mutable { h.resume(); });
                }
            }

            void await_resume() noexcept {}
        };

        yield_awaiter yield_value(T value) {
            current = std::move(value);
            return yield_awaiter{*this};
        }

        void unhandled_exception() noexcept { exception = std::current_exception(); }

        void return_void() noexcept {}

        void rethrow_if_exception() const {
            if (exception) {
                std::rethrow_exception(exception);
            }
        }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    generator() noexcept = default;

    generator(generator const&) = delete;
    generator& operator=(generator const&) = delete;

    generator(generator&& other) noexcept : coro_{std::exchange(other.coro_, {})} {}
    generator& operator=(generator&& other) noexcept {
        if (this != &other) {
            destroy();
            coro_ = std::exchange(other.coro_, {});
        }
        return *this;
    }

    ~generator() { destroy(); }

    /// Следующее значение; `std::nullopt` — генератор исчерпан.
    struct next_awaitable {
        generator* self;
        scheduler& sched;

        bool await_ready() const noexcept { return self == nullptr; }

        void await_suspend(std::coroutine_handle<> h) const {
            if (!self) {
                return;
            }
            auto& prom = self->coro_.promise();
            prom.set_scheduler(sched);
            prom.consumer = h;
            prom.completion_resume_sent.store(false, std::memory_order_relaxed);
            if (self->coro_.done()) {
                return;
            }
            self->coro_.resume();
            if (self->coro_.done() &&
                !prom.completion_resume_sent.exchange(true, std::memory_order_acq_rel)) {
                prom.sched->schedule([h]() mutable { h.resume(); });
            }
        }

        std::optional<T> await_resume() const {
            if (!self) {
                return std::nullopt;
            }
            if (self->coro_.done() && !self->coro_.promise().current) {
                return std::nullopt;
            }
            auto& prom = self->coro_.promise();
            prom.rethrow_if_exception();
            if (!prom.current) {
                return std::nullopt;
            }
            auto value = std::move(*prom.current);
            prom.current.reset();
            return value;
        }
    };

    [[nodiscard]] next_awaitable next(scheduler& sched) noexcept { return next_awaitable{this, sched}; }

    [[nodiscard]] bool done() const noexcept { return !coro_ || coro_.done(); }

private:
    explicit generator(handle_type h) noexcept : coro_{h} {}

    void destroy() noexcept {
        if (coro_) {
            coro_.destroy();
            coro_ = {};
        }
    }

    handle_type coro_{};
};

}  // namespace rrmode::netlib::execution
