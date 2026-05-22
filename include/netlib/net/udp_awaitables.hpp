#pragma once

#include <netlib/execution/task.hpp>
#include <netlib/net/awaitables.hpp>
#include <netlib/net/cancellation_token.hpp>
#include <netlib/net/endpoint.hpp>
#include <netlib/net/error.hpp>
#include <netlib/net/event_loop.hpp>
#include <netlib/net/udp_socket.hpp>

#include <coroutine>
#include <exception>
#include <span>
#include <vector>

namespace rrmode::netlib::net {

/// Результат recv_from_async.
struct udp_recv_result {
    endpoint remote;
    std::size_t bytes{0};
};

/// co_await send_to_async(sock, loop, data, dest)
struct send_to_awaitable {
    udp_socket& socket;
    event_loop& loop;
    std::vector<char> data;
    endpoint dest;
    cancellation_token* token{nullptr};
    std::exception_ptr error{};

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        if (detail::cancel_requested(token)) {
            error = std::make_exception_ptr(net_error("send_to_async: операция отменена"));
            h.resume();
            return;
        }
        detail::bind_cancel(token, socket, loop);
        cancellation_token* const tok = token;
        socket.async_send_to(
            loop, std::move(data), dest,
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

inline send_to_awaitable send_to_async(udp_socket& socket, event_loop& loop, std::vector<char> data,
                                      endpoint const& dest, cancellation_token* token = nullptr) {
    return send_to_awaitable{socket, loop, std::move(data), dest, token};
}

/// co_await recv_from_async(sock, loop, buffer) → udp_recv_result
struct recv_from_awaitable {
    udp_socket& socket;
    event_loop& loop;
    std::span<char> buffer;
    cancellation_token* token{nullptr};
    endpoint remote{};
    std::size_t bytes{0};
    std::exception_ptr error{};

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        if (detail::cancel_requested(token)) {
            error = std::make_exception_ptr(net_error("recv_from_async: операция отменена"));
            h.resume();
            return;
        }
        detail::bind_cancel(token, socket, loop);
        cancellation_token* const tok = token;
        socket.async_recv_from(
            loop, buffer,
            [this, h, tok](endpoint ep, std::size_t n) {
                if (tok != nullptr) {
                    tok->clear_on_cancel();
                }
                remote = ep;
                bytes = n;
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

    udp_recv_result await_resume() const {
        if (error) {
            std::rethrow_exception(error);
        }
        return udp_recv_result{.remote = remote, .bytes = bytes};
    }
};

inline recv_from_awaitable recv_from_async(udp_socket& socket, event_loop& loop,
                                           std::span<char> buffer,
                                           cancellation_token* token = nullptr) {
    return recv_from_awaitable{socket, loop, buffer, token};
}

}  // namespace rrmode::netlib::net
