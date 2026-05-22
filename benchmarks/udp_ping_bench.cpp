#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/spawn.hpp>
#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/coro.hpp>

#include "bench_common.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#if !defined(_WIN32)

using rrmode::netlib::execution::scheduler;
using rrmode::netlib::execution::spawn;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::execution::thread_pool;
using rrmode::netlib::net::udp_echo_once;
using rrmode::netlib::net::udp_recv_string;
using rrmode::netlib::net::udp_send_string;

namespace {

task<void> serve_loop(scheduler& sched, rrmode::netlib::net::event_loop& loop,
                      rrmode::netlib::net::udp_socket& server) {
    for (;;) {
        co_await sched;
        co_await udp_echo_once(server, loop, 2048);
    }
}

task<void> ping_once(scheduler& sched, rrmode::netlib::net::event_loop& loop,
                     rrmode::netlib::net::endpoint ep, std::string const& msg) {
    co_await sched;
    rrmode::netlib::net::udp_socket client;
    client.bind({.host = "127.0.0.1", .port = 0});
    co_await udp_send_string(client, loop, msg, ep);
    (void)co_await udp_recv_string(client, loop, 2048);
    client.close(loop);
}

}  // namespace

int main(int argc, char** argv) {
    auto const iters = netlib_benchmark::parse_iterations(argc, argv, 2'000);
    std::string const msg(32, 'u');

    thread_pool pool{4};
    scheduler sched{pool};
    rrmode::netlib::net::event_loop loop{sched};

    rrmode::netlib::net::udp_socket server;
    server.bind({.host = "127.0.0.1", .port = 0});
    auto const ep = server.local_endpoint();

    std::atomic<bool> stop_io{false};
    std::thread io_thread{[&] {
        while (!stop_io.load(std::memory_order_acquire)) {
            loop.run_once(std::chrono::milliseconds{1});
        }
    }};

    spawn(sched, serve_loop(sched, loop, server));
    std::this_thread::sleep_for(std::chrono::milliseconds{20});

    auto const start = netlib_benchmark::clock::now();
    for (std::size_t i = 0; i < iters; ++i) {
        sync_wait(sched, ping_once(sched, loop, ep, msg));
    }
    auto const end = netlib_benchmark::clock::now();

    stop_io.store(true);
    loop.stop();
    io_thread.join();
    server.close(loop);
    pool.shutdown();

    netlib_benchmark::print_throughput("udp_ping_bench", iters,
                                       netlib_benchmark::elapsed_ms(start, end));
    return 0;
}

#else

int main() {
    std::cerr << "udp_ping_bench: только POSIX\n";
    return 1;
}

#endif
