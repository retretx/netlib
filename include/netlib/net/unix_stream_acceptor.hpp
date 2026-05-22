#pragma once

#include <netlib/net/detail/default_socket_backend.hpp>
#include <netlib/net/detail/socket_backend.hpp>
#include <netlib/net/error.hpp>
#include <netlib/net/event_loop.hpp>
#include <netlib/net/unix_endpoint.hpp>
#include <netlib/net/unix_stream_socket.hpp>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace rrmode::netlib::net {

/// UNIX domain listener: bind/listen/accept через event_loop.
class unix_stream_acceptor {
public:
    explicit unix_stream_acceptor(detail::socket_backend& sockets = detail::default_socket_backend())
        : sockets_{&sockets} {}

    unix_stream_acceptor(unix_stream_acceptor const&) = delete;
    unix_stream_acceptor& operator=(unix_stream_acceptor const&) = delete;

    ~unix_stream_acceptor() { close(); }

    void open(unix_endpoint const& ep) {
        close();
        fd_ = sockets_->create_unix_stream_socket();
        sockets_->set_reuseaddr(fd_);
        sockets_->set_non_blocking(fd_);
        sockets_->bind_unix(fd_, ep);
        sockets_->listen_socket(fd_);
        bound_path_ = sockets_->get_bound_unix_path(fd_);
    }

    [[nodiscard]] unix_endpoint local_endpoint() const { return unix_endpoint{.path = bound_path_}; }

    [[nodiscard]] bool is_open() const noexcept { return fd_ >= 0; }
    [[nodiscard]] int native_handle() const noexcept { return fd_; }

    void close() {
        if (fd_ >= 0) {
            sockets_->close(fd_);
            fd_ = -1;
        }
        bound_path_.clear();
    }

    void close(event_loop& loop) {
        if (fd_ >= 0) {
            loop.unregister_fd(fd_);
        }
        close();
    }

    void cancel_accept(event_loop& loop) {
        if (fd_ >= 0) {
            loop.unregister_fd(fd_);
        }
        fire_accept_cancel(net_error("async_accept: операция отменена"));
    }

    void async_accept(event_loop& loop,
                      std::move_only_function<void(unix_stream_socket)> on_accept,
                      std::move_only_function<void(net_error const&)> on_error) {
        if (!is_open()) {
            on_error(net_error("async_accept: acceptor не открыт"));
            return;
        }

        struct accept_callbacks {
            std::move_only_function<void(unix_stream_socket)> on_accept;
            std::move_only_function<void(net_error const&)> on_error;
        };
        auto cbs = std::make_shared<accept_callbacks>(
            accept_callbacks{std::move(on_accept), std::move(on_error)});

        begin_accept_op();
        set_cancel_handler([cbs](net_error const& e) { cbs->on_error(e); });

        try {
            int const client = sockets_->accept_nonblocking(fd_);
            if (client >= 0) {
                clear_cancel_handler();
                loop.scheduler().schedule([cbs, client, this]() mutable {
                    cbs->on_accept(unix_stream_socket{client, *sockets_});
                });
                return;
            }
        } catch (net_error const& e) {
            clear_cancel_handler();
            cbs->on_error(e);
            return;
        }

        loop.register_fd(
            fd_, detail::poll_event::readable,
            [this, &loop, cbs](detail::poll_event) mutable {
                loop.unregister_fd(fd_);
                if (accept_cancelled()) {
                    clear_cancel_handler();
                    cbs->on_error(net_error("async_accept: операция отменена"));
                    return;
                }
                try {
                    int const client_fd = sockets_->accept_nonblocking(fd_);
                    if (client_fd >= 0) {
                        clear_cancel_handler();
                        cbs->on_accept(unix_stream_socket{client_fd, *sockets_});
                    } else {
                        async_accept(loop, std::move(cbs->on_accept), std::move(cbs->on_error));
                    }
                } catch (net_error const& e) {
                    clear_cancel_handler();
                    cbs->on_error(e);
                }
            });
    }

private:
    void begin_accept_op() noexcept { accept_cancelled_.store(false, std::memory_order_release); }

    [[nodiscard]] bool accept_cancelled() const noexcept {
        return accept_cancelled_.load(std::memory_order_acquire);
    }

    void set_cancel_handler(std::function<void(net_error const&)> handler) {
        std::scoped_lock lock{cancel_mutex_};
        cancel_handler_ = std::move(handler);
    }

    void clear_cancel_handler() {
        std::scoped_lock lock{cancel_mutex_};
        cancel_handler_ = {};
    }

    void fire_accept_cancel(net_error const& err) {
        accept_cancelled_.store(true, std::memory_order_release);
        std::function<void(net_error const&)> handler;
        {
            std::scoped_lock lock{cancel_mutex_};
            handler = std::move(cancel_handler_);
            cancel_handler_ = {};
        }
        if (handler) {
            handler(err);
        }
    }

    detail::socket_backend* sockets_;
    int fd_{-1};
    std::string bound_path_;
    std::atomic<bool> accept_cancelled_{false};
    std::mutex cancel_mutex_;
    std::function<void(net_error const&)> cancel_handler_;
};

}  // namespace rrmode::netlib::net
