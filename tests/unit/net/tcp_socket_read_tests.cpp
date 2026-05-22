#include <netlib/net/tcp_socket.hpp>

#include "test_context.hpp"

#include <array>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <string>

using namespace netlib::fakes;

namespace {

void read_into(test_context& ctx, rrmode::netlib::net::tcp_socket& sock, std::string& out,
              std::atomic<bool>& done, std::string& err) {
    auto buf = std::make_shared<std::array<char, 32>>();
    sock.async_read_some(
        ctx.loop, std::span<char>{buf->data(), buf->size()},
        [&out, buf, &done](std::size_t n) {
            out.assign(buf->data(), n);
            done.store(true, std::memory_order_release);
        },
        [&err]() { err = "eof"; },
        [&err](rrmode::netlib::net::net_error const& e) { err = e.what(); });
}

}  // namespace

TEST_CASE("async_read_some возвращает данные если они уже в буфере") {
    test_context ctx;
    int const a = ctx.sockets.create_tcp_socket();
    int const b = ctx.sockets.create_tcp_socket();
    ctx.sockets.link_peers(a, b);
    ctx.sockets.push_rx(a, "hello");

    rrmode::netlib::net::tcp_socket sock{a, ctx.sockets};
    std::string got;
    std::atomic<bool> done{false};
    std::string err;

    read_into(ctx, sock, got, done, err);
    ctx.flush();

    REQUIRE(err.empty());
    REQUIRE(done.load());
    REQUIRE(got == "hello");
}

TEST_CASE("async_read_some читает после writable peer и trigger readable") {
    test_context ctx;
    int const a = ctx.sockets.create_tcp_socket();
    int const b = ctx.sockets.create_tcp_socket();
    ctx.sockets.link_peers(a, b);

    rrmode::netlib::net::tcp_socket reader{a, ctx.sockets};
    rrmode::netlib::net::tcp_socket writer{b, ctx.sockets};

    std::string got;
    std::atomic<bool> done{false};
    std::string err;

    read_into(ctx, reader, got, done, err);

    writer.async_write_all(
        ctx.loop, std::vector<char>{'x', 'y', 'z'}, []() {},
        [&](rrmode::netlib::net::net_error const& e) { err = e.what(); });

    ctx.flush();

    REQUIRE(err.empty());
    REQUIRE(done.load());
    REQUIRE(got == "xyz");
}

TEST_CASE("async_read_some читает если peer отправил до register") {
    test_context ctx;
    int const a = ctx.sockets.create_tcp_socket();
    int const b = ctx.sockets.create_tcp_socket();
    ctx.sockets.link_peers(a, b);

    rrmode::netlib::net::tcp_socket writer{b, ctx.sockets};
    writer.async_write_all(
        ctx.loop, std::vector<char>{'h', 'i'}, []() {},
        [](rrmode::netlib::net::net_error const&) {});

    ctx.flush();

    rrmode::netlib::net::tcp_socket reader{a, ctx.sockets};
    std::string got;
    std::atomic<bool> done{false};
    std::string err;

    read_into(ctx, reader, got, done, err);
    ctx.flush();

    REQUIRE(err.empty());
    REQUIRE(done.load());
    REQUIRE(got == "hi");
}

TEST_CASE("async_read_some вызывает on_eof при нулевом чтении") {
    test_context ctx;
    int const fd = ctx.sockets.create_tcp_socket();
    rrmode::netlib::net::tcp_socket sock{fd, ctx.sockets};

    std::atomic<bool> eof{false};
    std::string err;
    std::array<char, 8> buf{};

    sock.async_read_some(
        ctx.loop, buf, [](std::size_t) {},
        [&eof]() { eof.store(true, std::memory_order_release); },
        [&](rrmode::netlib::net::net_error const& e) { err = e.what(); });

    ctx.sockets.set_rx_eof(fd);
    ctx.sockets.trigger_readable(fd);
    ctx.flush();

    REQUIRE(err.empty());
    REQUIRE(eof.load());
}

TEST_CASE("async_read_some сообщает ошибку если сокет не открыт") {
    test_context ctx;
    rrmode::netlib::net::tcp_socket sock{ctx.sockets};
    std::string err;
    std::array<char, 4> buf{};

    sock.async_read_some(
        ctx.loop, buf, [](std::size_t) {}, []() {},
        [&](rrmode::netlib::net::net_error const& e) { err = e.what(); });

    ctx.flush();
    REQUIRE(err == "async_read_some: сокет закрыт");
}

TEST_CASE("async_read_some завершается после уничтожения tcp_socket") {
    test_context ctx;
    int const a = ctx.sockets.create_tcp_socket();
    int const b = ctx.sockets.create_tcp_socket();
    ctx.sockets.link_peers(a, b);

    std::string got;
    std::atomic<bool> done{false};
    std::string err;

    {
        rrmode::netlib::net::tcp_socket server{a, ctx.sockets};
        read_into(ctx, server, got, done, err);
        ctx.flush();
        REQUIRE_FALSE(done.load());
    }

    ctx.sockets.push_rx(a, "ok");
    ctx.sockets.trigger_readable(a);
    ctx.flush();

    REQUIRE(err.empty());
    REQUIRE(done.load());
    REQUIRE(got == "ok");
}

TEST_CASE("async_read_some требует живой сокет после register") {
    test_context ctx;
    int const a = ctx.sockets.create_tcp_socket();
    int const b = ctx.sockets.create_tcp_socket();
    ctx.sockets.link_peers(a, b);

    auto server = std::make_unique<rrmode::netlib::net::tcp_socket>(a, ctx.sockets);
    std::string got;
    std::atomic<bool> done{false};
    std::string err;

    read_into(ctx, *server, got, done, err);
    REQUIRE_FALSE(done.load());

    {
        rrmode::netlib::net::tcp_socket peer{b, ctx.sockets};
        peer.async_write_all(
            ctx.loop, std::vector<char>{'z'}, []() {},
            [](rrmode::netlib::net::net_error const&) {});
        ctx.flush();
    }

    ctx.sockets.push_rx(a, "z");
    ctx.sockets.trigger_readable(a);
    ctx.flush();

    REQUIRE(err.empty());
    REQUIRE(done.load());
    REQUIRE(got == "z");
}

TEST_CASE("async_read_some повторно ждёт readable при spurious EAGAIN") {
    test_context ctx;
    int const a = ctx.sockets.create_tcp_socket();
    int const b = ctx.sockets.create_tcp_socket();
    ctx.sockets.link_peers(a, b);

    rrmode::netlib::net::tcp_socket sock{a, ctx.sockets};
    std::string got;
    std::atomic<bool> done{false};
    std::string err;

    read_into(ctx, sock, got, done, err);
    ctx.sockets.trigger_readable(a);
    ctx.flush();
    REQUIRE_FALSE(done.load());

    ctx.sockets.push_rx(a, "ok");
    ctx.sockets.trigger_readable(a);
    ctx.flush();

    REQUIRE(err.empty());
    REQUIRE(done.load());
    REQUIRE(got == "ok");
}
