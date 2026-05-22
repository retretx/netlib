#include <netlib/net/simple/io_runtime.hpp>
#include <netlib/net/simple/tcp_connection.hpp>
#include <netlib/net/simple/write_stream.hpp>

#include "test_context.hpp"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <thread>
#include <vector>

using namespace netlib::fakes;

TEST_CASE("simple sync connect на фейке immediate") {
    test_context ctx;
    rrmode::netlib::net::simple::io_runtime runtime{ctx.pool, ctx.sched, ctx.loop, ctx.sockets};
    rrmode::netlib::net::simple::tcp_connection conn{runtime};

    conn.connect({.host = "immediate", .port = 1});
    ctx.flush();

    REQUIRE(conn.is_open());
}

TEST_CASE("simple sync connect после writable") {
    test_context ctx;
    rrmode::netlib::net::simple::io_runtime runtime{ctx.pool, ctx.sched, ctx.loop, ctx.sockets};
    rrmode::netlib::net::simple::tcp_connection conn{runtime};

    std::atomic<bool> started{false};
    std::thread t{[&] {
        started.store(true);
        try {
            conn.connect({.host = "127.0.0.1", .port = 1});
        } catch (...) {
        }
    }};

    while (!started.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    ctx.sockets.complete_connect(conn.socket().native_handle());
    ctx.flush();
    t.join();

    REQUIRE(conn.is_open());
}

TEST_CASE("simple sync write_all и read_some") {
    test_context ctx;
    int const a = ctx.sockets.create_tcp_socket();
    int const b = ctx.sockets.create_tcp_socket();
    ctx.sockets.link_peers(a, b);

    rrmode::netlib::net::simple::io_runtime runtime{ctx.pool, ctx.sched, ctx.loop, ctx.sockets};
    rrmode::netlib::net::simple::tcp_connection conn{
        runtime, rrmode::netlib::net::tcp_socket{b, ctx.sockets}};

    ctx.sockets.push_rx(b, "ping");

    char buf[8]{};
    auto const n = conn.read_some(std::span<char>{buf, 4});
    ctx.flush();

    REQUIRE(n == 4);
    REQUIRE(std::string_view{buf, 4} == "ping");
}

TEST_CASE("write_stream накапливает и flush отправляет") {
    test_context ctx;
    int const a = ctx.sockets.create_tcp_socket();
    int const b = ctx.sockets.create_tcp_socket();
    ctx.sockets.link_peers(a, b);

    rrmode::netlib::net::simple::io_runtime runtime{ctx.pool, ctx.sched, ctx.loop, ctx.sockets};
    rrmode::netlib::net::simple::tcp_connection conn{
        runtime, rrmode::netlib::net::tcp_socket{b, ctx.sockets}};

    {
        auto ws = conn.write();
        ws << 'a' << 'b' << 'c';
        ws.flush();
    }
    ctx.flush();

    REQUIRE(ctx.sockets.state(a).rx.size() >= 3);
}
