#include <netlib/net/coro.hpp>

#include "test_context.hpp"

#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace netlib::fakes;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::net::read_unix_string_async;
using rrmode::netlib::net::write_all_unix_async;

namespace {

task<std::string> roundtrip(test_context& ctx) {
    int const a = ctx.sockets.create_unix_stream_socket();
    int const b = ctx.sockets.create_unix_stream_socket();
    ctx.sockets.link_peers(a, b);

    rrmode::netlib::net::unix_stream_socket writer{b, ctx.sockets};
    rrmode::netlib::net::unix_stream_socket reader{a, ctx.sockets};

    co_await write_all_unix_async(writer, ctx.loop, std::vector<char>{'u', 'n', 'i', 'x'});
    ctx.flush();

    co_return co_await read_unix_string_async(reader, ctx.loop, 16);
}

}  // namespace

TEST_CASE("unix read/write awaitables на fake backend") {
    test_context ctx;
    REQUIRE(sync_wait(ctx.sched, roundtrip(ctx)) == "unix");
}
