#include <netlib/net/unix_stream_acceptor.hpp>
#include <netlib/net/unix_stream_socket.hpp>

#include "test_context.hpp"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

using namespace netlib::fakes;

TEST_CASE("unix_stream_acceptor open сохраняет путь") {
    test_context ctx;
    rrmode::netlib::net::unix_stream_acceptor acc{ctx.sockets};
    acc.open({.path = "/tmp/netlib-fake.sock"});
    REQUIRE(acc.is_open());
    REQUIRE(acc.local_endpoint().path == "/tmp/netlib-fake.sock");
}

TEST_CASE("unix async_connect и async_accept обменивают данные") {
    test_context ctx;
    rrmode::netlib::net::unix_stream_acceptor acc{ctx.sockets};
    acc.open({.path = "/tmp/netlib-fake-echo.sock"});

    std::atomic<bool> connected{false};
    std::atomic<bool> accepted{false};
    std::string err;

    rrmode::netlib::net::unix_stream_socket client{ctx.sockets};
    client.async_connect(
        ctx.loop, {.path = "/tmp/netlib-fake-echo.sock"},
        [&connected] { connected.store(true, std::memory_order_release); },
        [&](rrmode::netlib::net::net_error const& e) { err = e.what(); });

    acc.async_accept(
        ctx.loop,
        [&](rrmode::netlib::net::unix_stream_socket server) {
            accepted.store(true, std::memory_order_release);
            server.async_write_all(
                ctx.loop, std::vector<char>{'h', 'i'},
                [] {},
                [&](rrmode::netlib::net::net_error const& e) { err = e.what(); });
        },
        [&](rrmode::netlib::net::net_error const& e) { err = e.what(); });

    ctx.flush();
    REQUIRE(err.empty());
    REQUIRE(connected.load());
    REQUIRE(accepted.load());

    char buf[8]{};
    std::atomic<bool> got_data{false};
    client.async_read_some(
        ctx.loop, buf,
        [&](std::size_t n) {
            REQUIRE(n == 2);
            REQUIRE(buf[0] == 'h');
            REQUIRE(buf[1] == 'i');
            got_data.store(true, std::memory_order_release);
        },
        [] {},
        [&](rrmode::netlib::net::net_error const& e) { err = e.what(); });

    ctx.flush();
    REQUIRE(err.empty());
    REQUIRE(got_data.load());
}
