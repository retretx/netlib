#include <netlib/net/coro.hpp>

#include "test_context.hpp"

#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace netlib::fakes;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::net::read_exact_async;
using rrmode::netlib::net::write_all_async;

namespace {

task<std::vector<char>> read_after_write(test_context& ctx) {
    int const a = ctx.sockets.create_tcp_socket();
    int const b = ctx.sockets.create_tcp_socket();
    ctx.sockets.link_peers(a, b);

    rrmode::netlib::net::tcp_socket writer{b, ctx.sockets};
    rrmode::netlib::net::tcp_socket reader{a, ctx.sockets};

    std::vector<char> payload{'e', 'x', 'a', 'c', 't'};
    co_await write_all_async(writer, ctx.loop, payload);
    ctx.flush();

    std::vector<char> buf(5);
    co_await read_exact_async(reader, ctx.loop, std::span<char>{buf.data(), buf.size()});
    co_return buf;
}

}  // namespace

TEST_CASE("read_exact_async читает ровно N байт") {
    test_context ctx;
    auto buf = sync_wait(ctx.sched, read_after_write(ctx));
    REQUIRE(buf == std::vector<char>{'e', 'x', 'a', 'c', 't'});
}
