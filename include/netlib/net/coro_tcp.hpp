#pragma once

#include <netlib/execution/coroutine.hpp>
#include <netlib/net/awaitables.hpp>
#include <netlib/net/tcp_acceptor.hpp>
#include <netlib/net/tcp_socket.hpp>

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace rrmode::netlib::net {

/// Прочитать ровно len байт в std::vector.
inline rrmode::netlib::execution::task<std::vector<char>> read_exact_vec_async(tcp_socket& socket,
                                                                               event_loop& loop,
                                                                               std::size_t len) {
    auto buf = std::make_shared<std::vector<char>>(len);
    co_await read_exact_async(socket, loop, std::span<char>{buf->data(), buf->size()});
    co_return std::move(*buf);
}

/// Прочитать до max байт в std::string (один read_some).
inline rrmode::netlib::execution::task<std::string> read_string_async(tcp_socket& socket,
                                                                    event_loop& loop,
                                                                    std::size_t max_len = 4096) {
    auto buf = std::make_shared<std::vector<char>>(max_len);
    auto const n = (co_await read_some_async(socket, loop, std::span<char>{buf->data(), buf->size()}))
                       .bytes;
    co_return std::string{buf->data(), buf->data() + static_cast<std::ptrdiff_t>(n)};
}

/// Принять соединение, прочитать, отправить echo тех же байт.
inline rrmode::netlib::execution::task<void> tcp_echo_peer(tcp_socket peer, event_loop& loop) {
    auto data = co_await read_string_async(peer, loop);
    co_await write_all_async(peer, loop, std::vector<char>{data.begin(), data.end()});
    peer.close(loop);
}

/// Один цикл accept → echo для acceptor.
inline rrmode::netlib::execution::task<void> tcp_serve_echo_once(tcp_acceptor& acceptor,
                                                                 event_loop& loop) {
    auto peer = co_await accept_async(acceptor, loop);
    co_await tcp_echo_peer(std::move(peer), loop);
}

}  // namespace rrmode::netlib::net
