#pragma once

#include <netlib/execution/timeout_error.hpp>
#include <netlib/execution/when_any.hpp>
#include <netlib/net/awaitables.hpp>
#include <netlib/net/cancellation_token.hpp>
#include <netlib/net/event_loop.hpp>
#include <netlib/net/tcp_acceptor.hpp>
#include <netlib/net/tcp_socket.hpp>
#include <netlib/net/udp_awaitables.hpp>
#include <netlib/net/udp_socket.hpp>
#include <netlib/net/unix_awaitables.hpp>
#include <netlib/net/unix_endpoint.hpp>
#include <netlib/net/unix_stream_acceptor.hpp>
#include <netlib/net/unix_stream_socket.hpp>

#include <chrono>
#include <span>

namespace rrmode::netlib::net {

using timeout_error = execution::timeout_error;

namespace detail {

inline void cleanup_after_timeout(tcp_socket& socket, event_loop& loop) {
    socket.close(loop);
}

inline void cleanup_after_timeout(udp_socket& socket, event_loop& loop) {
    socket.close(loop);
}

inline void cleanup_after_timeout(unix_stream_socket& socket, event_loop& loop) {
    socket.close(loop);
}

inline execution::task<void> connect_task(tcp_socket& socket, event_loop& loop, endpoint ep,
                                          cancellation_token* token) {
    co_await connect_async(socket, loop, ep, token);
}

inline execution::task<std::size_t> read_some_task(tcp_socket& socket, event_loop& loop,
                                                   std::span<char> buffer,
                                                   cancellation_token* token) {
    co_return (co_await read_some_async(socket, loop, buffer, token)).bytes;
}

inline execution::task<void> read_exact_task(tcp_socket& socket, event_loop& loop,
                                             std::span<char> buffer,
                                             cancellation_token* token) {
    co_await read_exact_async(socket, loop, buffer, token);
}

inline execution::task<tcp_socket> accept_task(tcp_acceptor& acceptor, event_loop& loop,
                                               cancellation_token* token) {
    co_return co_await accept_async(acceptor, loop, token);
}

inline execution::task<void> send_to_task(udp_socket& socket, event_loop& loop,
                                          std::vector<char> data, endpoint dest,
                                          cancellation_token* token) {
    co_await send_to_async(socket, loop, std::move(data), dest, token);
}

inline execution::task<udp_recv_result> recv_from_task(udp_socket& socket, event_loop& loop,
                                                       std::span<char> buffer,
                                                       cancellation_token* token) {
    co_return co_await recv_from_async(socket, loop, buffer, token);
}

inline execution::task<void> connect_unix_task(unix_stream_socket& socket, event_loop& loop,
                                               unix_endpoint ep, cancellation_token* token) {
    co_await connect_unix_async(socket, loop, ep, token);
}

inline execution::task<std::size_t>
read_some_unix_task(unix_stream_socket& socket, event_loop& loop, std::span<char> buffer,
                    cancellation_token* token) {
    co_return (co_await read_some_unix_async(socket, loop, buffer, token)).bytes;
}

inline execution::task<void> read_exact_unix_task(unix_stream_socket& socket, event_loop& loop,
                                                  std::span<char> buffer,
                                                  cancellation_token* token) {
    co_await read_exact_unix_async(socket, loop, buffer, token);
}

inline execution::task<unix_stream_socket> accept_unix_task(unix_stream_acceptor& acceptor,
                                                            event_loop& loop,
                                                            cancellation_token* token) {
    co_return co_await accept_unix_async(acceptor, loop, token);
}

}  // namespace detail

/// connect с лимитом; при timeout — cancel token, cancel_io, close.
inline execution::task<void>
connect_with_timeout(tcp_socket& socket, event_loop& loop, endpoint ep,
                     execution::scheduler& sched, std::chrono::milliseconds limit,
                     cancellation_source* cancel = nullptr) {
    cancellation_source owned;
    cancellation_source* const src = cancel != nullptr ? cancel : &owned;
    try {
        co_await execution::with_timeout(
            sched, detail::connect_task(socket, loop, ep, &src->token()), limit);
    } catch (timeout_error const&) {
        src->cancel();
        detail::cleanup_after_timeout(socket, loop);
        throw;
    }
}

/// read_some с лимитом; при timeout — cancel и close.
inline execution::task<std::size_t>
read_some_with_timeout(tcp_socket& socket, event_loop& loop, std::span<char> buffer,
                       execution::scheduler& sched, std::chrono::milliseconds limit,
                       cancellation_source* cancel = nullptr) {
    cancellation_source owned;
    cancellation_source* const src = cancel != nullptr ? cancel : &owned;
    try {
        co_return co_await execution::with_timeout(
            sched, detail::read_some_task(socket, loop, buffer, &src->token()), limit);
    } catch (timeout_error const&) {
        src->cancel();
        detail::cleanup_after_timeout(socket, loop);
        throw;
    }
}

/// read_exact с лимитом; при timeout — cancel и close.
inline execution::task<void>
read_exact_with_timeout(tcp_socket& socket, event_loop& loop, std::span<char> buffer,
                        execution::scheduler& sched, std::chrono::milliseconds limit,
                        cancellation_source* cancel = nullptr) {
    cancellation_source owned;
    cancellation_source* const src = cancel != nullptr ? cancel : &owned;
    try {
        co_await execution::with_timeout(
            sched, detail::read_exact_task(socket, loop, buffer, &src->token()), limit);
    } catch (timeout_error const&) {
        src->cancel();
        detail::cleanup_after_timeout(socket, loop);
        throw;
    }
}

/// accept с лимитом; при timeout — cancel_accept.
inline execution::task<tcp_socket>
accept_with_timeout(tcp_acceptor& acceptor, event_loop& loop, execution::scheduler& sched,
                    std::chrono::milliseconds limit, cancellation_source* cancel = nullptr) {
    cancellation_source owned;
    cancellation_source* const src = cancel != nullptr ? cancel : &owned;
    try {
        co_return co_await execution::with_timeout(
            sched, detail::accept_task(acceptor, loop, &src->token()), limit);
    } catch (timeout_error const&) {
        src->cancel();
        acceptor.cancel_accept(loop);
        throw;
    }
}

/// sendto с лимитом; при timeout — cancel и close.
inline execution::task<void>
send_to_with_timeout(udp_socket& socket, event_loop& loop, std::vector<char> data,
                     endpoint const& dest, execution::scheduler& sched,
                     std::chrono::milliseconds limit, cancellation_source* cancel = nullptr) {
    cancellation_source owned;
    cancellation_source* const src = cancel != nullptr ? cancel : &owned;
    try {
        co_await execution::with_timeout(
            sched,
            detail::send_to_task(socket, loop, std::move(data), dest, &src->token()), limit);
    } catch (timeout_error const&) {
        src->cancel();
        detail::cleanup_after_timeout(socket, loop);
        throw;
    }
}

/// recvfrom с лимитом; при timeout — cancel и close.
inline execution::task<udp_recv_result>
recv_from_with_timeout(udp_socket& socket, event_loop& loop, std::span<char> buffer,
                       execution::scheduler& sched, std::chrono::milliseconds limit,
                       cancellation_source* cancel = nullptr) {
    cancellation_source owned;
    cancellation_source* const src = cancel != nullptr ? cancel : &owned;
    try {
        co_return co_await execution::with_timeout(
            sched, detail::recv_from_task(socket, loop, buffer, &src->token()), limit);
    } catch (timeout_error const&) {
        src->cancel();
        detail::cleanup_after_timeout(socket, loop);
        throw;
    }
}

inline execution::task<void>
connect_unix_with_timeout(unix_stream_socket& socket, event_loop& loop, unix_endpoint ep,
                          execution::scheduler& sched, std::chrono::milliseconds limit,
                          cancellation_source* cancel = nullptr) {
    cancellation_source owned;
    cancellation_source* const src = cancel != nullptr ? cancel : &owned;
    try {
        co_await execution::with_timeout(
            sched, detail::connect_unix_task(socket, loop, ep, &src->token()), limit);
    } catch (timeout_error const&) {
        src->cancel();
        detail::cleanup_after_timeout(socket, loop);
        throw;
    }
}

inline execution::task<std::size_t>
read_some_unix_with_timeout(unix_stream_socket& socket, event_loop& loop, std::span<char> buffer,
                            execution::scheduler& sched, std::chrono::milliseconds limit,
                            cancellation_source* cancel = nullptr) {
    cancellation_source owned;
    cancellation_source* const src = cancel != nullptr ? cancel : &owned;
    try {
        co_return co_await execution::with_timeout(
            sched, detail::read_some_unix_task(socket, loop, buffer, &src->token()), limit);
    } catch (timeout_error const&) {
        src->cancel();
        detail::cleanup_after_timeout(socket, loop);
        throw;
    }
}

inline execution::task<void>
read_exact_unix_with_timeout(unix_stream_socket& socket, event_loop& loop, std::span<char> buffer,
                             execution::scheduler& sched, std::chrono::milliseconds limit,
                             cancellation_source* cancel = nullptr) {
    cancellation_source owned;
    cancellation_source* const src = cancel != nullptr ? cancel : &owned;
    try {
        co_await execution::with_timeout(
            sched, detail::read_exact_unix_task(socket, loop, buffer, &src->token()), limit);
    } catch (timeout_error const&) {
        src->cancel();
        detail::cleanup_after_timeout(socket, loop);
        throw;
    }
}

inline execution::task<unix_stream_socket>
accept_unix_with_timeout(unix_stream_acceptor& acceptor, event_loop& loop,
                         execution::scheduler& sched, std::chrono::milliseconds limit,
                         cancellation_source* cancel = nullptr) {
    cancellation_source owned;
    cancellation_source* const src = cancel != nullptr ? cancel : &owned;
    try {
        co_return co_await execution::with_timeout(
            sched, detail::accept_unix_task(acceptor, loop, &src->token()), limit);
    } catch (timeout_error const&) {
        src->cancel();
        acceptor.cancel_accept(loop);
        throw;
    }
}

}  // namespace rrmode::netlib::net
