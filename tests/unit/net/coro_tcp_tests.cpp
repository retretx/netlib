#include <netlib/net/coro.hpp>

#include "test_context.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace netlib::fakes;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::net::read_string_async;
using rrmode::netlib::net::write_all_async;

namespace {

task<std::string> roundtrip(test_context& ctx) {
    int const a = ctx.sockets.create_tcp_socket();
    int const b = ctx.sockets.create_tcp_socket();
    ctx.sockets.link_peers(a, b);

    rrmode::netlib::net::tcp_socket writer{b, ctx.sockets};
    rrmode::netlib::net::tcp_socket reader{a, ctx.sockets};

    co_await write_all_async(writer, ctx.loop, std::vector<char>{'n', 'e', 't'});
    ctx.flush();

    co_return co_await read_string_async(reader, ctx.loop, 16);
}

}  // namespace

TEST_CASE("read_string_async читает с fake backend") {
    test_context ctx;
    REQUIRE(sync_wait(ctx.sched, roundtrip(ctx)) == "net");
}
