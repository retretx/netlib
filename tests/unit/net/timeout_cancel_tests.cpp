#include <netlib/net/coro.hpp>

#include "test_context.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>

using namespace netlib::fakes;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::net::cancellation_source;
using rrmode::netlib::net::connect_with_timeout;
using rrmode::netlib::net::timeout_error;

namespace {

task<void> connect_timeout_sets_cancel(test_context& ctx, cancellation_source& source) {
    int const fd = ctx.sockets.create_tcp_socket();
    ctx.sockets.set_connect_pending(fd);
    rrmode::netlib::net::tcp_socket sock{fd, ctx.sockets};
    co_await connect_with_timeout(sock, ctx.loop, {.host = "pending", .port = 1}, ctx.sched,
                                std::chrono::milliseconds{25}, &source);
}

}  // namespace

TEST_CASE("connect_with_timeout: по таймауту отменяет cancellation_source") {
    test_context ctx;
    cancellation_source source;

    REQUIRE_THROWS_AS(sync_wait(ctx.sched, connect_timeout_sets_cancel(ctx, source)),
                      timeout_error);
    REQUIRE(source.token().is_cancelled());
}
