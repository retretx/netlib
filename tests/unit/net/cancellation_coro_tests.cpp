#include <netlib/net/coro.hpp>

#include "test_context.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <string>

using namespace netlib::fakes;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::net::cancellation_source;
using rrmode::netlib::net::read_some_async;

namespace {

task<void> pending_read_cancelled(test_context& ctx, cancellation_source& source, std::string& err) {
    int const a = ctx.sockets.create_tcp_socket();
    int const b = ctx.sockets.create_tcp_socket();
    ctx.sockets.link_peers(a, b);

    rrmode::netlib::net::tcp_socket reader{a, ctx.sockets};
    auto buf = std::make_shared<std::array<char, 16>>();

    auto op = read_some_async(reader, ctx.loop, std::span<char>{buf->data(), buf->size()},
                              &source.token());
    source.cancel();
    try {
        co_await op;
    } catch (rrmode::netlib::net::net_error const& e) {
        err = e.what();
    }
}

}  // namespace

TEST_CASE("cancellation_token отменяет read_some_async") {
    test_context ctx;
    cancellation_source source;
    std::string err;

    sync_wait(ctx.sched, pending_read_cancelled(ctx, source, err));

    REQUIRE(err.find("отменена") != std::string::npos);
}
