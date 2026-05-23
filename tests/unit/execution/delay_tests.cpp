#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/thread_pool.hpp>

#include <catch2/catch_test_macros.hpp>
#include <chrono>

using rrmode::netlib::execution::delay_async;
using rrmode::netlib::execution::scheduler;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::execution::thread_pool;
using rrmode::netlib::execution::when_all;

namespace {

task<void> pause(scheduler& sched, std::chrono::milliseconds ms) {
    co_await delay_async(sched, ms);
}

task<void> parallel_delays(scheduler& sched) {
    co_await when_all(sched, pause(sched, std::chrono::milliseconds{40}),
                      pause(sched, std::chrono::milliseconds{40}));
}

}  // namespace

TEST_CASE("delay_async scheduler: параллельные паузы") {
    thread_pool pool{4};
    scheduler sched{pool};

    auto const t0 = std::chrono::steady_clock::now();
    sync_wait(sched, parallel_delays(sched));
    auto const elapsed = std::chrono::steady_clock::now() - t0;

    REQUIRE(elapsed >= std::chrono::milliseconds{35});
    REQUIRE(elapsed < std::chrono::milliseconds{300});
    pool.shutdown();
}

TEST_CASE("delay_async scheduler: нулевая длительность") {
    thread_pool pool{1};
    scheduler sched{pool};
    sync_wait(sched, pause(sched, std::chrono::milliseconds{0}));
    pool.shutdown();
    REQUIRE(true);
}
