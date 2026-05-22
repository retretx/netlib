#include <netlib/net/tcp_socket.hpp>

#include "test_context.hpp"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <string>

using namespace netlib::fakes;

TEST_CASE("async_write_all отправляет все байты peer") {
    test_context ctx;
    int const a = ctx.sockets.create_tcp_socket();
    int const b = ctx.sockets.create_tcp_socket();
    ctx.sockets.link_peers(a, b);

    rrmode::netlib::net::tcp_socket writer{b, ctx.sockets};
    std::atomic<bool> done{false};
    std::string err;

    writer.async_write_all(
        ctx.loop, std::vector<char>{'d', 'a', 't', 'a'},
        [&done]() { done.store(true, std::memory_order_release); },
        [&](rrmode::netlib::net::net_error const& e) { err = e.what(); });

    ctx.flush();
    REQUIRE(err.empty());
    REQUIRE(done.load());
    REQUIRE(ctx.sockets.state(a).rx.size() == 4);
}

TEST_CASE("async_write_all сообщает ошибку если сокет закрыт") {
    test_context ctx;
    rrmode::netlib::net::tcp_socket sock{ctx.sockets};
    std::string err;

    sock.async_write_all(
        ctx.loop, std::vector<char>{1}, []() {},
        [&](rrmode::netlib::net::net_error const& e) { err = e.what(); });

    ctx.flush();
    REQUIRE(err == "async_write_all: сокет закрыт");
}
