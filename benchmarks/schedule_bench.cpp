#include <netlib/execution/scheduler.hpp>
#include <netlib/execution/thread_pool.hpp>

#include "bench_common.hpp"

#include <atomic>
#include <iostream>
#include <thread>

int main(int argc, char** argv) {
    auto const iters = netlib_benchmark::parse_iterations(argc, argv, 100'000);

    rrmode::netlib::execution::thread_pool pool{4};
    rrmode::netlib::execution::scheduler sched{pool};

    std::atomic<std::size_t> done{0};
    auto const start = netlib_benchmark::clock::now();

    for (std::size_t i = 0; i < iters; ++i) {
        sched.schedule([&done] { done.fetch_add(1, std::memory_order_relaxed); });
    }

    while (done.load(std::memory_order_acquire) < iters) {
        std::this_thread::yield();
    }

    auto const end = netlib_benchmark::clock::now();
    pool.shutdown();

    netlib_benchmark::print_throughput("schedule_bench", iters,
                                       netlib_benchmark::elapsed_ms(start, end));
    return 0;
}
