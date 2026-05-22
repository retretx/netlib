#pragma once

#include <netlib/net/endpoint.hpp>
#include <netlib/net/medium/apply_socket_options.hpp>
#include <netlib/net/medium/io_context.hpp>
#include <netlib/net/medium/socket_options.hpp>
#include <netlib/net/medium/tcp_socket.hpp>
#include <netlib/net/tcp_acceptor.hpp>

#include <functional>
#include <utility>

namespace rrmode::netlib::net::medium {

/// TCP acceptor с socket_options на listener и принятых соединениях.
class tcp_acceptor {
public:
    tcp_acceptor(io_context& ctx, socket_options const& opts = {})
        : ctx_{&ctx}, opts_{opts}, acceptor_{ctx.sockets()} {}

    void open(endpoint const& ep) {
        acceptor_.open(ep);
        if (acceptor_.is_open()) {
            apply_socket_options(acceptor_.native_handle(), ctx_->sockets(), opts_);
        }
    }

    [[nodiscard]] endpoint local_endpoint() const { return acceptor_.local_endpoint(); }
    [[nodiscard]] bool is_open() const noexcept { return acceptor_.is_open(); }
    [[nodiscard]] int native_handle() const noexcept { return acceptor_.native_handle(); }

    void close() { acceptor_.close(ctx_->loop()); }

    void async_accept(std::move_only_function<void(tcp_socket)> on_accept,
                      std::move_only_function<void(net_error const&)> on_error) {
        acceptor_.async_accept(
            ctx_->loop(),
            [this, on_accept = std::move(on_accept)](net::tcp_socket peer) mutable {
                if (peer.is_open()) {
                    apply_socket_options(peer.native_handle(), ctx_->sockets(), opts_);
                }
                on_accept(tcp_socket{peer.io_handle(), *ctx_});
            },
            std::move(on_error));
    }

private:
    io_context* ctx_;
    socket_options opts_;
    net::tcp_acceptor acceptor_;
};

}  // namespace rrmode::netlib::net::medium
