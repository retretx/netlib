#pragma once

#include <chrono>
#include <functional>
#include <memory>

namespace rrmode::netlib::net {

class event_loop;

namespace detail {

class timer_scheduler {
public:
    virtual ~timer_scheduler() = default;

    virtual void run_after(std::chrono::milliseconds delay, std::function<void()> fn) = 0;

    virtual void dispatch_ready() = 0;
};

}  // namespace detail
}  // namespace rrmode::netlib::net
