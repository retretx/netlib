#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/event_loop.hpp>
#include <netlib/net/udp_socket.hpp>

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <array>
#include <string>
#include <thread>
#include <vector>

#if defined(__linux__) || defined(__APPLE__)

using namespace std::chrono_literals;

namespace {

TEST_CASE("UDP loopback ping-pong") {
    rrmode::netlib::execution::thread_pool pool{2};
    rrmode::netlib::execution::scheduler sched{pool};
    rrmode::netlib::net::event_loop loop{sched};

    rrmode::netlib::net::udp_socket server;
    rrmode::netlib::net::udp_socket client;
    server.bind({.host = "127.0.0.1", .port = 0});
    client.bind({.host = "127.0.0.1", .port = 0});
    auto const server_ep = server.local_endpoint();

    std::atomic<bool> stop_io{false};
    std::thread io_thread{[&loop, &stop_io] {
        while (!stop_io.load(std::memory_order_acquire)) {
            loop.run_once(5ms);
        }
    }};

    std::string server_got;
    std::array<char, 64> server_buf{};
    server.async_recv_from(
        loop, server_buf,
        [&](rrmode::netlib::net::endpoint remote, std::size_t n) {
            server_got.assign(server_buf.data(), server_buf.data() + static_cast<std::ptrdiff_t>(n));
            std::vector<char> echo{server_got.begin(), server_got.end()};
            server.async_send_to(loop, std::move(echo), remote, [] {}, [](auto const&) {});
        },
        [](auto const&) {});

    std::atomic<bool> client_done{false};
    std::string client_msg;
    std::array<char, 64> client_buf{};

    client.async_send_to(loop, std::vector<char>{'p', 'o', 'n', 'g'}, server_ep,
                       [&] {
                           client.async_recv_from(
                               loop, client_buf,
                               [&](rrmode::netlib::net::endpoint, std::size_t bytes) {
                                   client_msg.assign(
                                       client_buf.data(),
                                       client_buf.data() + static_cast<std::ptrdiff_t>(bytes));
                                   client_done.store(true);
                               },
                               [](auto const&) {});
                       },
                       [](auto const&) {});

    auto const deadline = std::chrono::steady_clock::now() + 2s;
    while (!client_done.load(std::memory_order_acquire)) {
        REQUIRE(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(2ms);
    }

    stop_io.store(true);
    loop.stop();
    io_thread.join();
    server.close(loop);
    client.close(loop);
    pool.shutdown();

    REQUIRE(server_got == "pong");
    REQUIRE(client_msg == "pong");
}

#else

TEST_CASE("UDP loopback ping-pong") { REQUIRE(true); }

#endif

}  // namespace
