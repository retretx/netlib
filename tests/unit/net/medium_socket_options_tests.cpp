#include <netlib/net/medium/io_context.hpp>
#include <netlib/net/medium/socket_options.hpp>
#include <netlib/net/medium/tcp_acceptor.hpp>
#include <netlib/net/medium/tcp_socket.hpp>

#include "test_context.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace netlib::fakes;

TEST_CASE("medium tcp_socket применяет tcp_nodelay на фейке") {
    test_context ctx;
    rrmode::netlib::net::medium::io_context io_ctx{ctx.pool, ctx.sched, ctx.loop, ctx.sockets};

    rrmode::netlib::net::medium::socket_options opts;
    opts.tcp_nodelay = true;

    rrmode::netlib::net::medium::tcp_socket sock{io_ctx, opts};
    std::atomic<bool> done{false};

    sock.async_connect(
        {.host = "immediate", .port = 1}, [&done]() { done.store(true); },
        [](rrmode::netlib::net::net_error const&) {});

    ctx.flush();
    REQUIRE(done.load());

    int const fd = sock.native_handle();
    REQUIRE(ctx.sockets.options(fd).tcp_nodelay.value_or(false));
}

TEST_CASE("medium tcp_acceptor open применяет reuseaddr") {
    test_context ctx;
    rrmode::netlib::net::medium::io_context io_ctx{ctx.pool, ctx.sched, ctx.loop, ctx.sockets};

    rrmode::netlib::net::medium::socket_options opts;
    opts.reuseaddr = true;

    rrmode::netlib::net::medium::tcp_acceptor acc{io_ctx, opts};
    acc.open({.host = "127.0.0.1", .port = 0});

    REQUIRE(acc.is_open());
    REQUIRE(ctx.sockets.options(acc.native_handle()).reuseaddr);
}
