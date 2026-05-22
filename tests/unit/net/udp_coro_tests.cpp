#include <netlib/net/coro.hpp>

#include "test_context.hpp"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <string>
#include <thread>

using namespace netlib::fakes;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::net::udp_echo_once;
using rrmode::netlib::net::udp_recv_string;
using rrmode::netlib::net::udp_send_string;

namespace {

task<std::string> echo_roundtrip(test_context& ctx) {
    rrmode::netlib::net::udp_socket server{ctx.sockets};
    rrmode::netlib::net::udp_socket client{ctx.sockets};
    server.bind({.host = "127.0.0.1", .port = 0});
    client.bind({.host = "127.0.0.1", .port = 0});

    ctx.sockets.link_udp(server.native_handle(), client.native_handle());

    auto const server_ep = server.local_endpoint();

    co_await udp_send_string(client, ctx.loop, "hi", server_ep);
    co_await udp_echo_once(server, ctx.loop, 64);
    co_return co_await udp_recv_string(client, ctx.loop, 64);
}

}  // namespace

TEST_CASE("udp coro: send_to / recv_from / echo_once на фейке") {
    test_context ctx;
    REQUIRE(sync_wait(ctx.sched, echo_roundtrip(ctx)) == "hi");
}

namespace {

task<void> loop_until_cancel(test_context& ctx, rrmode::netlib::net::cancellation_source& stop) {
    rrmode::netlib::net::udp_socket sock{ctx.sockets};
    sock.bind({.host = "127.0.0.1", .port = 0});
    co_await rrmode::netlib::net::udp_echo_loop(sock, ctx.loop, &stop.token());
}

}  // namespace

TEST_CASE("udp_echo_loop: выходит после cancel") {
    test_context ctx;
    rrmode::netlib::net::cancellation_source stop;

    std::atomic<bool> done{false};
    std::thread worker{[&] {
        try {
            sync_wait(ctx.sched, loop_until_cancel(ctx, stop));
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
