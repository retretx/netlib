#pragma once

#include <netlib/net/detail/socket_backend.hpp>
#include <netlib/net/error.hpp>

#include <atomic>
#include <functional>

namespace rrmode::netlib::net::detail {

/// Разделяемое владение fd: удерживает дескриптор открытым, пока идут async-операции.
struct socket_handle {
    int fd{-1};
    socket_backend* sockets{nullptr};

    socket_handle() = default;

    socket_handle(int fd, socket_backend& socks) : fd{fd}, sockets{&socks} {}

    socket_handle(socket_handle const&) = delete;
    socket_handle& operator=(socket_handle const&) = delete;

    ~socket_handle() { close_fd(); }

    void close_fd() {
        if (fd >= 0 && sockets != nullptr) {
            sockets->close(fd);
            fd = -1;
        }
    }

    [[nodiscard]] bool is_open() const noexcept { return fd >= 0; }
    [[nodiscard]] int native_handle() const noexcept { return fd; }

    void begin_io() noexcept { io_cancelled_.store(false, std::memory_order_release); }

    void request_cancel() noexcept { io_cancelled_.store(true, std::memory_order_release); }

    [[nodiscard]] bool is_io_cancelled() const noexcept {
        return io_cancelled_.load(std::memory_order_acquire);
    }

    void set_cancel_handler(std::function<void(net_error const&)> handler) {
        cancel_handler_ = std::move(handler);
    }

    void clear_cancel_handler() { cancel_handler_ = {}; }

    void fire_cancel(net_error const& err) {
        if (cancel_handler_) {
            cancel_handler_(err);
            cancel_handler_ = {};
        }
    }

private:
    std::atomic<bool> io_cancelled_{false};
    std::function<void(net_error const&)> cancel_handler_;
};

}  // namespace rrmode::netlib::net::detail
