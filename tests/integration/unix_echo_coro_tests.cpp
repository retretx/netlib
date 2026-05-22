#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/thread_pool.hpp>
#include <netlib/execution/when_all.hpp>
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
using rrmode::netlib::execution::when_all;
using rrmode::netlib::net::connect_unix_async;
using rrmode::netlib::net::read_unix_string_async;
using rrmode::netlib::net::unix_serve_echo_once;
using rrmode::netlib::net::write_all_unix_async;

std::string unique_path() {
    return std::string{"/tmp/netlib_unix_coro_"} + std::to_string(static_cast<unsigned>(::getpid())) +
           ".sock";
}

task<std::string> client_echo(scheduler& sched, rrmode::netlib::net::event_loop& loop,
                              std::string const& path) {
    co_await sched;
    rrmode::netlib::net::unix_stream_socket client;
    co_await connect_unix_async(client, loop, {.path = path});
    co_await sched;
    co_await write_all_unix_async(client, loop, std::vector<char>{'u', 'n', 'i', 'x'});
    co_await sched;
    auto msg = co_await read_unix_string_async(client, loop, 64);
    client.close(loop);
    co_return msg;
}

}  // namespace

TEST_CASE("unix echo loopback coroutines") {
    auto const path = unique_path();
    thread_pool pool{4};
    scheduler sched{pool};
    rrmode::netlib::net::event_loop loop{sched};

    rrmode::netlib::net::unix_stream_acceptor acceptor;
    acceptor.open({.path = path});

    std::atomic<bool> stop_io{false};
    std::thread io_thread{[&loop, &stop_io] {
        while (!stop_io.load(std::memory_order_acquire)) {
            loop.run_once(10ms);
        }
    }};

    auto const response = sync_wait(
        sched, when_all(sched, unix_serve_echo_once(acceptor, loop), client_echo(sched, loop, path)));

    stop_io.store(true);
    loop.stop();
    io_thread.join();
    acceptor.close(loop);
    pool.shutdown();
    std::remove(path.c_str());

    REQUIRE(response == "unix");
}

#else

TEST_CASE("unix echo loopback coroutines") { REQUIRE(true); }

#endif
