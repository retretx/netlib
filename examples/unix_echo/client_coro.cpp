#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/coro.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#if !defined(_WIN32)

using rrmode::netlib::execution::scheduler;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::execution::thread_pool;
using rrmode::netlib::net::read_unix_string_async;
using rrmode::netlib::net::unix_connect;
using rrmode::netlib::net::write_all_unix_async;

namespace {

task<std::string> ping(scheduler& sched, rrmode::netlib::net::event_loop& loop,
                       std::string const& path, std::string message) {
    co_await sched;
    rrmode::netlib::net::unix_stream_socket client;
    co_await unix_connect(client, loop, {.path = path});
    co_await sched;
    co_await write_all_unix_async(client, loop, std::vector<char>{message.begin(), message.end()});
    co_await sched;
    auto reply = co_await read_unix_string_async(client, loop);
    client.close(loop);
    co_return reply;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            std::cerr << "использование: unix_echo_client_coro <socket-path> [message]\n";
            return 1;
        }
        std::string const path = argv[1];
        std::string message = argc > 2 ? argv[2] : "hello-unix";

        thread_pool pool{2};
        scheduler sched{pool};
        rrmode::netlib::net::event_loop loop{sched};

        std::atomic<bool> stop{false};
        std::thread io{[&] {
            while (!stop.load()) {
                loop.run_once(std::chrono::milliseconds{10});
            }
        }};

        auto const reply = sync_wait(sched, ping(sched, loop, path, message));

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
    std::cerr << "unix_echo client_coro: только POSIX\n";
    return 1;
}

#endif
