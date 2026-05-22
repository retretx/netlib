#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/thread_pool.hpp>

#include <catch2/catch_test_macros.hpp>

using rrmode::netlib::execution::scheduler;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::execution::then;
using rrmode::netlib::execution::thread_pool;

namespace {

task<int> produce(scheduler& sched, int v) {
    co_await sched;
    co_return v;
}

}  // namespace

TEST_CASE("then преобразует результат task") {
    thread_pool pool{2};
    scheduler sched{pool};

    auto pipeline = then(sched, produce(sched, 10), [](int v) { return v * 2; });
    REQUIRE(sync_wait(sched, std::move(pipeline)) == 20);
    pool.shutdown();
}
