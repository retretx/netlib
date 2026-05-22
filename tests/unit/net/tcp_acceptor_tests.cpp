#include <netlib/net/tcp_acceptor.hpp>
#include <netlib/net/tcp_socket.hpp>

#include "test_context.hpp"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <string>

using namespace netlib::fakes;

TEST_CASE("tcp_acceptor open назначает ephemeral порт") {
    test_context ctx;
    rrmode::netlib::net::tcp_acceptor acc{ctx.sockets};
    acc.open({.host = "127.0.0.1", .port = 0});
    REQUIRE(acc.is_open());
    REQUIRE(acc.local_endpoint().port >= 20000);
}

TEST_CASE("async_accept принимает сокет из accept_queue") {
    test_context ctx;
    rrmode::netlib::net::tcp_acceptor acc{ctx.sockets};
    acc.open({.host = "127.0.0.1", .port = 0});

    int const client_fd = ctx.sockets.create_tcp_socket();
    ctx.sockets.enqueue_accept(acc.native_handle(), client_fd);

    std::atomic<bool> accepted{false};
    std::string err;

    acc.async_accept(
        ctx.loop,
        [&accepted](rrmode::netlib::net::tcp_socket sock) {
            REQUIRE(sock.is_open());
            accepted.store(true, std::memory_order_release);
        },
        [&](rrmode::netlib::net::net_error const& e) { err = e.what(); });

    ctx.flush();
    REQUIRE(err.empty());
    REQUIRE(accepted.load());
}

TEST_CASE("async_accept ждёт readable если очередь пуста") {
    test_context ctx;
    rrmode::netlib::net::tcp_acceptor acc{ctx.sockets};
    acc.open({.host = "127.0.0.1", .port = 0});

    std::atomic<bool> accepted{false};
    std::string err;

    acc.async_accept(
        ctx.loop,
        [&accepted](rrmode::netlib::net::tcp_socket) {
            accepted.store(true, std::memory_order_release);
        },
        [&](rrmode::netlib::net::net_error const& e) { err = e.what(); });

    REQUIRE_FALSE(accepted.load());

    int const client_fd = ctx.sockets.create_tcp_socket();
    ctx.sockets.enqueue_accept(acc.native_handle(), client_fd);
    ctx.reactor().arm(acc.native_handle(), rrmode::netlib::net::detail::poll_event::readable);

    ctx.flush();
    REQUIRE(err.empty());
    REQUIRE(accepted.load());
}

TEST_CASE("async_accept сообщает ошибку если acceptor закрыт") {
    test_context ctx;
    rrmode::netlib::net::tcp_acceptor acc{ctx.sockets};
    std::string err;

    acc.async_accept(
        ctx.loop, [](rrmode::netlib::net::tcp_socket) {},
        [&](rrmode::netlib::net::net_error const& e) { err = e.what(); });

    ctx.flush();
    REQUIRE(err == "async_accept: acceptor не открыт");
}
