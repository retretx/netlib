#include <netlib/net/simple/coro.hpp>

#include "test_context.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace netlib::fakes;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::net::simple::connect_async;
using rrmode::netlib::net::simple::io_runtime;
using rrmode::netlib::net::simple::tcp_connection;

namespace {

task<bool> connect_task(io_runtime& runtime) {
    tcp_connection conn{runtime};
    co_await connect_async(conn, {.host = "immediate", .port = 1});
    co_return conn.is_open();
}

}  // namespace

TEST_CASE("simple connect_async на фейке") {
    test_context ctx;
    io_runtime runtime{ctx.pool, ctx.sched, ctx.loop, ctx.sockets};
    bool const open = sync_wait(ctx.sched, connect_task(runtime));
    ctx.flush();
    REQUIRE(open);
}
