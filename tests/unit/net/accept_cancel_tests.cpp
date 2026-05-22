#include <netlib/net/coro.hpp>

#include "test_context.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>

using namespace netlib::fakes;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::net::accept_async;
using rrmode::netlib::net::cancellation_source;

namespace {

task<void> pending_accept_cancelled(test_context& ctx, cancellation_source& source,
                                    std::string& err) {
    rrmode::netlib::net::tcp_acceptor acc{ctx.sockets};
    acc.open({.host = "127.0.0.1", .port = 0});

    auto op = accept_async(acc, ctx.loop, &source.token());
    source.cancel();
    try {
        (void)co_await op;
    } catch (rrmode::netlib::net::net_error const& e) {
        err = e.what();
    }
}

}  // namespace

TEST_CASE("cancellation_token отменяет accept_async") {
    test_context ctx;
    cancellation_source source;
    std::string err;

    sync_wait(ctx.sched, pending_accept_cancelled(ctx, source, err));

    REQUIRE(err.find("отменена") != std::string::npos);
}
