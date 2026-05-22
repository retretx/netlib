#pragma once

#include <netlib/execution/scheduler.hpp>

#include <chrono>
#include <coroutine>
#include <thread>

namespace rrmode::netlib::execution {

/// co_await delay_async(sched, duration) — пауза без блокировки потока, выполняющего корутину.
template<typename Rep, typename Period>
struct delay_awaitable {
    scheduler& sched;
    std::chrono::duration<Rep, Period> duration;

    [[nodiscard]] bool await_ready() const noexcept { return duration <= duration.zero(); }

    void await_suspend(std::coroutine_handle<> h) const {
        auto const wait_for = duration;
        sched.get_executor().post([sched = std::ref(sched), wait_for, h]() mutable {
            if (wait_for > wait_for.zero()) {
                std::this_thread::sleep_for(wait_for);
            }
            sched.get().schedule([h]() mutable { h.resume(); });
        });
    }

    void await_resume() const noexcept {}
};

template<typename Rep, typename Period>
[[nodiscard]] delay_awaitable<Rep, Period> delay_async(scheduler& sched,
                                                        std::chrono::duration<Rep, Period> duration) {
    return delay_awaitable<Rep, Period>{sched, duration};
}

}  // namespace rrmode::netlib::execution
