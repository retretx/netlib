#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/coro.hpp>
#include <netlib/net/tcp_acceptor.hpp>

#include <atomic>
#include <chrono>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <thread>
#include <vector>

#if defined(__linux__) || defined(__APPLE__)

using namespace std::chrono_literals;

namespace {

using rrmode::netlib::execution::scheduler;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::execution::thread_pool;
using rrmode::netlib::execution::when_all;
using rrmode::netlib::net::connect_async;
using rrmode::netlib::net::read_string_async;
using rrmode::netlib::net::tcp_serve_echo_once;
using rrmode::netlib::net::write_all_async;

task<std::string> client_echo(scheduler& sched, rrmode::netlib::net::event_loop& loop,
                              rrmode::netlib::net::endpoint ep) {
    co_await sched;
    rrmode::netlib::net::tcp_socket client;
    co_await connect_async(client, loop, ep);
    co_await sched;
    co_await write_all_async(client, loop, std::vector<char>{'c', 'o', 'r', 'o'});
    co_await sched;
    auto msg = co_await read_string_async(client, loop, 64);
    client.close(loop);
    co_return msg;
}

}  // namespace

TEST_CASE("tcp echo loopback coroutines") {
    thread_pool pool{4};
    scheduler sched{pool};
    rrmode::netlib::net::event_loop loop{sched};

    rrmode::netlib::net::tcp_acceptor acceptor;
    acceptor.open({.host = "127.0.0.1", .port = 0});
    auto const ep = acceptor.local_endpoint();

    std::atomic<bool> stop_io{false};
    std::thread io_thread{[&loop, &stop_io] {
        while (!stop_io.load(std::memory_order_acquire)) {
            loop.run_once(10ms);
        }
    }};

    auto const response = sync_wait(
        sched, when_all(sched, tcp_serve_echo_once(acceptor, loop), client_echo(sched, loop, ep)));

    stop_io.store(true);
    loop.stop();
    io_thread.join();
    acceptor.close(loop);
    pool.shutdown();

    REQUIRE(response == "coro");
}

#else

TEST_CASE("tcp echo loopback coroutines") { REQUIRE(true); }

#endif
