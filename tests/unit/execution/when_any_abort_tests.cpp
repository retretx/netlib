#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/delay.hpp>
#include <netlib/execution/thread_pool.hpp>

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>

using rrmode::netlib::execution::delay_async;
using rrmode::netlib::execution::scheduler;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::execution::thread_pool;
using rrmode::netlib::execution::when_any;

namespace {

task<int> slow_poll(scheduler& sched, int value, std::shared_ptr<std::atomic<bool>> aborted) {
    co_await sched;
    for (int i = 0; i < 40; ++i) {
        if (aborted->load(std::memory_order_acquire)) {
            co_return -1;
        }
        co_await delay_async(sched, std::chrono::milliseconds{10});
    }
    co_return value;
}

task<int> fast(scheduler& sched, int value) {
    co_await delay_async(sched, std::chrono::milliseconds{5});
    co_return value;
}

task<int> race_with_abort(scheduler& sched, std::shared_ptr<std::atomic<bool>> slow_aborted) {
    co_return co_await when_any(
        sched, slow_poll(sched, 99, slow_aborted), fast(sched, 7),
        [slow_aborted]() { slow_aborted->store(true, std::memory_order_release); }, nullptr);
}

}  // namespace

TEST_CASE("when_any: on_loser_b прерывает медленный task") {
    thread_pool pool{4};
    scheduler sched{pool};
    auto slow_aborted = std::make_shared<std::atomic<bool>>(false);

    REQUIRE(sync_wait(sched, race_with_abort(sched, slow_aborted)) == 7);
    REQUIRE(slow_aborted->load(std::memory_order_acquire));

    std::this_thread::sleep_for(std::chrono::milliseconds{30});
    pool.shutdown();
}
