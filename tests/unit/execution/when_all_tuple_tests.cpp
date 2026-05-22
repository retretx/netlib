#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/thread_pool.hpp>

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <thread>
#include <tuple>

using rrmode::netlib::execution::scheduler;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::execution::thread_pool;
using rrmode::netlib::execution::when_all;

namespace {

task<int> value_after(scheduler& sched, int v, std::chrono::milliseconds delay) {
    co_await sched;
    std::this_thread::sleep_for(delay);
    co_return v;
}

task<int> sum_three(scheduler& sched) {
    auto [a, b, c] = co_await when_all(sched, value_after(sched, 1, std::chrono::milliseconds{30}),
                                       value_after(sched, 2, std::chrono::milliseconds{10}),
                                       value_after(sched, 3, std::chrono::milliseconds{20}));
    co_return a + b + c;
}

}  // namespace

TEST_CASE("when_all tuple: 3 task параллельно") {
    thread_pool pool{4};
    scheduler sched{pool};

    auto const t0 = std::chrono::steady_clock::now();
    REQUIRE(sync_wait(sched, sum_three(sched)) == 6);
    auto const elapsed = std::chrono::steady_clock::now() - t0;

    pool.shutdown();
    REQUIRE(elapsed < std::chrono::milliseconds{80});
}

TEST_CASE("when_all tuple: разные типы") {
    thread_pool pool{2};
    scheduler sched{pool};

    auto int_task = [](scheduler& sched) -> task<int> {
        co_await sched;
        co_return 7;
    };
    auto str_task = [](scheduler& sched) -> task<std::string> {
        co_await sched;
        co_return "ok";
    };
    auto bool_task = [](scheduler& sched) -> task<bool> {
        co_await sched;
        co_return true;
    };

    auto [i, s, b] = sync_wait(sched, when_all(sched, int_task(sched), str_task(sched), bool_task(sched)));
    pool.shutdown();

    REQUIRE(i == 7);
    REQUIRE(s == "ok");
    REQUIRE(b);
}
