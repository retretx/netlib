#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/coro.hpp>

#include <atomic>
#include <chrono>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#if defined(__linux__) || defined(__APPLE__)

using namespace std::chrono_literals;

namespace {

using rrmode::netlib::execution::scheduler;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::execution::thread_pool;
using rrmode::netlib::net::cancellation_source;
using rrmode::netlib::net::read_unix_string_async;
using rrmode::netlib::net::unix_connect;
using rrmode::netlib::net::unix_echo_server_loop;
using rrmode::netlib::net::write_all_unix_async;

std::string unique_path() {
    return std::string{"/tmp/netlib_unix_shutdown_"} +
           std::to_string(static_cast<unsigned>(::getpid())) + ".sock";
}

task<void> run_server(scheduler& sched, rrmode::netlib::net::event_loop& loop,
                      rrmode::netlib::net::unix_stream_acceptor& acceptor,
                      cancellation_source& shutdown) {
    co_await sched;
    co_await unix_echo_server_loop(acceptor, loop, &shutdown.token());
}

task<std::string> one_client(scheduler& sched, rrmode::netlib::net::event_loop& loop,
                             std::string const& path) {
    co_await sched;
    rrmode::netlib::net::unix_stream_socket client;
    co_await unix_connect(client, loop, {.path = path});
    co_await write_all_unix_async(client, loop, std::vector<char>{'b', 'y', 'e'});
    auto msg = co_await read_unix_string_async(client, loop, 16);
    client.close(loop);
    co_return msg;
}

}  // namespace

TEST_CASE("unix_echo_server_loop: cancel завершает сервер") {
    auto const path = unique_path();
    thread_pool pool{4};
    scheduler sched{pool};
    rrmode::netlib::net::event_loop loop{sched};

    rrmode::netlib::net::unix_stream_acceptor acceptor;
    acceptor.open({.path = path});

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
    REQUIRE(sync_wait(sched, one_client(sched, loop, path)) == "bye");

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
    std::remove(path.c_str());
}

#else

TEST_CASE("unix_echo_server_loop: cancel завершает сервер") { REQUIRE(true); }

#endif
