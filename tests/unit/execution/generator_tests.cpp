#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/thread_pool.hpp>

#include <catch2/catch_test_macros.hpp>

using rrmode::netlib::execution::generator;
using rrmode::netlib::execution::scheduler;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::execution::thread_pool;

namespace {

generator<int> iota(scheduler& sched, int count) {
    for (int i = 0; i < count; ++i) {
        co_await sched;
        co_yield i;
    }
}

task<int> consume(scheduler& sched, generator<int> gen) {
    int sum = 0;
    while (auto v = co_await gen.next(sched)) {
        sum += *v;
    }
    co_return sum;
}

}  // namespace

TEST_CASE("generator выдаёт последовательность значений") {
    thread_pool pool{2};
    scheduler sched{pool};
    REQUIRE(sync_wait(sched, consume(sched, iota(sched, 4))) == 6);
    pool.shutdown();
}
