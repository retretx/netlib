#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/event_loop.hpp>
#include <netlib/net/udp_socket.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

#if !defined(_WIN32)

namespace {

std::uint16_t parse_port(char const* arg) {
    if (arg == nullptr) {
        return 9'002;
    }
    int const p = std::atoi(arg);
    if (p <= 0 || p > 65535) {
        throw rrmode::netlib::net::net_error("неверный порт");
    }
    return static_cast<std::uint16_t>(p);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        auto const port = parse_port(argc > 1 ? argv[1] : nullptr);

        rrmode::netlib::execution::thread_pool pool{2};
        rrmode::netlib::execution::scheduler sched{pool};
        rrmode::netlib::net::event_loop loop{sched};

        rrmode::netlib::net::udp_socket server;
        server.bind({.host = "127.0.0.1", .port = port});
        std::cout << "UDP echo " << server.local_endpoint().host << ':' << server.local_endpoint().port
                  << '\n';

        std::atomic<bool> stop{false};
        std::thread io{[&] {
            while (!stop.load()) {
                loop.run_once(std::chrono::milliseconds{10});
            }
        }};

        std::array<char, 2048> buf{};
        std::function<void()> serve_once;
        serve_once = [&] {
            server.async_recv_from(
                loop, buf,
                [&](rrmode::netlib::net::endpoint remote, std::size_t n) {
                    std::vector<char> echo{buf.data(), buf.data() + static_cast<std::ptrdiff_t>(n)};
                    server.async_send_to(loop, std::move(echo), remote, [] {}, [](auto const&) {});
                    serve_once();
                },
                [&](rrmode::netlib::net::net_error const& e) {
                    std::cerr << "recv: " << e.what() << '\n';
                });
        };
        serve_once();

        std::cout << "Enter — выход\n";
        std::cin.get();
        stop.store(true);
        loop.stop();
        io.join();
        server.close(loop);
        pool.shutdown();
        return 0;
    } catch (std::exception const& e) {
        std::cerr << "ошибка: " << e.what() << '\n';
        return 1;
    }
}

#else

int main() {
    std::cerr << "udp_echo server: только POSIX\n";
    return 1;
}

#endif
