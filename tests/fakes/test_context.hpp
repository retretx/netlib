#pragma once

#include <netlib/execution/scheduler.hpp>
#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/event_loop.hpp>

#include "fake_reactor.hpp"
#include "fake_socket_backend.hpp"

#include <chrono>
#include <memory>
#include <thread>

namespace netlib::fakes {

/// Среда unit-теста: pool + scheduler + event_loop на фейках.
struct test_context {
    rrmode::netlib::execution::thread_pool pool{2};
    fake_socket_backend sockets;
    rrmode::netlib::execution::scheduler sched;
    rrmode::netlib::net::event_loop loop;

    test_context()
        : sched{pool}
        , loop{sched, std::unique_ptr<rrmode::netlib::net::detail::reactor_backend>{
                   std::make_unique<fake_reactor>()}} {
        sockets.attach_reactor(reactor());
    }

    fake_reactor& reactor() {
        return static_cast<fake_reactor&>(loop.reactor());
    }

    void flush(int rounds = 50) {
        for (int i = 0; i < rounds; ++i) {
            reactor().dispatch_pending();
            loop.run_once(std::chrono::milliseconds{0});
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
    }
};

}  // namespace netlib::fakes
