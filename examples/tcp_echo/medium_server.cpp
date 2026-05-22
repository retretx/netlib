#include <netlib/medium.hpp>

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
    auto const port = static_cast<std::uint16_t>(argc > 1 ? std::stoi(argv[1]) : 9001);

    rrmode::netlib::net::medium::socket_options opts;
    opts.reuseaddr = true;
    opts.tcp_nodelay = true;

    rrmode::netlib::net::medium::io_context ctx;
    rrmode::netlib::net::medium::tcp_acceptor acc{ctx, opts};
    acc.open({.host = "127.0.0.1", .port = port});

    std::cout << "medium server on " << acc.local_endpoint().port << '\n';

    std::atomic<bool> done{false};
    acc.async_accept(
        [&](rrmode::netlib::net::medium::tcp_socket peer) {
            char buf[256]{};
            peer.async_read_some(
                std::span<char>{buf},
                [&](std::size_t n) {
                    std::vector<char> echo(buf, buf + static_cast<std::ptrdiff_t>(n));
                    peer.async_write_all(
                        std::move(echo), [&done]() { done.store(true); },
                        [](auto const& e) { std::cerr << e.what(); });
                },
                []() {},
                [](auto const& e) { std::cerr << e.what(); });
        },
        [](auto const& e) { std::cerr << e.what(); });

    while (!done.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    return 0;
}
