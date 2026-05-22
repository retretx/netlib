#pragma once

#include <netlib/execution/coroutine.hpp>
#include <netlib/net/cancellation_token.hpp>
#include <netlib/net/coro_tcp.hpp>
#include <netlib/net/timeout.hpp>

#include <chrono>
#include <string>
#include <vector>

namespace rrmode::netlib::net {

/// Подключиться (coro-обёртка).
inline rrmode::netlib::execution::task<void>
tcp_connect(tcp_socket& socket, event_loop& loop, endpoint ep,
            cancellation_token* token = nullptr) {
    co_await connect_async(socket, loop, ep, token);
}

inline rrmode::netlib::execution::task<void>
tcp_connect(tcp_socket& socket, event_loop& loop, endpoint ep, execution::scheduler& sched,
            std::chrono::milliseconds limit, cancellation_source* cancel = nullptr) {
    co_await connect_with_timeout(socket, loop, ep, sched, limit, cancel);
}

/// read_some → std::string с лимитом времени.
inline rrmode::netlib::execution::task<std::string>
read_string_with_timeout(tcp_socket& socket, event_loop& loop, execution::scheduler& sched,
                         std::chrono::milliseconds limit, cancellation_source* cancel = nullptr,
                         std::size_t max_len = 4096) {
    auto buf = std::vector<char>(max_len);
    auto const n = co_await read_some_with_timeout(socket, loop, std::span<char>{buf.data(), buf.size()},
                                                   sched, limit, cancel);
    co_return std::string{buf.data(), buf.data() + static_cast<std::ptrdiff_t>(n)};
}

/// Echo-сессия: read → write → close.
inline rrmode::netlib::execution::task<void>
tcp_echo_session(tcp_socket peer, event_loop& loop, cancellation_token* token = nullptr) {
    co_await tcp_echo_peer(std::move(peer), loop);
    if (token != nullptr && token->is_cancelled()) {
        throw net_error("tcp_echo_session: отменено");
    }
}

/// Один accept + echo.
inline rrmode::netlib::execution::task<void>
tcp_accept_echo_once(tcp_acceptor& acceptor, event_loop& loop,
                     cancellation_token* token = nullptr) {
    co_await tcp_serve_echo_once(acceptor, loop);
    (void)token;
}

/// Цикл accept→echo пока не отменён token (типично — shutdown сервера).
inline rrmode::netlib::execution::task<void>
tcp_echo_server_loop(tcp_acceptor& acceptor, event_loop& loop, cancellation_token* token) {
    while (token == nullptr || !token->is_cancelled()) {
        auto peer = co_await accept_async(acceptor, loop, token);
        if (token != nullptr && token->is_cancelled()) {
            peer.close(loop);
            break;
        }
        co_await tcp_echo_peer(std::move(peer), loop);
    }
}

/// Запустить task; при исключении вызывает source.cancel() и пробрасывает дальше.
template<typename T>
inline rrmode::netlib::execution::task<T>
run_until_cancelled(cancellation_source& source, rrmode::netlib::execution::task<T> work) {
    try {
        co_return co_await std::move(work);
    } catch (...) {
        source.cancel();
        throw;
    }
}

inline rrmode::netlib::execution::task<void>
run_until_cancelled(cancellation_source& source, rrmode::netlib::execution::task<void> work) {
    try {
        co_await std::move(work);
    } catch (...) {
        source.cancel();
        throw;
    }
}

}  // namespace rrmode::netlib::net
