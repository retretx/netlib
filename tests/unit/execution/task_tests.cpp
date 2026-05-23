#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/error.hpp>
#include <netlib/execution/executor.hpp>
#include <netlib/execution/thread_pool.hpp>

#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <stdexcept>

using rrmode::netlib::execution::execution_error;
using rrmode::netlib::execution::executor;
using rrmode::netlib::execution::scheduler;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::execution::thread_pool;

namespace {

task<int> add_one(scheduler& sched, int v) {
    co_await sched;
    co_return v + 1;
}

task<int> add_two_nested(scheduler& sched, int v) {
    co_await sched;
    int const a = co_await add_one(sched, v);
    co_await sched;
    co_return a + 1;
}

task<void> void_task(scheduler& sched) {
    co_await sched;
    co_return;
}

}  // namespace

TEST_CASE("task sync_wait возвращает значение") {
    thread_pool pool{2};
    scheduler sched{pool};
    REQUIRE(sync_wait(sched, add_one(sched, 41)) == 42);
    pool.shutdown();
}

TEST_CASE("task co_await вложенной task") {
    thread_pool pool{2};
    scheduler sched{pool};
    REQUIRE(sync_wait(sched, add_two_nested(sched, 40)) == 42);
    pool.shutdown();
}

TEST_CASE("task void завершается без значения") {
    thread_pool pool{1};
    scheduler sched{pool};
    REQUIRE_NOTHROW(sync_wait(sched, void_task(sched)));
    pool.shutdown();
}

TEST_CASE("task: execution_error от schedule возобновляет continuation") {
    struct stopped_executor final : executor {
        void post(std::function<void()> /*fn*/) override {
            throw execution_error("post в остановленный thread_pool");
        }
    };

    stopped_executor ex;
    scheduler sched{ex};
    bool parent_done = false;
    bool outer_done = false;

    auto child = []() -> task<void> { co_return; };
    auto parent = [&]() -> task<void> {
        co_await child();
        parent_done = true;
    };
    auto outer = [&]() -> task<void> {
        co_await parent();
        outer_done = true;
    };

    auto h = outer().release();
    h.promise().set_scheduler(sched);
    h.resume();

    REQUIRE(parent_done);
    REQUIRE(outer_done);
}

TEST_CASE("task пробрасывает исключение") {
    thread_pool pool{1};
    scheduler sched{pool};

    auto failing = [](scheduler& sched) -> task<int> {
        co_await sched;
        throw std::runtime_error("boom");
        co_return 0;
    };

    REQUIRE_THROWS_AS(sync_wait(sched, failing(sched)), std::runtime_error);
    pool.shutdown();
}
