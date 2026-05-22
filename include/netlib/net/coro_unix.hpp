#pragma once

#include <netlib/execution/coroutine.hpp>
#include <netlib/net/unix_awaitables.hpp>
#include <netlib/net/unix_stream_acceptor.hpp>
#include <netlib/net/unix_stream_socket.hpp>

#include <memory>
#include <string>
#include <vector>

namespace rrmode::netlib::net {

inline rrmode::netlib::execution::task<std::string>
read_unix_string_async(unix_stream_socket& socket, event_loop& loop, std::size_t max_len = 4096) {
    auto buf = std::make_shared<std::vector<char>>(max_len);
    auto const n =
        (co_await read_some_unix_async(socket, loop, std::span<char>{buf->data(), buf->size()}))
            .bytes;
    co_return std::string{buf->data(), buf->data() + static_cast<std::ptrdiff_t>(n)};
}

inline rrmode::netlib::execution::task<void> unix_echo_peer(unix_stream_socket peer,
                                                            event_loop& loop) {
    auto data = co_await read_unix_string_async(peer, loop);
    co_await write_all_unix_async(peer, loop, std::vector<char>{data.begin(), data.end()});
    peer.close(loop);
}

inline rrmode::netlib::execution::task<void>
unix_serve_echo_once(unix_stream_acceptor& acceptor, event_loop& loop) {
    auto peer = co_await accept_unix_async(acceptor, loop);
    co_await unix_echo_peer(std::move(peer), loop);
}

}  // namespace rrmode::netlib::net
