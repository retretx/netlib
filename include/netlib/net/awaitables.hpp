#pragma once

#include <netlib/execution/task.hpp>
#include <netlib/net/cancellation_token.hpp>
#include <netlib/net/endpoint.hpp>
#include <netlib/net/error.hpp>
#include <netlib/net/event_loop.hpp>
#include <netlib/net/tcp_acceptor.hpp>
#include <netlib/net/tcp_socket.hpp>
#include <netlib/net/udp_socket.hpp>

#include <coroutine>
#include <exception>
#include <optional>
#include <span>
#include <vector>

namespace rrmode::netlib::net::detail {

inline bool cancel_requested(cancellation_token const* token) {
    return token != nullptr && token->is_cancelled();
}

inline void bind_cancel(cancellation_token* token, tcp_socket& socket, event_loop& loop) {
    if (token != nullptr) {
        token->on_cancel([&socket, &loop]() { socket.cancel_io(loop); });
    }
}

inline void bind_cancel(cancellation_token* token, udp_socket& socket, event_loop& loop) {
    if (token != nullptr) {
        token->on_cancel([&socket, &loop]() { socket.cancel_io(loop); });
    }
}

inline void bind_accept_cancel(cancellation_token* token, tcp_acceptor& acceptor,
                               event_loop& loop) {
    if (token != nullptr) {
        token->on_cancel([&acceptor, &loop]() { acceptor.cancel_accept(loop); });
    }
}

}  // namespace rrmode::netlib::net::detail

namespace rrmode::netlib::net {

/// co_await connect(sock, loop, endpoint)
struct connect_awaitable {
    tcp_socket& socket;
    event_loop& loop;
    endpoint ep;
    cancellation_token* token{nullptr};
    std::exception_ptr error{};

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        if (detail::cancel_requested(token)) {
            error = std::make_exception_ptr(net_error("connect_async: операция отменена"));
            h.resume();
            return;
        }
        detail::bind_cancel(token, socket, loop);
        cancellation_token* const tok = token;
        socket.async_connect(
            loop, ep,
            [h, tok]() mutable {
                if (tok != nullptr) {
                    tok->clear_on_cancel();
                }
                h.resume();
            },
            [this, h, tok](net_error const& e) {
                if (tok != nullptr) {
                    tok->clear_on_cancel();
                }
                error = std::make_exception_ptr(e);
                h.resume();
            });
    }

    void await_resume() const {
        if (error) {
            std::rethrow_exception(error);
        }
    }
};

inline connect_awaitable connect_async(tcp_socket& socket, event_loop& loop, endpoint ep,
                                       cancellation_token* token = nullptr) {
    return connect_awaitable{socket, loop, ep, token};
}

/// co_await write_all(sock, loop, data)
struct write_all_awaitable {
    tcp_socket& socket;
    event_loop& loop;
    std::vector<char> data;
    cancellation_token* token{nullptr};
    std::exception_ptr error{};

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        if (detail::cancel_requested(token)) {
            error = std::make_exception_ptr(net_error("write_all_async: операция отменена"));
            h.resume();
            return;
        }
        detail::bind_cancel(token, socket, loop);
        cancellation_token* const tok = token;
        socket.async_write_all(
            loop, std::move(data),
            [h, tok]() mutable {
                if (tok != nullptr) {
                    tok->clear_on_cancel();
                }
                h.resume();
            },
            [this, h, tok](net_error const& e) {
                if (tok != nullptr) {
                    tok->clear_on_cancel();
                }
                error = std::make_exception_ptr(e);
                h.resume();
            });
    }

    void await_resume() const {
        if (error) {
            std::rethrow_exception(error);
        }
    }
};

inline write_all_awaitable write_all_async(tcp_socket& socket, event_loop& loop,
                                           std::vector<char> data,
                                           cancellation_token* token = nullptr) {
    return write_all_awaitable{socket, loop, std::move(data), token};
}

/// Результат read_some_async после co_await.
struct read_some_result {
    std::size_t bytes{0};
};

/// co_await read_some(sock, loop, buffer)
struct read_some_awaitable {
    tcp_socket& socket;
    event_loop& loop;
    std::span<char> buffer;
    cancellation_token* token{nullptr};
    std::size_t bytes{0};
    bool eof{false};
    std::exception_ptr error{};

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        if (detail::cancel_requested(token)) {
            error = std::make_exception_ptr(net_error("read_some_async: операция отменена"));
            h.resume();
            return;
        }
        detail::bind_cancel(token, socket, loop);
        cancellation_token* const tok = token;
        socket.async_read_some(
            loop, buffer,
            [this, h, tok](std::size_t n) {
                if (tok != nullptr) {
                    tok->clear_on_cancel();
                }
                bytes = n;
                h.resume();
            },
            [this, h, tok]() {
                if (tok != nullptr) {
                    tok->clear_on_cancel();
                }
                eof = true;
                h.resume();
            },
            [this, h, tok](net_error const& e) {
                if (tok != nullptr) {
                    tok->clear_on_cancel();
                }
                error = std::make_exception_ptr(e);
                h.resume();
            });
    }

    read_some_result await_resume() const {
        if (error) {
            std::rethrow_exception(error);
        }
        if (eof) {
            throw net_error("read_some: eof");
        }
        return read_some_result{bytes};
    }

    [[nodiscard]] std::size_t size() const noexcept { return bytes; }
};

inline read_some_awaitable read_some_async(tcp_socket& socket, event_loop& loop,
                                           std::span<char> buffer,
                                           cancellation_token* token = nullptr) {
    return read_some_awaitable{socket, loop, buffer, token};
}

/// Прочитать ровно buffer.size() байт (цикл read_some).
inline rrmode::netlib::execution::task<void> read_exact_async(tcp_socket& socket, event_loop& loop,
                                                              std::span<char> buffer,
                                                              cancellation_token* token = nullptr) {
    std::size_t filled = 0;
    while (filled < buffer.size()) {
        auto const n =
            (co_await read_some_async(socket, loop, buffer.subspan(filled), token)).bytes;
        if (n == 0) {
            throw net_error("read_exact: неожиданный eof");
        }
        filled += n;
    }
}

/// co_await accept_async(acceptor, loop) → tcp_socket
struct accept_awaitable {
    tcp_acceptor& acceptor;
    event_loop& loop;
    cancellation_token* token{nullptr};
    std::optional<tcp_socket> peer;
    std::exception_ptr error{};

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        if (detail::cancel_requested(token)) {
            error = std::make_exception_ptr(net_error("accept_async: операция отменена"));
            h.resume();
            return;
        }
        detail::bind_accept_cancel(token, acceptor, loop);
        cancellation_token* const tok = token;
        acceptor.async_accept(
            loop,
            [this, h, tok](tcp_socket sock) mutable {
                if (tok != nullptr) {
                    tok->clear_on_cancel();
                }
                peer.emplace(std::move(sock));
                h.resume();
            },
            [this, h, tok](net_error const& e) {
                if (tok != nullptr) {
                    tok->clear_on_cancel();
                }
                error = std::make_exception_ptr(e);
                h.resume();
            });
    }

    tcp_socket await_resume() {
        if (error) {
            std::rethrow_exception(error);
        }
        if (!peer) {
            throw net_error("accept_async: сокет не получен");
        }
        return std::move(*peer);
    }
};

inline accept_awaitable accept_async(tcp_acceptor& acceptor, event_loop& loop,
                                       cancellation_token* token = nullptr) {
    return accept_awaitable{acceptor, loop, token};
}

}  // namespace rrmode::netlib::net
