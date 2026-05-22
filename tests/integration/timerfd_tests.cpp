#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/event_loop.hpp>

#include <atomic>
#include <chrono>
#include <catch2/catch_test_macros.hpp>
#include <thread>

#if defined(__linux__)

TEST_CASE("event_loop timerfd: run_after через epoll") {
    rrmode::netlib::execution::thread_pool pool{2};
    rrmode::netlib::execution::scheduler sched{pool};
    rrmode::netlib::net::event_loop loop{sched};

    REQUIRE(loop.uses_kernel_timers());

    std::atomic<bool> fired{false};
    auto const t0 = std::chrono::steady_clock::now();

    loop.run_after(std::chrono::milliseconds{30}, [&fired] { fired.store(true); });

    std::atomic<bool> stop_io{false};
    std::thread io{[&loop, &stop_io] {
        while (!stop_io.load(std::memory_order_acquire)) {
            (void)loop.run_once(std::chrono::milliseconds{50});
        }
    }};

    while (!fired.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }

    auto const elapsed = std::chrono::steady_clock::now() - t0;
    stop_io.store(true, std::memory_order_release);
    loop.stop();
    io.join();
    pool.shutdown();

    REQUIRE(elapsed >= std::chrono::milliseconds{25});
    REQUIRE(elapsed < std::chrono::milliseconds{200});
}

#else

TEST_CASE("event_loop timerfd: run_after через epoll") { REQUIRE(true); }

#endif
