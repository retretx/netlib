#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/coro.hpp>

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
using rrmode::netlib::net::cancellation_source;
using rrmode::netlib::net::connect_async;
using rrmode::netlib::net::read_string_async;
using rrmode::netlib::net::tcp_echo_server_loop;
using rrmode::netlib::net::write_all_async;

task<void> run_server(scheduler& sched, rrmode::netlib::net::event_loop& loop,
                      rrmode::netlib::net::tcp_acceptor& acceptor, cancellation_source& shutdown) {
    co_await sched;
    co_await tcp_echo_server_loop(acceptor, loop, &shutdown.token());
}

task<std::string> one_client(scheduler& sched, rrmode::netlib::net::event_loop& loop,
                             rrmode::netlib::net::endpoint ep) {
    co_await sched;
    rrmode::netlib::net::tcp_socket client;
    co_await connect_async(client, loop, ep);
    co_await write_all_async(client, loop, std::vector<char>{'b', 'y', 'e'});
    auto msg = co_await read_string_async(client, loop, 16);
    client.close(loop);
    co_return msg;
}

}  // namespace

TEST_CASE("tcp_echo_server_loop: cancel завершает сервер") {
    thread_pool pool{4};
    scheduler sched{pool};
    rrmode::netlib::net::event_loop loop{sched};

    rrmode::netlib::net::tcp_acceptor acceptor;
    acceptor.open({.host = "127.0.0.1", .port = 0});
    auto const ep = acceptor.local_endpoint();

    cancellation_source shutdown;
    std::atomic<bool> server_done{false};

    std::atomic<bool> stop_io{false};
    std::thread io_thread{[&loop, &stop_io] {
        while (!stop_io.load(std::memory_order_acquire)) {
            loop.run_once(10ms);
        }
    }};

    std::thread server_thread{[&] {
        try {
            sync_wait(sched, run_server(sched, loop, acceptor, shutdown));
        } catch (...) {
        }
        server_done.store(true, std::memory_order_release);
    }};

    std::this_thread::sleep_for(20ms);
    REQUIRE(sync_wait(sched, one_client(sched, loop, ep)) == "bye");

    shutdown.cancel();
    auto const deadline = std::chrono::steady_clock::now() + 2s;
    while (!server_done.load(std::memory_order_acquire)) {
        REQUIRE(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(5ms);
    }

    stop_io.store(true);
    loop.stop();
    io_thread.join();
    server_thread.join();
    acceptor.close(loop);
    pool.shutdown();
}

#else

TEST_CASE("tcp_echo_server_loop: cancel завершает сервер") { REQUIRE(true); }

#endif
