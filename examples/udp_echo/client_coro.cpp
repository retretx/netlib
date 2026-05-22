#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/coro.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#if !defined(_WIN32)

using rrmode::netlib::execution::scheduler;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::execution::thread_pool;
using rrmode::netlib::net::udp_recv_string;
using rrmode::netlib::net::udp_send_string;

namespace {

task<std::string> ping(scheduler& sched, rrmode::netlib::net::event_loop& loop,
                       rrmode::netlib::net::endpoint ep, std::string message) {
    co_await sched;
    rrmode::netlib::net::udp_socket client;
    client.bind({.host = "127.0.0.1", .port = 0});
    co_await sched;
    co_await udp_send_string(client, loop, message, ep);
    co_await sched;
    auto reply = co_await udp_recv_string(client, loop, 2048);
    client.close(loop);
    co_return reply;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::uint16_t port = 9'002;
        std::string message = "hello-udp";
        if (argc > 1) {
            port = static_cast<std::uint16_t>(std::atoi(argv[1]));
        }
        if (argc > 2) {
            message = argv[2];
        }

        thread_pool pool{2};
        scheduler sched{pool};
        rrmode::netlib::net::event_loop loop{sched};

        std::atomic<bool> stop{false};
        std::thread io{[&] {
            while (!stop.load()) {
                loop.run_once(std::chrono::milliseconds{10});
            }
        }};

        auto const reply = sync_wait(
            sched, ping(sched, loop, {.host = "127.0.0.1", .port = port}, message));

        stop.store(true);
        loop.stop();
        io.join();
        pool.shutdown();

        std::cout << "ответ: " << reply << '\n';
        return 0;
    } catch (std::exception const& e) {
        std::cerr << "ошибка: " << e.what() << '\n';
        return 1;
    }
}

#else

int main() {
    std::cerr << "udp_echo client_coro: только POSIX\n";
    return 1;
}

#endif
