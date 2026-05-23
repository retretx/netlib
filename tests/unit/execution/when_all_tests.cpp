#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/thread_pool.hpp>

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <stdexcept>
#include <thread>

using rrmode::netlib::execution::scheduler;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::execution::thread_pool;
using rrmode::netlib::execution::when_all;

namespace {

task<int> slow_int(scheduler& sched, int v, std::chrono::milliseconds delay) {
    co_await sched;
    std::this_thread::sleep_for(delay);
    co_return v;
}

task<int> when_all_demo(scheduler& sched) {
    auto [a, b] = co_await when_all(sched, slow_int(sched, 10, std::chrono::milliseconds{30}),
                                    slow_int(sched, 20, std::chrono::milliseconds{10}));
    co_return a + b;
}

}  // namespace

TEST_CASE("when_all выполняет обе task параллельно") {
    thread_pool pool{4};
    scheduler sched{pool};

    auto const t0 = std::chrono::steady_clock::now();
    REQUIRE(sync_wait(sched, when_all_demo(sched)) == 30);
    auto const elapsed = std::chrono::steady_clock::now() - t0;

    REQUIRE(elapsed < std::chrono::milliseconds{200});
    pool.shutdown();
}

TEST_CASE("when_all пробрасывает исключение") {
    thread_pool pool{2};
    scheduler sched{pool};

    auto fail = [](scheduler& sched) -> task<int> {
        co_await sched;
        throw std::runtime_error("fail");
        co_return 0;
    };

    auto runner = [&]() -> task<int> {
        auto [a, b] = co_await when_all(sched, fail(sched), slow_int(sched, 1, std::chrono::milliseconds{1}));
        co_return a + b;
    };

    REQUIRE_THROWS_AS(sync_wait(sched, runner()), std::runtime_error);
    pool.shutdown();
}
