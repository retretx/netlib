#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/coro.hpp>

#include <atomic>
#include <chrono>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <thread>

#if defined(__linux__) || defined(__APPLE__)

using namespace std::chrono_literals;

namespace {

using rrmode::netlib::execution::scheduler;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::execution::thread_pool;
using rrmode::netlib::execution::when_all;
using rrmode::netlib::net::udp_echo_once;
using rrmode::netlib::net::udp_recv_string;
using rrmode::netlib::net::udp_send_string;

task<std::string> client_ping(scheduler& sched, rrmode::netlib::net::event_loop& loop,
                              rrmode::netlib::net::endpoint server_ep) {
    co_await sched;
    rrmode::netlib::net::udp_socket client;
    client.bind({.host = "127.0.0.1", .port = 0});
    co_await sched;
    co_await udp_send_string(client, loop, "udp-coro", server_ep);
    co_await sched;
    auto msg = co_await udp_recv_string(client, loop, 64);
    client.close(loop);
    co_return msg;
}

}  // namespace

TEST_CASE("UDP echo coroutines loopback") {
    thread_pool pool{4};
    scheduler sched{pool};
    rrmode::netlib::net::event_loop loop{sched};

    rrmode::netlib::net::udp_socket server;
    server.bind({.host = "127.0.0.1", .port = 0});
    auto const ep = server.local_endpoint();

    std::atomic<bool> stop_io{false};
    std::thread io_thread{[&loop, &stop_io] {
        while (!stop_io.load(std::memory_order_acquire)) {
            loop.run_once(10ms);
        }
    }};

    auto const response =
        sync_wait(sched, when_all(sched, udp_echo_once(server, loop), client_ping(sched, loop, ep)));

    stop_io.store(true);
    loop.stop();
    io_thread.join();
    server.close(loop);
    pool.shutdown();

    REQUIRE(response == "udp-coro");
}

#else

TEST_CASE("UDP echo coroutines loopback") { REQUIRE(true); }

#endif
