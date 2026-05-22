#pragma once

#include <netlib/execution/coroutine.hpp>
#include <netlib/net/udp_awaitables.hpp>

#include <string>
#include <vector>

namespace rrmode::netlib::net {

/// Отправить строку как датаграмму.
inline rrmode::netlib::execution::task<void>
udp_send_string(udp_socket& socket, event_loop& loop, std::string const& message,
                endpoint const& dest, cancellation_token* token = nullptr) {
    co_await send_to_async(socket, loop, std::vector<char>{message.begin(), message.end()}, dest,
                           token);
}

/// Принять одну датаграмму в std::string (до max_len).
inline rrmode::netlib::execution::task<std::string>
udp_recv_string(udp_socket& socket, event_loop& loop, std::size_t max_len = 4096,
                cancellation_token* token = nullptr) {
    auto buf = std::vector<char>(max_len);
    auto const r = co_await recv_from_async(socket, loop, std::span<char>{buf.data(), buf.size()}, token);
    co_return std::string{buf.data(), buf.data() + static_cast<std::ptrdiff_t>(r.bytes)};
}

/// Echo одной датаграммы: recv → send обратно отправителю.
inline rrmode::netlib::execution::task<void>
udp_echo_once(udp_socket& socket, event_loop& loop, std::size_t max_len = 4096,
              cancellation_token* token = nullptr) {
    auto buf = std::vector<char>(max_len);
    auto const r =
        co_await recv_from_async(socket, loop, std::span<char>{buf.data(), buf.size()}, token);
    if (token != nullptr && token->is_cancelled()) {
        co_return;
    }
    co_await send_to_async(socket, loop,
                           std::vector<char>{buf.data(), buf.data() + static_cast<std::ptrdiff_t>(r.bytes)},
                           r.remote, token);
}

/// Цикл echo до отмены token (shutdown сервера).
inline rrmode::netlib::execution::task<void>
udp_echo_loop(udp_socket& socket, event_loop& loop, cancellation_token* token,
              std::size_t max_len = 4096) {
    while (token == nullptr || !token->is_cancelled()) {
        try {
            co_await udp_echo_once(socket, loop, max_len, token);
        } catch (net_error const&) {
            if (token != nullptr && token->is_cancelled()) {
                break;
            }
            throw;
        }
    }
}

}  // namespace rrmode::netlib::net
