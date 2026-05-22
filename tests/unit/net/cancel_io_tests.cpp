#include <netlib/net/tcp_socket.hpp>

#include "test_context.hpp"

#include <array>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <string>

using namespace netlib::fakes;

namespace {

void read_with_cancel(test_context& ctx, std::atomic<bool>& done, std::string& err) {
    int const a = ctx.sockets.create_tcp_socket();
    int const b = ctx.sockets.create_tcp_socket();
    ctx.sockets.link_peers(a, b);

    rrmode::netlib::net::tcp_socket reader{a, ctx.sockets};

    auto buf = std::make_shared<std::array<char, 16>>();
    reader.async_read_some(
        ctx.loop, std::span<char>{buf->data(), buf->size()},
        [&done](std::size_t) { done.store(true); },
        []() {},
        [&err](rrmode::netlib::net::net_error const& e) { err = e.what(); });

    reader.cancel_io(ctx.loop);
}

}  // namespace

TEST_CASE("cancel_io завершает pending read с ошибкой отмены") {
    test_context ctx;
    std::atomic<bool> done{false};
    std::string err;

    read_with_cancel(ctx, done, err);

    REQUIRE_FALSE(done.load());
    REQUIRE(err.find("отменена") != std::string::npos);
}
