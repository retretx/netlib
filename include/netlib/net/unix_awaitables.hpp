#pragma once

#include <netlib/execution/task.hpp>
#include <netlib/net/awaitables.hpp>
#include <netlib/net/cancellation_token.hpp>
#include <netlib/net/error.hpp>
#include <netlib/net/event_loop.hpp>
#include <netlib/net/unix_endpoint.hpp>
#include <netlib/net/unix_stream_acceptor.hpp>
#include <netlib/net/unix_stream_socket.hpp>

#include <coroutine>
#include <exception>
#include <optional>
#include <span>
#include <vector>

namespace rrmode::netlib::net::detail {

inline void bind_cancel(cancellation_token* token, unix_stream_socket& socket, event_loop& loop) {
    if (token != nullptr) {
        token->on_cancel([&socket, &loop]() { socket.cancel_io(loop); });
    }
}

inline void bind_accept_cancel(cancellation_token* token, unix_stream_acceptor& acceptor,
                               event_loop& loop) {
    if (token != nullptr) {
        token->on_cancel([&acceptor, &loop]() { acceptor.cancel_accept(loop); });
    }
}

}  // namespace rrmode::netlib::net::detail

namespace rrmode::netlib::net {

struct connect_unix_awaitable {
    unix_stream_socket& socket;
    event_loop& loop;
    unix_endpoint ep;
    cancellation_token* token{nullptr};
    std::exception_ptr error{};

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        if (detail::cancel_requested(token)) {
            error = std::make_exception_ptr(net_error("connect_unix_async: операция отменена"));
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

inline connect_unix_awaitable connect_unix_async(unix_stream_socket& socket, event_loop& loop,
                                                 unix_endpoint ep,
                                                 cancellation_token* token = nullptr) {
    return connect_unix_awaitable{socket, loop, ep, token};
}

struct write_all_unix_awaitable {
    unix_stream_socket& socket;
    event_loop& loop;
    std::vector<char> data;
    cancellation_token* token{nullptr};
    std::exception_ptr error{};

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        if (detail::cancel_requested(token)) {
            error = std::make_exception_ptr(net_error("write_all_unix_async: операция отменена"));
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

inline write_all_unix_awaitable write_all_unix_async(unix_stream_socket& socket, event_loop& loop,
                                                     std::vector<char> data,
                                                     cancellation_token* token = nullptr) {
    return write_all_unix_awaitable{socket, loop, std::move(data), token};
}

struct read_some_unix_awaitable {
    unix_stream_socket& socket;
    event_loop& loop;
    std::span<char> buffer;
    cancellation_token* token{nullptr};
    std::size_t bytes{0};
    bool eof{false};
    std::exception_ptr error{};

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        if (detail::cancel_requested(token)) {
            error = std::make_exception_ptr(net_error("read_some_unix_async: операция отменена"));
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
            throw net_error("read_some_unix: eof");
        }
        return read_some_result{bytes};
    }
};

inline read_some_unix_awaitable read_some_unix_async(unix_stream_socket& socket, event_loop& loop,
                                                     std::span<char> buffer,
                                                     cancellation_token* token = nullptr) {
    return read_some_unix_awaitable{socket, loop, buffer, token};
}

inline rrmode::netlib::execution::task<void>
read_exact_unix_async(unix_stream_socket& socket, event_loop& loop, std::span<char> buffer,
                      cancellation_token* token = nullptr) {
    std::size_t filled = 0;
    while (filled < buffer.size()) {
        auto const n =
            (co_await read_some_unix_async(socket, loop, buffer.subspan(filled), token)).bytes;
        if (n == 0) {
            throw net_error("read_exact_unix: неожиданный eof");
        }
        filled += n;
    }
}

struct accept_unix_awaitable {
    unix_stream_acceptor& acceptor;
    event_loop& loop;
    cancellation_token* token{nullptr};
    std::optional<unix_stream_socket> peer;
    std::exception_ptr error{};

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        if (detail::cancel_requested(token)) {
            error = std::make_exception_ptr(net_error("accept_unix_async: операция отменена"));
            h.resume();
            return;
        }
        detail::bind_accept_cancel(token, acceptor, loop);
        cancellation_token* const tok = token;
        acceptor.async_accept(
            loop,
            [this, h, tok](unix_stream_socket sock) mutable {
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

    unix_stream_socket await_resume() {
        if (error) {
            std::rethrow_exception(error);
        }
        if (!peer) {
            throw net_error("accept_unix_async: сокет не получен");
        }
        return std::move(*peer);
    }
};

inline accept_unix_awaitable accept_unix_async(unix_stream_acceptor& acceptor, event_loop& loop,
                                               cancellation_token* token = nullptr) {
    return accept_unix_awaitable{acceptor, loop, token};
}

}  // namespace rrmode::netlib::net
