#include <netlib/execution/coroutine.hpp>
#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/coro.hpp>
#include <netlib/net/tcp_socket.hpp>

#include "../common/io_runner.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#if !defined(_WIN32)

using rrmode::netlib::execution::scheduler;
using rrmode::netlib::execution::sync_wait;
using rrmode::netlib::execution::task;
using rrmode::netlib::execution::thread_pool;
using rrmode::netlib::net::connect_async;
using rrmode::netlib::net::read_string_async;
using rrmode::netlib::net::write_all_async;

namespace {

task<std::string> echo_once(scheduler& sched, rrmode::netlib::net::event_loop& loop,
                            rrmode::netlib::net::endpoint const& ep, std::string message) {
    co_await sched;

    rrmode::netlib::net::tcp_socket client;
    co_await connect_async(client, loop, ep);

    co_await sched;
    co_await write_all_async(client, loop, std::vector<char>{message.begin(), message.end()});

    co_await sched;
    auto response = co_await read_string_async(client, loop, 256);

    client.close(loop);
    co_return response;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::uint16_t port = 9'001;
        std::string message = "hello";
        if (argc > 1) {
            port = static_cast<std::uint16_t>(std::atoi(argv[1]));
        }
        if (argc > 2) {
            message = argv[2];
        }

        thread_pool pool{2};
        scheduler sched{pool};
        rrmode::netlib::net::event_loop loop{sched};
        netlib_examples::io_runner io{loop};

        auto const response =
            sync_wait(sched, echo_once(sched, loop, {.host = "127.0.0.1", .port = port}, message));

        io.stop();
        pool.shutdown();
        std::cout << "ответ: " << response << '\n';
        return 0;
    } catch (std::exception const& e) {
        std::cerr << "ошибка: " << e.what() << '\n';
        return 1;
    }
}

#else

int main() {
    std::cerr << "client_coro: только POSIX\n";
    return 1;
}

#endif
