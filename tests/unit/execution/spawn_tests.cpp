#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/thread_pool.hpp>

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <thread>

using rrmode::netlib::execution::scheduler;
using rrmode::netlib::execution::spawn;
using rrmode::netlib::execution::task;
using rrmode::netlib::execution::thread_pool;

namespace {

task<void> set_flag(scheduler& sched, std::atomic<bool>& flag) {
    co_await sched;
    flag.store(true, std::memory_order_release);
}

}  // namespace

TEST_CASE("spawn выполняет task без sync_wait") {
    thread_pool pool{2};
    scheduler sched{pool};
    std::atomic<bool> done{false};

    spawn(sched, set_flag(sched, done));

    auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    while (!done.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }

    pool.shutdown();
    REQUIRE(done.load(std::memory_order_acquire));
}
