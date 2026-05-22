#pragma once

#include <netlib/execution/scheduler.hpp>

#include <coroutine>

namespace rrmode::netlib::execution {

/// Awaitable: выполнить продолжение на executor планировщика.
struct schedule_awaitable {
    scheduler const& sched;

    bool await_ready() const noexcept { return false; }

    template<typename Promise>
    void await_suspend(std::coroutine_handle<Promise> h) const {
        if constexpr (requires(Promise& p, scheduler& s) { p.set_scheduler(s); }) {
            h.promise().set_scheduler(const_cast<scheduler&>(sched));
        }
        sched.schedule([h]() mutable { h.resume(); });
    }

    void await_resume() const noexcept {}
};

inline schedule_awaitable operator co_await(scheduler const& sched) { return schedule_awaitable{sched}; }

}  // namespace rrmode::netlib::execution
