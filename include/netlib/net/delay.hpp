#pragma once

#include <netlib/net/event_loop.hpp>

#include <atomic>
#include <chrono>
#include <coroutine>
#include <memory>

namespace rrmode::netlib::net {

namespace detail {

struct loop_delay_state {
    std::atomic<bool> done{false};
};

}  // namespace detail

/// co_await delay_async(loop, duration) — таймер event_loop; pump run_once на scheduler.
template<typename Rep, typename Period>
struct loop_delay_awaitable {
    event_loop& loop;
    std::chrono::duration<Rep, Period> duration;

    [[nodiscard]] bool await_ready() const noexcept { return duration <= duration.zero(); }

    void await_suspend(std::coroutine_handle<> h) const {
        event_loop& lp = loop;
        auto state = std::make_shared<detail::loop_delay_state>();
        auto const ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

        lp.run_after(ms, [state, h, &lp]() mutable {
            state->done.store(true, std::memory_order_release);
            lp.scheduler().schedule([h]() mutable { h.resume(); });
        });

        auto const poll_timeout = lp.uses_kernel_timers() ? std::chrono::milliseconds{100}
                                                          : std::chrono::milliseconds{1};
        lp.scheduler().schedule([&lp, state, poll_timeout]() mutable {
            while (!state->done.load(std::memory_order_acquire)) {
                (void)lp.run_once(poll_timeout);
            }
        });
    }

    void await_resume() const noexcept {}
};

template<typename Rep, typename Period>
[[nodiscard]] loop_delay_awaitable<Rep, Period> delay_async(event_loop& loop,
                                                            std::chrono::duration<Rep, Period> duration) {
    return loop_delay_awaitable<Rep, Period>{loop, duration};
}

}  // namespace rrmode::netlib::net
