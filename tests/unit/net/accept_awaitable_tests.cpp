#include <netlib/net/coro.hpp>

#include "test_context.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace netlib::fakes;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::net::accept_async;

namespace {

task<void> accept_one(test_context& ctx) {
    rrmode::netlib::net::tcp_acceptor acc{ctx.sockets};
    acc.open({.host = "127.0.0.1", .port = 0});

    int const client_fd = ctx.sockets.create_tcp_socket();
    ctx.sockets.enqueue_accept(acc.native_handle(), client_fd);

    auto peer = co_await accept_async(acc, ctx.loop);
    REQUIRE(peer.is_open());
}

}  // namespace

TEST_CASE("accept_awaitable принимает соединение") {
    test_context ctx;
    sync_wait(ctx.sched, accept_one(ctx));
    ctx.flush();
}
