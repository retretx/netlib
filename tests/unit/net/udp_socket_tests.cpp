#include <netlib/net/udp_socket.hpp>

#include "test_context.hpp"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <string>
#include <vector>

using namespace netlib::fakes;

namespace {

TEST_CASE("UDP fake: sendto/recvfrom через link_udp") {
    test_context ctx;
    int const a = ctx.sockets.create_udp_socket();
    int const b = ctx.sockets.create_udp_socket();
    ctx.sockets.bind_endpoint(a, {.host = "127.0.0.1", .port = 0});
    ctx.sockets.bind_endpoint(b, {.host = "127.0.0.1", .port = 0});
    ctx.sockets.link_udp(a, b);

    auto const src =
        rrmode::netlib::net::endpoint{.host = "127.0.0.1", .port = ctx.sockets.get_local_port(a)};
    std::vector<char> payload{'u', 'd', 'p'};
    auto const sent = ctx.sockets.try_sendto(a, payload, {.host = "127.0.0.1", .port = 9999});
    REQUIRE(sent == payload.size());

    std::array<char, 16> buf{};
    rrmode::netlib::net::endpoint remote{};
    auto const n = ctx.sockets.try_recvfrom(b, buf, remote);
    REQUIRE(n == 3);
    REQUIRE(remote.port == src.port);
    REQUIRE(std::string{buf.data(), buf.data() + 3} == "udp");
}

TEST_CASE("udp_socket async_send_to / async_recv_from") {
    test_context ctx;
    rrmode::netlib::net::udp_socket sender{ctx.sockets};
    rrmode::netlib::net::udp_socket receiver{ctx.sockets};
    sender.bind({.host = "127.0.0.1", .port = 0});
    receiver.bind({.host = "127.0.0.1", .port = 0});

    ctx.sockets.link_udp(sender.native_handle(), receiver.native_handle());

    std::atomic<bool> sent{false};
    std::atomic<bool> received{false};
    std::string msg;
    std::array<char, 32> buf{};

    receiver.async_recv_from(
        ctx.loop, buf,
        [&](rrmode::netlib::net::endpoint, std::size_t n) {
            msg.assign(buf.data(), buf.data() + static_cast<std::ptrdiff_t>(n));
            received.store(true);
        },
        [](rrmode::netlib::net::net_error const&) {});

    sender.async_send_to(ctx.loop, std::vector<char>{'p', 'i', 'n', 'g'},
                         {.host = "127.0.0.1", .port = 1}, [&] { sent.store(true); },
                         [](rrmode::netlib::net::net_error const&) {});

    ctx.flush();
    REQUIRE(sent.load());
    REQUIRE(received.load());
    REQUIRE(msg == "ping");
}

TEST_CASE("udp_socket bind port 0 назначает порт") {
    test_context ctx;
    rrmode::netlib::net::udp_socket sock{ctx.sockets};
    sock.bind({.host = "127.0.0.1", .port = 0});
    REQUIRE(sock.local_endpoint().port > 0);
}

}  // namespace
