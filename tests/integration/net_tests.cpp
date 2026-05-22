#include <netlib/execution/scheduler.hpp>
#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/event_loop.hpp>

#include <atomic>
#include <chrono>
#include <catch2/catch_test_macros.hpp>
#include <thread>

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#endif

using namespace std::chrono_literals;

#if !defined(_WIN32)

TEST_CASE("event_loop получает событие pipe") {
    rrmode::netlib::execution::thread_pool pool{2};
    rrmode::netlib::execution::scheduler sched{pool};
    rrmode::netlib::net::event_loop loop{sched};

    int pipefd[2]{-1, -1};
    REQUIRE(::pipe(pipefd) == 0);
    int flags = ::fcntl(pipefd[0], F_GETFL, 0);
    REQUIRE(flags >= 0);
    REQUIRE(::fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK) == 0);

    std::atomic<bool> fired{false};
    loop.register_fd(pipefd[0], rrmode::netlib::net::detail::poll_event::readable,
                     [&fired](rrmode::netlib::net::detail::poll_event) {
                         fired.store(true, std::memory_order_release);
                     });

    std::thread io_thread{[&loop] {
        while (!loop.run_once(500ms)) {
        }
    }};

    std::this_thread::sleep_for(20ms);
    char const byte = 1;
    REQUIRE(::write(pipefd[1], &byte, 1) == 1);

    auto const deadline = std::chrono::steady_clock::now() + 2s;
    while (!fired.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(5ms);
    }

    loop.stop();
    io_thread.join();
    ::close(pipefd[0]);
    ::close(pipefd[1]);
    pool.shutdown();

    REQUIRE(fired.load(std::memory_order_acquire));
}

#else

TEST_CASE("event_loop reactor smoke на Windows") {
    rrmode::netlib::execution::thread_pool pool{1};
    rrmode::netlib::execution::scheduler sched{pool};
    rrmode::netlib::net::event_loop loop{sched};
    pool.shutdown();
    REQUIRE(true);
}

#endif
