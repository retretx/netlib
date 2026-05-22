#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/coro.hpp>

#include "../common/io_runner.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

#if !defined(_WIN32)

using rrmode::netlib::execution::scheduler;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::execution::thread_pool;
using rrmode::netlib::net::cancellation_source;
using rrmode::netlib::net::tcp_acceptor;
using rrmode::netlib::net::tcp_echo_server_loop;

namespace {

std::uint16_t parse_port(char const* arg) {
    if (arg == nullptr) {
        return 9'001;
    }
    int const p = std::atoi(arg);
    if (p <= 0 || p > 65535) {
        throw rrmode::netlib::net::net_error("неверный порт");
    }
    return static_cast<std::uint16_t>(p);
}

task<void> run_server(scheduler& sched, rrmode::netlib::net::event_loop& loop, std::uint16_t port,
                      cancellation_source& shutdown) {
    tcp_acceptor acceptor;
    acceptor.open({.host = "127.0.0.1", .port = port});
    auto const ep = acceptor.local_endpoint();
    std::cout << "coro echo-сервер " << ep.host << ':' << ep.port << " (Enter — остановка)\n";
    co_await tcp_echo_server_loop(acceptor, loop, &shutdown.token());
    acceptor.close(loop);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        auto const port = parse_port(argc > 1 ? argv[1] : nullptr);

        thread_pool pool{2};
        scheduler sched{pool};
        rrmode::netlib::net::event_loop loop{sched};
        netlib_examples::io_runner io{loop};

        cancellation_source shutdown;
        std::atomic<bool> server_done{false};

        std::thread server_thread{[&] {
            try {
                sync_wait(sched, run_server(sched, loop, port, shutdown));
            } catch (std::exception const& e) {
                std::cerr << "сервер: " << e.what() << '\n';
            }
            server_done.store(true, std::memory_order_release);
        }};

        std::cout << "Нажмите Enter для остановки...\n";
        std::cin.get();
        shutdown.cancel();

        while (!server_done.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }

        io.stop();
        pool.shutdown();
        server_thread.join();
        return 0;
    } catch (std::exception const& e) {
        std::cerr << "ошибка: " << e.what() << '\n';
        return 1;
    }
}

#else

int main() {
    std::cerr << "server_coro: только POSIX\n";
    return 1;
}

#endif
