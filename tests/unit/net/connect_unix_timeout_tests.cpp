#include <netlib/net/coro.hpp>

#include "test_context.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>

using namespace netlib::fakes;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::net::connect_unix_with_timeout;
using rrmode::netlib::net::timeout_error;

namespace {

task<void> connect_never_finishes(test_context& ctx) {
    int const fd = ctx.sockets.create_unix_stream_socket();
    ctx.sockets.set_connect_pending(fd);
    rrmode::netlib::net::unix_stream_socket sock{fd, ctx.sockets};
    co_await connect_unix_with_timeout(sock, ctx.loop, {.path = "pending"}, ctx.sched,
                                       std::chrono::milliseconds{30});
}

}  // namespace

TEST_CASE("connect_unix_with_timeout: pending → timeout_error") {
    test_context ctx;
    REQUIRE_THROWS_AS(sync_wait(ctx.sched, connect_never_finishes(ctx)), timeout_error);
    ctx.flush();
}
