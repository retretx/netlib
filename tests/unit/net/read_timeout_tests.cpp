#include <netlib/net/coro.hpp>

#include "test_context.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <vector>

using namespace netlib::fakes;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::net::read_string_with_timeout;
using rrmode::netlib::net::timeout_error;

namespace {

task<void> read_never_completes(test_context& ctx) {
    int const fd = ctx.sockets.create_tcp_socket();
    ctx.sockets.set_connect_pending(fd);
    rrmode::netlib::net::tcp_socket sock{fd, ctx.sockets};
    co_await connect_async(sock, ctx.loop, {.host = "immediate", .port = 1});
    (void)co_await read_string_with_timeout(sock, ctx.loop, ctx.sched, std::chrono::milliseconds{30});
}

}  // namespace

TEST_CASE("read_string_with_timeout: пустой сокет → timeout_error") {
    test_context ctx;
    REQUIRE_THROWS_AS(sync_wait(ctx.sched, read_never_completes(ctx)), timeout_error);
    ctx.flush();
}
