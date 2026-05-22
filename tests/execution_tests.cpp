#include <netlib/execution/operation.hpp>
#include <netlib/execution/scheduler.hpp>
#include <netlib/execution/thread_pool.hpp>

#include <atomic>
#include <chrono>
#include <catch2/catch_test_macros.hpp>
#include <thread>

using namespace std::chrono_literals;

TEST_CASE("thread_pool выполняет задачу") {
    rrmode::netlib::execution::thread_pool pool{2};
    std::atomic<bool> done{false};

    pool.post([&done] { done.store(true, std::memory_order_release); });

    auto const deadline = std::chrono::steady_clock::now() + 2s;
    while (!done.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }

    pool.shutdown();
    REQUIRE(done.load(std::memory_order_acquire));
}

TEST_CASE("thread_pool выполняет несколько задач") {
    rrmode::netlib::execution::thread_pool pool{4};
    std::atomic<int> count{0};
    constexpr int n = 32;

    for (int i = 0; i < n; ++i) {
        pool.post([&count] { count.fetch_add(1, std::memory_order_relaxed); });
    }

    auto const deadline = std::chrono::steady_clock::now() + 2s;
    while (count.load(std::memory_order_acquire) < n &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }

    pool.shutdown();
    REQUIRE(count.load(std::memory_order_acquire) == n);
}

TEST_CASE("scheduler перенаправляет на executor") {
    rrmode::netlib::execution::thread_pool pool{1};
    rrmode::netlib::execution::scheduler sched{pool};
    std::atomic<bool> done{false};

    sched.schedule([&done] { done.store(true, std::memory_order_release); });

    auto const deadline = std::chrono::steady_clock::now() + 2s;
    while (!done.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }

    REQUIRE(&sched.get_executor() == &pool);
    pool.shutdown();
    REQUIRE(done.load(std::memory_order_acquire));
}

TEST_CASE("operation поддерживает отмену") {
    struct test_operation : rrmode::netlib::execution::operation {};

    test_operation op;
    REQUIRE_FALSE(op.is_cancelled());
    op.cancel();
    REQUIRE(op.is_cancelled());
    op.cancel();
    REQUIRE(op.is_cancelled());
}

TEST_CASE("post в остановленный thread_pool бросает execution_error") {
    rrmode::netlib::execution::thread_pool pool{1};
    pool.shutdown();
    REQUIRE_THROWS_AS(pool.post([] {}), rrmode::netlib::execution::execution_error);
}
