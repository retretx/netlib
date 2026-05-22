#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/thread_pool.hpp>

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <thread>
#include <vector>

using rrmode::netlib::execution::scheduler;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::execution::thread_pool;
using rrmode::netlib::execution::when_all;

namespace {

task<int> delayed(scheduler& sched, int v, std::chrono::milliseconds ms) {
    co_await sched;
    std::this_thread::sleep_for(ms);
    co_return v;
}

task<int> sum_three(scheduler& sched) {
    std::vector<task<int>> tasks;
    tasks.push_back(delayed(sched, 1, std::chrono::milliseconds{30}));
    tasks.push_back(delayed(sched, 2, std::chrono::milliseconds{10}));
    tasks.push_back(delayed(sched, 3, std::chrono::milliseconds{20}));

    auto values = co_await when_all(sched, std::move(tasks));
    int total = 0;
    for (int v : values) {
        total += v;
    }
    co_return total;
}

}  // namespace

TEST_CASE("when_all vector возвращает результаты по порядку") {
    thread_pool pool{4};
    scheduler sched{pool};

    auto const t0 = std::chrono::steady_clock::now();
    REQUIRE(sync_wait(sched, sum_three(sched)) == 6);
    auto const elapsed = std::chrono::steady_clock::now() - t0;

    pool.shutdown();
    REQUIRE(elapsed < std::chrono::milliseconds{80});
}

TEST_CASE("when_all vector пустой") {
    thread_pool pool{1};
    scheduler sched{pool};

    auto empty = [](scheduler& sched) -> task<std::vector<int>> {
        co_return co_await when_all(sched, std::vector<task<int>>{});
    };

    REQUIRE(sync_wait(sched, empty(sched)).empty());
    pool.shutdown();
}
