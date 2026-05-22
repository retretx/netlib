#include <netlib/net/coro.hpp>

#include "test_context.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>

using namespace netlib::fakes;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::net::accept_with_timeout;
using rrmode::netlib::net::timeout_error;

namespace {

task<void> accept_never_completes(test_context& ctx) {
    rrmode::netlib::net::tcp_acceptor acc{ctx.sockets};
    acc.open({.host = "127.0.0.1", .port = 0});
    (void)co_await accept_with_timeout(acc, ctx.loop, ctx.sched, std::chrono::milliseconds{30});
}

}  // namespace

TEST_CASE("accept_with_timeout: pending accept → timeout_error") {
    test_context ctx;
    REQUIRE_THROWS_AS(sync_wait(ctx.sched, accept_never_completes(ctx)), timeout_error);
    ctx.flush();
}
