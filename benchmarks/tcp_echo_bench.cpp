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
#include <vector>

#if !defined(_WIN32)

using rrmode::netlib::execution::scheduler;
using rrmode::netlib::execution::spawn;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::execution::thread_pool;
using rrmode::netlib::net::connect_async;
using rrmode::netlib::net::read_string_async;
using rrmode::netlib::net::tcp_acceptor;
using rrmode::netlib::net::tcp_serve_echo_once;
using rrmode::netlib::net::write_all_async;

namespace {

task<void> serve_loop(scheduler& sched, rrmode::netlib::net::event_loop& loop, tcp_acceptor& acc) {
    for (;;) {
        co_await sched;
        co_await tcp_serve_echo_once(acc, loop);
    }
}

task<void> client_once(scheduler& sched, rrmode::netlib::net::event_loop& loop,
                       rrmode::netlib::net::endpoint ep, std::string const& payload) {
    co_await sched;
    rrmode::netlib::net::tcp_socket client;
    co_await connect_async(client, loop, ep);
    co_await write_all_async(client, loop, std::vector<char>{payload.begin(), payload.end()});
    (void)co_await read_string_async(client, loop, 256);
    client.close(loop);
}

}  // namespace

int main(int argc, char** argv) {
    auto const iters = netlib_benchmark::parse_iterations(argc, argv, 500);
    std::string const payload(64, 'x');

    thread_pool pool{4};
    scheduler sched{pool};
    rrmode::netlib::net::event_loop loop{sched};

    tcp_acceptor acceptor;
    acceptor.open({.host = "127.0.0.1", .port = 0});
    auto const ep = acceptor.local_endpoint();

    std::atomic<bool> stop_io{false};
    std::thread io_thread{[&] {
        while (!stop_io.load(std::memory_order_acquire)) {
            loop.run_once(std::chrono::milliseconds{1});
        }
    }};

    spawn(sched, serve_loop(sched, loop, acceptor));
    std::this_thread::sleep_for(std::chrono::milliseconds{20});

    auto const start = netlib_benchmark::clock::now();
    for (std::size_t i = 0; i < iters; ++i) {
        sync_wait(sched, client_once(sched, loop, ep, payload));
    }
    auto const end = netlib_benchmark::clock::now();

    stop_io.store(true);
    loop.stop();
    io_thread.join();
    acceptor.close(loop);
    pool.shutdown();

    netlib_benchmark::print_throughput("tcp_echo_bench", iters,
                                       netlib_benchmark::elapsed_ms(start, end));
    return 0;
}

#else

int main() {
    std::cerr << "tcp_echo_bench: только POSIX\n";
    return 1;
}

#endif
