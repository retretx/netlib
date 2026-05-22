#include <netlib/execution/scheduler.hpp>
#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/event_loop.hpp>
#include <netlib/net/tcp_socket.hpp>

#include "../common/io_runner.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#if !defined(_WIN32)

namespace {

struct options {
    std::string host{"127.0.0.1"};
    std::uint16_t port{9'001};
    std::string message{"hello"};
};

options parse_options(int argc, char** argv) {
    options o;
    if (argc > 1) {
        o.port = static_cast<std::uint16_t>(std::atoi(argv[1]));
    }
    if (argc > 2) {
        o.message = argv[2];
    }
    if (argc > 3) {
        o.host = argv[3];
    }
    return o;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        auto const opt = parse_options(argc, argv);

        rrmode::netlib::execution::thread_pool pool{2};
        rrmode::netlib::execution::scheduler sched{pool};
        rrmode::netlib::net::event_loop loop{sched};
        netlib_examples::io_runner io{loop};

        rrmode::netlib::net::tcp_socket client;
        std::atomic<bool> done{false};
        std::string response;
        std::string error_msg;

        client.async_connect(
            loop, {.host = opt.host, .port = opt.port},
            [&]() {
                std::vector<char> payload{opt.message.begin(), opt.message.end()};
                client.async_write_all(
                    loop, std::move(payload),
                    [&]() {
                        auto buf = std::make_shared<std::array<char, 256>>();
                        client.async_read_some(
                            loop, std::span<char>{buf->data(), buf->size()},
                            [&done, &response, buf](std::size_t n) {
                                response.assign(buf->data(), n);
                                done.store(true, std::memory_order_release);
                            },
                            [&error_msg]() { error_msg = "eof"; },
                            [&error_msg](rrmode::netlib::net::net_error const& e) {
                                error_msg = e.what();
                            });
                    },
                    [&error_msg](rrmode::netlib::net::net_error const& e) { error_msg = e.what(); });
            },
            [&error_msg](rrmode::netlib::net::net_error const& e) { error_msg = e.what(); });

        auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        while (!done.load(std::memory_order_acquire) && error_msg.empty() &&
               std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }

        io.stop();
        client.close(loop);
        pool.shutdown();

        if (!error_msg.empty()) {
            std::cerr << "ошибка: " << error_msg << '\n';
            return 1;
        }
        if (!done.load()) {
            std::cerr << "таймаут\n";
            return 1;
        }

        std::cout << "ответ: " << response << '\n';
        return 0;
    } catch (std::exception const& e) {
        std::cerr << "ошибка: " << e.what() << '\n';
        return 1;
    }
}

#else

int main() {
    std::cerr << "tcp_echo client: пример только для POSIX (Linux/macOS)\n";
    return 1;
}

#endif
