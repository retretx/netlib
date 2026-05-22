#pragma once

#include <netlib/execution/scheduler.hpp>
#include <netlib/net/detail/kqueue_reactor.hpp>
#include <netlib/net/detail/timer_scheduler.hpp>

#include <chrono>
#include <functional>

namespace rrmode::netlib::net::detail {

#if defined(__APPLE__)

/// Таймеры через EVFILT_TIMER в том же kqueue, что и сокеты.
class kqueue_timer_scheduler final : public timer_scheduler {
public:
    kqueue_timer_scheduler(kqueue_reactor& reactor, execution::scheduler const& sched)
        : reactor_{reactor}, sched_{sched} {}

    void run_after(std::chrono::milliseconds delay, std::function<void()> fn) override {
        if (!fn) {
            return;
        }
        reactor_.run_after(delay, [sched = sched_, fn = std::move(fn)]() mutable {
            sched.schedule(std::move(fn));
        });
    }

    void dispatch_ready() override { reactor_.dispatch_timers(sched_); }

private:
    kqueue_reactor& reactor_;
    execution::scheduler const& sched_;
};

#endif

}  // namespace rrmode::netlib::net::detail
