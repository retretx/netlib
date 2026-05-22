#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/coro.hpp>

#include "../common/io_runner.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#if !defined(_WIN32)

using rrmode::netlib::execution::scheduler;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::execution::thread_pool;
using rrmode::netlib::net::cancellation_source;
using rrmode::netlib::net::read_unix_string_with_timeout;
using rrmode::netlib::net::unix_connect;
using rrmode::netlib::net::write_all_unix_async;

namespace {

task<std::string> echo_with_timeout(scheduler& sched, rrmode::netlib::net::event_loop& loop,
                                    std::string const& path, std::string message) {
    co_await sched;

    cancellation_source src;
    rrmode::netlib::net::unix_stream_socket client;
    co_await unix_connect(client, loop, {.path = path}, sched, std::chrono::seconds{5}, &src);

    co_await sched;
    co_await write_all_unix_async(client, loop, std::vector<char>{message.begin(), message.end()});

    co_await sched;
    auto response = co_await read_unix_string_with_timeout(client, loop, sched,
                                                          std::chrono::seconds{5}, &src, 256);

    client.close(loop);
    co_return response;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            std::cerr << "использование: unix_echo_client_coro_timeout <socket-path> [message]\n";
            return 1;
        }
        std::string const path = argv[1];
        std::string message = argc > 2 ? argv[2] : "hello-unix";

        thread_pool pool{2};
        scheduler sched{pool};
        rrmode::netlib::net::event_loop loop{sched};
        netlib_examples::io_runner io{loop};

        auto const response = sync_wait(sched, echo_with_timeout(sched, loop, path, message));

        io.stop();
        pool.shutdown();
        std::cout << "ответ (timeout 5s): " << response << '\n';
        return 0;
    } catch (std::exception const& e) {
        std::cerr << "ошибка: " << e.what() << '\n';
        return 1;
    }
}

#else

int main() {
    std::cerr << "unix_echo_client_coro_timeout: только POSIX\n";
    return 1;
}

#endif
