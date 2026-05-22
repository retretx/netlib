#include <netlib/execution/scheduler.hpp>
#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/detail/posix_socket.hpp>
#include <netlib/net/event_loop.hpp>
#include <netlib/net/tcp_acceptor.hpp>
#include <netlib/net/tcp_socket.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if defined(__linux__) || defined(__APPLE__)

using namespace std::chrono_literals;

TEST_CASE("tcp sync ping loopback") {
    int const listen_fd = rrmode::netlib::net::detail::create_tcp_socket();
    rrmode::netlib::net::detail::set_reuseaddr(listen_fd);
    rrmode::netlib::net::detail::bind_endpoint(listen_fd, {.host = "127.0.0.1", .port = 0});
    rrmode::netlib::net::detail::listen_socket(listen_fd);
    auto const port = rrmode::netlib::net::detail::get_local_port(listen_fd);

    int const client_fd = rrmode::netlib::net::detail::create_tcp_socket();
    rrmode::netlib::net::endpoint remote{.host = "127.0.0.1", .port = port};
    REQUIRE(rrmode::netlib::net::detail::start_connect(client_fd, remote) == 0);

    int const server_fd = ::accept(listen_fd, nullptr, nullptr);
    REQUIRE(server_fd >= 0);

    std::array<char, 16> buf{};
    std::array<char, 4> msg{'p', 'i', 'n', 'g'};
    auto sent = rrmode::netlib::net::detail::try_send(client_fd, std::span<char const>{msg});
    REQUIRE(sent);
    REQUIRE(*sent == 4);

    auto got = rrmode::netlib::net::detail::try_recv(server_fd, buf);
    REQUIRE(got);
    REQUIRE(*got == 4);
    REQUIRE(std::memcmp(buf.data(), "ping", 4) == 0);

    ::close(listen_fd);
    ::close(client_fd);
    ::close(server_fd);
}

TEST_CASE("tcp accept и connect loopback") {
    rrmode::netlib::execution::thread_pool pool{2};
    rrmode::netlib::execution::scheduler sched{pool};
    rrmode::netlib::net::event_loop loop{sched};

    rrmode::netlib::net::tcp_acceptor acceptor;
    acceptor.open({.host = "127.0.0.1", .port = 0});
    auto const listen_ep = acceptor.local_endpoint();

    std::atomic<bool> accepted{false};
    std::atomic<bool> connected{false};
    std::string error_msg;

    acceptor.async_accept(
        loop,
        [&accepted](rrmode::netlib::net::tcp_socket) {
            accepted.store(true, std::memory_order_release);
        },
        [&error_msg](rrmode::netlib::net::net_error const& e) { error_msg = e.what(); });

    rrmode::netlib::net::tcp_socket client;
    client.async_connect(
        loop, listen_ep, [&connected]() { connected.store(true, std::memory_order_release); },
        [&error_msg](rrmode::netlib::net::net_error const& e) { error_msg = e.what(); });

    std::atomic<bool> stop_io{false};
    std::thread io_thread{[&loop, &stop_io] {
        while (!stop_io.load(std::memory_order_acquire)) {
            loop.run_once(10ms);
        }
    }};

    auto const deadline = std::chrono::steady_clock::now() + 3s;
    while ((!accepted.load(std::memory_order_acquire) ||
            !connected.load(std::memory_order_acquire)) &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(5ms);
    }

    stop_io.store(true);
    loop.stop();
    io_thread.join();
    acceptor.close(loop);
    client.close(loop);
    pool.shutdown();

    CAPTURE(error_msg);
    REQUIRE(error_msg.empty());
    REQUIRE(accepted.load(std::memory_order_acquire));
    REQUIRE(connected.load(std::memory_order_acquire));
}

TEST_CASE("tcp echo loopback") {
    rrmode::netlib::execution::thread_pool pool{4};
    rrmode::netlib::execution::scheduler sched{pool};
    rrmode::netlib::net::event_loop loop{sched};

    rrmode::netlib::net::tcp_acceptor acceptor;
    acceptor.open({.host = "127.0.0.1", .port = 0});
    auto const listen_ep = acceptor.local_endpoint();

    std::atomic<bool> echo_done{false};
    std::mutex echo_mutex;
    std::condition_variable echo_cv;
    bool server_echo_sent{false};
    std::string response;
    std::string error_msg;

    acceptor.async_accept(
        loop,
        [&loop, &echo_mutex, &echo_cv, &server_echo_sent, &error_msg](
            rrmode::netlib::net::tcp_socket server_sock) {
            auto const io = server_sock.io_handle();
            auto buf = std::make_shared<std::array<char, 64>>();
            server_sock.async_read_some(
                loop, std::span<char>{buf->data(), buf->size()},
                [io, &loop, buf, &echo_mutex, &echo_cv, &server_echo_sent,
                 &error_msg](std::size_t n) mutable {
                    std::vector<char> out(buf->begin(), buf->begin() + static_cast<std::ptrdiff_t>(n));
                    rrmode::netlib::net::tcp_socket{io}.async_write_all(
                        loop, std::move(out),
                        [&echo_mutex, &echo_cv, &server_echo_sent]() {
                            std::scoped_lock lock{echo_mutex};
                            server_echo_sent = true;
                            echo_cv.notify_all();
                        },
                        [&error_msg](rrmode::netlib::net::net_error const& e) {
                            error_msg = e.what();
                        });
                },
                [&error_msg]() { error_msg = "server eof"; },
                [&error_msg](rrmode::netlib::net::net_error const& e) { error_msg = e.what(); });
        },
        [&error_msg](rrmode::netlib::net::net_error const& e) { error_msg = e.what(); });

    rrmode::netlib::net::tcp_socket client;
    client.async_connect(
        loop, listen_ep,
        [&]() {
            // Даём серверу время принять соединение и зарегистрировать read в epoll.
            loop.scheduler().schedule([&loop, &client, &echo_done, &echo_mutex, &echo_cv, &server_echo_sent,
                                       &response, &error_msg]() {
                client.async_write_all(
                    loop, std::vector<char>{'h', 'e', 'l', 'l', 'o'},
                    [&loop, &client, &echo_done, &echo_mutex, &echo_cv, &server_echo_sent, &response,
                     &error_msg]() {
                        std::unique_lock lock{echo_mutex};
                        if (!echo_cv.wait_for(lock, 4s, [&] { return server_echo_sent; })) {
                            error_msg = "таймаут ожидания echo";
                            return;
                        }
                        lock.unlock();

                        auto buf = std::make_shared<std::array<char, 64>>();
                        client.async_read_some(
                            loop, std::span<char>{buf->data(), buf->size()},
                            [&echo_done, &response, buf](std::size_t n) {
                                response.assign(buf->data(), n);
                                echo_done.store(true, std::memory_order_release);
                            },
                            [&error_msg]() { error_msg = "client eof"; },
                            [&error_msg](rrmode::netlib::net::net_error const& e) {
                                error_msg = e.what();
                            });
                    },
                    [&error_msg](rrmode::netlib::net::net_error const& e) { error_msg = e.what(); });
            });
        },
        [&error_msg](rrmode::netlib::net::net_error const& e) { error_msg = e.what(); });

    std::atomic<bool> stop_io{false};
    std::thread io_thread{[&loop, &stop_io] {
        while (!stop_io.load(std::memory_order_acquire)) {
            loop.run_once(10ms);
        }
    }};

    auto const deadline = std::chrono::steady_clock::now() + 8s;
    while (!echo_done.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(5ms);
    }

    stop_io.store(true, std::memory_order_release);
    loop.stop();
    io_thread.join();
    acceptor.close(loop);
    client.close(loop);
    pool.shutdown();

    CAPTURE(error_msg);
    REQUIRE(error_msg.empty());
    REQUIRE(echo_done.load(std::memory_order_acquire));
    REQUIRE(response == "hello");
}

#else

TEST_CASE("tcp sync ping loopback") { REQUIRE(true); }
TEST_CASE("tcp accept и connect loopback") { REQUIRE(true); }
TEST_CASE("tcp echo loopback") { REQUIRE(true); }

#endif
