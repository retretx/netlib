#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/event_loop.hpp>
#include <netlib/net/unix_stream_acceptor.hpp>
#include <netlib/net/unix_stream_socket.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#if !defined(_WIN32)
#include <unistd.h>
#endif
#include <vector>

using namespace std::chrono_literals;

#if defined(_WIN32)
TEST_CASE("unix stream loopback пропуск на Windows", "[unix][integration]") {
    SUCCEED("AF_UNIX stream не поддерживается в win_socket_backend");
}
#else

namespace {

std::string unique_sock_path() {
    return std::string{"/tmp/netlib_unix_"} + std::to_string(static_cast<unsigned>(::getpid())) + ".sock";
}

}  // namespace

TEST_CASE("unix stream echo loopback", "[unix][integration]") {
    auto const path = unique_sock_path();
    rrmode::netlib::execution::thread_pool pool{2};
    rrmode::netlib::execution::scheduler sched{pool};
    rrmode::netlib::net::event_loop loop{sched};

    rrmode::netlib::net::unix_stream_acceptor acc;
    acc.open({.path = path});

    std::atomic<bool> done{false};
    std::atomic<bool> stop_io{false};
    std::string err;

    std::thread io_thread{[&loop, &stop_io] {
        while (!stop_io.load(std::memory_order_acquire)) {
            loop.run_once(5ms);
        }
    }};

    acc.async_accept(
        loop,
        [&](rrmode::netlib::net::unix_stream_socket peer) {
            char buf[64]{};
            peer.async_read_some(
                loop, buf,
                [&](std::size_t n) {
                    peer.async_write_all(
                        loop, std::vector<char>(buf, buf + n),
                        [&done] { done.store(true, std::memory_order_release); },
                        [&](rrmode::netlib::net::net_error const& e) { err = e.what(); });
                },
                [] {},
                [&](rrmode::netlib::net::net_error const& e) { err = e.what(); });
        },
        [&](rrmode::netlib::net::net_error const& e) { err = e.what(); });

    rrmode::netlib::net::unix_stream_socket client;
    client.async_connect(
        loop, {.path = path},
        [&] {
            client.async_write_all(
                loop, std::vector<char>{'p', 'i', 'n', 'g'},
                [] {},
                [&](rrmode::netlib::net::net_error const& e) { err = e.what(); });
        },
        [&](rrmode::netlib::net::net_error const& e) { err = e.what(); });

    for (int i = 0; i < 200 && !done.load() && err.empty(); ++i) {
        std::this_thread::sleep_for(5ms);
    }

    stop_io.store(true, std::memory_order_release);
    io_thread.join();

    REQUIRE(err.empty());
    REQUIRE(done.load());
    acc.close();
    client.close();
    std::remove(path.c_str());
}

#endif
