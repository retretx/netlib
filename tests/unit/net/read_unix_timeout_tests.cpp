#include <netlib/net/coro.hpp>

#include "test_context.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>

using namespace netlib::fakes;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::net::read_unix_string_with_timeout;
using rrmode::netlib::net::timeout_error;

namespace {

task<void> read_never_completes(test_context& ctx) {
    int const a = ctx.sockets.create_unix_stream_socket();
    int const b = ctx.sockets.create_unix_stream_socket();
    ctx.sockets.link_peers(a, b);
    rrmode::netlib::net::unix_stream_socket sock{a, ctx.sockets};
    (void)co_await read_unix_string_with_timeout(sock, ctx.loop, ctx.sched, std::chrono::milliseconds{30});
}

}  // namespace

TEST_CASE("read_unix_string_with_timeout: пустой сокет → timeout_error") {
    test_context ctx;
    REQUIRE_THROWS_AS(sync_wait(ctx.sched, read_never_completes(ctx)), timeout_error);
    ctx.flush();
}
