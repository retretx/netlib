#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/thread_pool.hpp>

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <thread>

using rrmode::netlib::execution::scheduler;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::execution::thread_pool;
using rrmode::netlib::execution::timeout_error;
using rrmode::netlib::execution::when_any;
using rrmode::netlib::execution::with_timeout;

namespace {

task<int> slow(scheduler& sched, int v, std::chrono::milliseconds ms) {
    co_await sched;
    std::this_thread::sleep_for(ms);
    co_return v;
}

task<int> race(scheduler& sched) {
    co_return co_await when_any(sched, slow(sched, 1, std::chrono::milliseconds{50}),
                                slow(sched, 2, std::chrono::milliseconds{5}));
}

task<int> timeout_race(scheduler& sched) {
    co_await with_timeout(sched, slow(sched, 99, std::chrono::milliseconds{200}),
                          std::chrono::milliseconds{20});
    co_return 0;
}

}  // namespace

TEST_CASE("when_any: побеждает быстрый task") {
    thread_pool pool{4};
    scheduler sched{pool};
    REQUIRE(sync_wait(sched, race(sched)) == 2);
    // проигравший task может ещё спать в pool
    std::this_thread::sleep_for(std::chrono::milliseconds{60});
    pool.shutdown();
}

TEST_CASE("with_timeout: бросает timeout_error") {
    thread_pool pool{2};
    scheduler sched{pool};
    REQUIRE_THROWS_AS(sync_wait(sched, timeout_race(sched)), timeout_error);
    std::this_thread::sleep_for(std::chrono::milliseconds{300});
    pool.shutdown();
}
