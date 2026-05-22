#pragma once

#include <netlib/net/event_loop.hpp>

#include <atomic>
#include <chrono>
#include <thread>

namespace netlib_examples {

/// Фоновый поток: loop.run_once() до stop().
class io_runner {
public:
    explicit io_runner(rrmode::netlib::net::event_loop& loop,
                       std::chrono::milliseconds tick = std::chrono::milliseconds{10})
        : loop_{loop}, tick_{tick} {
        thread_ = std::thread([this] {
            while (!stop_.load(std::memory_order_acquire)) {
                loop_.run_once(tick_);
            }
        });
    }

    io_runner(io_runner const&) = delete;
    io_runner& operator=(io_runner const&) = delete;

    ~io_runner() { stop(); }

    void stop() {
        if (stop_.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        loop_.stop();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

private:
    rrmode::netlib::net::event_loop& loop_;
    std::chrono::milliseconds tick_;
    std::atomic<bool> stop_{false};
    std::thread thread_;
};

}  // namespace netlib_examples
