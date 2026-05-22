#include <netlib/net/tcp_socket.hpp>

#include "test_context.hpp"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <string>

using namespace netlib::fakes;

TEST_CASE("io_handle удерживает fd после уничтожения tcp_socket") {
    test_context ctx;
    int const a = ctx.sockets.create_tcp_socket();
    int const b = ctx.sockets.create_tcp_socket();
    ctx.sockets.link_peers(a, b);

    std::shared_ptr<rrmode::netlib::net::detail::socket_handle> io;
    {
        rrmode::netlib::net::tcp_socket sock{a, ctx.sockets};
        io = sock.io_handle();
        REQUIRE(io->is_open());
    }
    REQUIRE(io->is_open());
    REQUIRE(io->native_handle() == a);
}

TEST_CASE("tcp_socket из io_handle пишет после уничтожения исходного объекта") {
    test_context ctx;
    int const a = ctx.sockets.create_tcp_socket();
    int const b = ctx.sockets.create_tcp_socket();
    ctx.sockets.link_peers(a, b);

    std::shared_ptr<rrmode::netlib::net::detail::socket_handle> io;
    std::atomic<bool> done{false};
    std::string err;

    {
        rrmode::netlib::net::tcp_socket writer{a, ctx.sockets};
        io = writer.io_handle();
        rrmode::netlib::net::tcp_socket{io}.async_write_all(
            ctx.loop, std::vector<char>{'p'}, [&done]() { done.store(true, std::memory_order_release); },
            [&](rrmode::netlib::net::net_error const& e) { err = e.what(); });
        ctx.flush();
    }

    REQUIRE(err.empty());
    REQUIRE(done.load());
    REQUIRE(ctx.sockets.state(b).rx.size() == 1);
}
