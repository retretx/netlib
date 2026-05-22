#include <netlib/net/tcp_socket.hpp>

#include "test_context.hpp"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cerrno>
#include <string>

using namespace netlib::fakes;

TEST_CASE("async_connect завершается сразу если start_connect успешен") {
    test_context ctx;
    rrmode::netlib::net::tcp_socket sock{ctx.sockets};
    std::atomic<bool> connected{false};
    std::string err;

    sock.async_connect(
        ctx.loop, {.host = "immediate", .port = 1},
        [&connected]() { connected.store(true, std::memory_order_release); },
        [&](rrmode::netlib::net::net_error const& e) { err = e.what(); });

    ctx.flush();
    REQUIRE(err.empty());
    REQUIRE(connected.load());
}

TEST_CASE("async_connect завершается после writable и SO_ERROR=0") {
    test_context ctx;
    rrmode::netlib::net::tcp_socket sock{ctx.sockets};
    std::atomic<bool> connected{false};
    std::string err;

    sock.async_connect(
        ctx.loop, {.host = "127.0.0.1", .port = 1},
        [&connected]() { connected.store(true, std::memory_order_release); },
        [&](rrmode::netlib::net::net_error const& e) { err = e.what(); });

    int const fd = sock.native_handle();
    ctx.sockets.complete_connect(fd);
    ctx.flush();

    REQUIRE(err.empty());
    REQUIRE(connected.load());
}

TEST_CASE("async_connect вызывает on_error при SO_ERROR") {
    test_context ctx;
    rrmode::netlib::net::tcp_socket sock{ctx.sockets};
    std::atomic<bool> connected{false};
    std::string err;

    sock.async_connect(
        ctx.loop, {.host = "127.0.0.1", .port = 1},
        [&connected]() { connected.store(true, std::memory_order_release); },
        [&](rrmode::netlib::net::net_error const& e) { err = e.what(); });

    int const fd = sock.native_handle();
    ctx.sockets.set_socket_error(fd, ECONNREFUSED);
    ctx.sockets.signal_connect_complete(fd);
    ctx.flush();

    REQUIRE_FALSE(connected.load());
    REQUIRE_FALSE(err.empty());
}

TEST_CASE("async_connect требует оба колбэка") {
    test_context ctx;
    rrmode::netlib::net::tcp_socket sock{ctx.sockets};
    REQUIRE_THROWS_AS(sock.async_connect(ctx.loop, {}, nullptr, nullptr),
                      rrmode::netlib::net::net_error);
}
