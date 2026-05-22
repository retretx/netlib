#pragma once

#include <netlib/execution/coroutine.hpp>
#include <netlib/net/cancellation_token.hpp>
#include <netlib/net/coro_unix.hpp>
#include <netlib/net/timeout.hpp>
#include <netlib/net/unix_awaitables.hpp>
#include <netlib/net/unix_stream_acceptor.hpp>
#include <netlib/net/unix_stream_socket.hpp>

#include <chrono>
#include <string>
#include <vector>

namespace rrmode::netlib::net {

inline rrmode::netlib::execution::task<void>
unix_connect(unix_stream_socket& socket, event_loop& loop, unix_endpoint ep,
             cancellation_token* token = nullptr) {
    co_await connect_unix_async(socket, loop, ep, token);
}

inline rrmode::netlib::execution::task<void>
unix_connect(unix_stream_socket& socket, event_loop& loop, unix_endpoint ep,
             execution::scheduler& sched, std::chrono::milliseconds limit,
             cancellation_source* cancel = nullptr) {
    co_await connect_unix_with_timeout(socket, loop, ep, sched, limit, cancel);
}

inline rrmode::netlib::execution::task<std::string>
read_unix_string_with_timeout(unix_stream_socket& socket, event_loop& loop,
                              execution::scheduler& sched, std::chrono::milliseconds limit,
                              cancellation_source* cancel = nullptr, std::size_t max_len = 4096) {
    auto buf = std::vector<char>(max_len);
    auto const n = co_await read_some_unix_with_timeout(
        socket, loop, std::span<char>{buf.data(), buf.size()}, sched, limit, cancel);
    co_return std::string{buf.data(), buf.data() + static_cast<std::ptrdiff_t>(n)};
}

inline rrmode::netlib::execution::task<void>
unix_echo_server_loop(unix_stream_acceptor& acceptor, event_loop& loop,
                      cancellation_token* token) {
    while (token == nullptr || !token->is_cancelled()) {
        auto peer = co_await accept_unix_async(acceptor, loop, token);
        if (token != nullptr && token->is_cancelled()) {
            peer.close(loop);
            break;
        }
        co_await unix_echo_peer(std::move(peer), loop);
    }
}

}  // namespace rrmode::netlib::net
