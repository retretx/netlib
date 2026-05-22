#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/coro.hpp>

#include "../common/io_runner.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>

#if !defined(_WIN32)

using rrmode::netlib::execution::scheduler;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::execution::thread_pool;
using rrmode::netlib::net::cancellation_source;
using rrmode::netlib::net::unix_echo_server_loop;
using rrmode::netlib::net::unix_stream_acceptor;

namespace {

std::string default_sock_path() {
    return std::string{"/tmp/netlib_unix_echo_"} + std::to_string(static_cast<unsigned>(::getpid())) +
           ".sock";
}

task<void> run_server(scheduler& sched, rrmode::netlib::net::event_loop& loop,
                      std::string const& path, cancellation_source& shutdown) {
    unix_stream_acceptor acceptor;
    acceptor.open({.path = path});
    std::cout << "unix coro echo: " << acceptor.local_endpoint().path << " (Enter — остановка)\n";
    co_await unix_echo_server_loop(acceptor, loop, &shutdown.token());
    acceptor.close(loop);
    std::remove(path.c_str());
}

}  // namespace

int main(int argc, char** argv) {
    try {
        auto const path = argc > 1 ? std::string{argv[1]} : default_sock_path();

        thread_pool pool{2};
        scheduler sched{pool};
        rrmode::netlib::net::event_loop loop{sched};
        netlib_examples::io_runner io{loop};

        cancellation_source shutdown;
        std::atomic<bool> server_done{false};

        std::thread server_thread{[&] {
            try {
                sync_wait(sched, run_server(sched, loop, path, shutdown));
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
    std::cerr << "unix_echo server_coro: только POSIX\n";
    return 1;
}

#endif
