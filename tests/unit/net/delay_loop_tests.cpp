#include <netlib/net/coro.hpp>

#include "test_context.hpp"

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>

using namespace netlib::fakes;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::net::delay_async;

namespace {

task<bool> wait_50ms(test_context& ctx) {
    auto const t0 = std::chrono::steady_clock::now();
    co_await delay_async(ctx.loop, std::chrono::milliseconds{50});
    auto const elapsed = std::chrono::steady_clock::now() - t0;
    co_return elapsed >= std::chrono::milliseconds{40};
}

}  // namespace

TEST_CASE("event_loop run_after срабатывает после flush") {
    test_context ctx;
    std::atomic<bool> fired{false};
    ctx.loop.run_after(std::chrono::milliseconds{10}, [&fired] { fired.store(true); });
    ctx.flush();
    REQUIRE(fired.load());
}

TEST_CASE("delay_async event_loop: корутина ждёт таймер") {
    test_context ctx;
    REQUIRE(sync_wait(ctx.sched, wait_50ms(ctx)));
}
