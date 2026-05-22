#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/thread_pool.hpp>

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <thread>

using rrmode::netlib::execution::scheduler;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::execution::thread_pool;
using rrmode::netlib::execution::when_all;

namespace {

task<void> noop(scheduler& sched) {
    co_await sched;
}

task<void> set_flag(scheduler& sched, std::atomic<bool>& flag) {
    co_await sched;
    flag.store(true, std::memory_order_release);
}

}  // namespace

TEST_CASE("when_all void+void завершается") {
    thread_pool pool{2};
    scheduler sched{pool};
    REQUIRE_NOTHROW(sync_wait(sched, when_all(sched, noop(sched), noop(sched))));
    pool.shutdown();
}

TEST_CASE("when_all vector void") {
    thread_pool pool{3};
    scheduler sched{pool};
    std::atomic<int> count{0};

    auto inc = [&count](scheduler& sched) -> task<void> {
        co_await sched;
        count.fetch_add(1, std::memory_order_relaxed);
    };

    std::vector<task<void>> tasks;
    tasks.push_back(inc(sched));
    tasks.push_back(inc(sched));
    tasks.push_back(inc(sched));

    sync_wait(sched, when_all(sched, std::move(tasks)));
    pool.shutdown();
    REQUIRE(count.load() == 3);
}

TEST_CASE("when_all void+T возвращает значение") {
    thread_pool pool{2};
    scheduler sched{pool};
    std::atomic<bool> done{false};

    auto value_task = [](scheduler& sched) -> task<int> {
        co_await sched;
        co_return 42;
    };

    REQUIRE(sync_wait(sched, when_all(sched, set_flag(sched, done), value_task(sched))) == 42);
    pool.shutdown();
    REQUIRE(done.load());
}
