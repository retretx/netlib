#include <netlib/execution/scheduler.hpp>
#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/event_loop.hpp>
#include <netlib/net/tcp_acceptor.hpp>
#include <netlib/net/tcp_socket.hpp>

#include "../common/io_runner.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#if !defined(_WIN32)

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

void echo_session(rrmode::netlib::net::event_loop& loop, rrmode::netlib::net::tcp_socket peer) {
    auto const io = peer.io_handle();
    auto buf = std::make_shared<std::array<char, 256>>();

    peer.async_read_some(
        loop, std::span<char>{buf->data(), buf->size()},
        [io, &loop, buf](std::size_t n) {
            std::vector<char> out(buf->begin(), buf->begin() + static_cast<std::ptrdiff_t>(n));
            rrmode::netlib::net::tcp_socket{io}.async_write_all(
                loop, std::move(out), []() {}, [](rrmode::netlib::net::net_error const& e) {
                    std::cerr << "write: " << e.what() << '\n';
                });
        },
        []() { std::cerr << "peer закрыл соединение\n"; },
        [](rrmode::netlib::net::net_error const& e) { std::cerr << "read: " << e.what() << '\n'; });
}

void accept_next(rrmode::netlib::net::event_loop& loop, rrmode::netlib::net::tcp_acceptor& acceptor) {
    acceptor.async_accept(
        loop,
        [&loop, &acceptor](rrmode::netlib::net::tcp_socket peer) {
            std::cout << "клиент подключён\n";
            echo_session(loop, std::move(peer));
            accept_next(loop, acceptor);
        },
        [](rrmode::netlib::net::net_error const& e) {
            std::cerr << "accept: " << e.what() << '\n';
        });
}

}  // namespace

int main(int argc, char** argv) {
    try {
        auto const port = parse_port(argc > 1 ? argv[1] : nullptr);

        rrmode::netlib::execution::thread_pool pool{2};
        rrmode::netlib::execution::scheduler sched{pool};
        rrmode::netlib::net::event_loop loop{sched};
        netlib_examples::io_runner io{loop};

        rrmode::netlib::net::tcp_acceptor acceptor;
        acceptor.open({.host = "127.0.0.1", .port = port});
        auto const ep = acceptor.local_endpoint();
        std::cout << "echo-сервер слушает " << ep.host << ':' << ep.port << '\n';

        accept_next(loop, acceptor);

        std::cout << "Ctrl+C для выхода (ожидание соединений)\n";
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds{60});
        }
    } catch (std::exception const& e) {
        std::cerr << "ошибка: " << e.what() << '\n';
        return 1;
    }
}

#else

int main() {
    std::cerr << "tcp_echo server: пример только для POSIX (Linux/macOS)\n";
    return 1;
}

#endif
