#include <netlib/net/coro.hpp>

#include "test_context.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>

using namespace netlib::fakes;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::net::connect_async;

namespace {

task<bool> connect_immediate(test_context& ctx) {
    rrmode::netlib::net::tcp_socket sock{ctx.sockets};
    co_await connect_async(sock, ctx.loop, {.host = "immediate", .port = 1});
    co_return sock.is_open();
}

}  // namespace

TEST_CASE("connect_awaitable подключает с immediate backend") {
    test_context ctx;
    bool const open = sync_wait(ctx.sched, connect_immediate(ctx));
    ctx.flush();
    REQUIRE(open);
}
