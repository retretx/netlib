#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/thread_pool.hpp>

#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

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
