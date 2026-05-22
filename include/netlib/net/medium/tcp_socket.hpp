#pragma once

#include <netlib/net/endpoint.hpp>
#include <netlib/net/medium/apply_socket_options.hpp>
#include <netlib/net/medium/io_context.hpp>
#include <netlib/net/medium/socket_options.hpp>
#include <netlib/net/tcp_socket.hpp>

#include <functional>
#include <span>
#include <vector>

namespace rrmode::netlib::net::medium {

namespace net_detail = ::rrmode::netlib::net::detail;

/// TCP-сокет с применением socket_options после создания fd.
class tcp_socket {
public:
    tcp_socket(io_context& ctx, socket_options const& opts = {})
        : ctx_{&ctx}, opts_{opts}, socket_{ctx.sockets()} {}

    tcp_socket(int fd, io_context& ctx, socket_options const& opts = {})
        : ctx_{&ctx}, opts_{opts}, socket_{fd, ctx.sockets()} {
        apply_socket_options(fd, ctx.sockets(), opts_);
    }

    explicit tcp_socket(std::shared_ptr<net_detail::socket_handle> handle, io_context& ctx)
        : ctx_{&ctx}, socket_{std::move(handle)} {}

    tcp_socket(tcp_socket const&) = delete;
    tcp_socket& operator=(tcp_socket const&) = delete;

    tcp_socket(tcp_socket&&) noexcept = default;
    tcp_socket& operator=(tcp_socket&&) noexcept = default;

    [[nodiscard]] net::tcp_socket& underlying() noexcept { return socket_; }
    [[nodiscard]] net::tcp_socket const& underlying() const noexcept { return socket_; }

    [[nodiscard]] bool is_open() const noexcept { return socket_.is_open(); }
    [[nodiscard]] int native_handle() const noexcept { return socket_.native_handle(); }

    [[nodiscard]] std::shared_ptr<net_detail::socket_handle> io_handle() const {
        return socket_.io_handle();
    }

    void close() { socket_.close(ctx_->loop()); }

    void async_connect(endpoint const& ep, std::move_only_function<void()> on_connected,
                       std::move_only_function<void(net_error const&)> on_error) {
        socket_.async_connect(
            ctx_->loop(), ep,
            [this, on_connected = std::move(on_connected)]() mutable {
                if (socket_.is_open()) {
                    apply_socket_options(socket_.native_handle(), ctx_->sockets(), opts_);
                }
                on_connected();
            },
            std::move(on_error));
    }

    void async_read_some(std::span<char> buffer,
                         std::move_only_function<void(std::size_t)> on_data,
                         std::move_only_function<void()> on_eof,
                         std::move_only_function<void(net_error const&)> on_error) {
        socket_.async_read_some(ctx_->loop(), buffer, std::move(on_data), std::move(on_eof),
                                std::move(on_error));
    }

    void async_write_all(std::vector<char> data, std::move_only_function<void()> on_done,
                         std::move_only_function<void(net_error const&)> on_error) {
        socket_.async_write_all(ctx_->loop(), std::move(data), std::move(on_done),
                                std::move(on_error));
    }

private:
    io_context* ctx_;
    socket_options opts_;
    net::tcp_socket socket_;
};

}  // namespace rrmode::netlib::net::medium
