#include <netlib/net/coro.hpp>

#include "test_context.hpp"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <thread>

using namespace netlib::fakes;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::net::cancellation_source;
using rrmode::netlib::net::tcp_connect;
using rrmode::netlib::net::tcp_echo_server_loop;

namespace {

task<void> loop_until_stopped(test_context& ctx, cancellation_source& stop) {
    rrmode::netlib::net::tcp_acceptor acc{ctx.sockets};
    acc.open({.host = "127.0.0.1", .port = 0});
    co_await tcp_echo_server_loop(acc, ctx.loop, &stop.token());
}

}  // namespace

TEST_CASE("tcp_connect immediate на фейке") {
    test_context ctx;
    rrmode::netlib::net::tcp_socket sock{ctx.sockets};
    sync_wait(ctx.sched, tcp_connect(sock, ctx.loop, {.host = "immediate", .port = 1}));
    REQUIRE(sock.is_open());
}

TEST_CASE("tcp_echo_server_loop выходит после cancel") {
    test_context ctx;
    cancellation_source stop;

    std::atomic<bool> done{false};
    std::thread worker{[&] {
        try {
            sync_wait(ctx.sched, loop_until_stopped(ctx, stop));
        } catch (...) {
        }
        done.store(true);
    }};

    std::this_thread::sleep_for(std::chrono::milliseconds{20});
    stop.cancel();
    ctx.flush();

    worker.join();
    REQUIRE(done.load());
}
