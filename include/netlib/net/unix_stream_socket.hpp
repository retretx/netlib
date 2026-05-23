#pragma once

#include <netlib/net/detail/default_socket_backend.hpp>
#include <netlib/net/detail/socket_backend.hpp>
#include <netlib/net/detail/socket_handle.hpp>
#include <netlib/net/error.hpp>
#include <netlib/net/event_loop.hpp>
#include <netlib/net/unix_endpoint.hpp>

#include <cstring>
#include <netlib/detail/move_only_function.hpp>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace rrmode::netlib::net {

/// UNIX domain stream-сокет (AF_UNIX SOCK_STREAM) с async I/O через event_loop.
class unix_stream_socket {
public:
    explicit unix_stream_socket(detail::socket_backend& sockets = detail::default_socket_backend())
        : sockets_{&sockets} {}

    unix_stream_socket(int fd, detail::socket_backend& sockets = detail::default_socket_backend())
        : sockets_{&sockets} {
        attach(fd);
    }

    explicit unix_stream_socket(std::shared_ptr<detail::socket_handle> handle)
        : handle_{std::move(handle)} {
        if (handle_ && handle_->sockets != nullptr) {
            sockets_ = handle_->sockets;
        }
    }

    unix_stream_socket(unix_stream_socket const&) = delete;
    unix_stream_socket& operator=(unix_stream_socket const&) = delete;

    unix_stream_socket(unix_stream_socket&& other) noexcept
        : sockets_{other.sockets_}, handle_{std::move(other.handle_)} {}

    unix_stream_socket& operator=(unix_stream_socket&& other) noexcept {
        if (this != &other) {
            release_if_last_owner();
            sockets_ = other.sockets_;
            handle_ = std::move(other.handle_);
        }
        return *this;
    }

    ~unix_stream_socket() { release_if_last_owner(); }

    [[nodiscard]] int native_handle() const noexcept {
        return handle_ ? handle_->native_handle() : -1;
    }

    [[nodiscard]] bool is_open() const noexcept { return handle_ && handle_->is_open(); }

    [[nodiscard]] std::shared_ptr<detail::socket_handle> io_handle() const { return handle_; }

    void close() {
        if (handle_) {
            handle_->close_fd();
        }
    }

    void close(event_loop& loop) {
        if (handle_ && handle_->is_open()) {
            loop.unregister_fd(handle_->native_handle());
        }
        close();
    }

    void cancel_io(event_loop& loop) {
        if (!handle_) {
            return;
        }
        handle_->request_cancel();
        if (handle_->is_open()) {
            loop.unregister_fd(handle_->native_handle());
        }
        handle_->fire_cancel(net_error("операция отменена"));
    }

    [[nodiscard]] bool is_io_cancelled() const noexcept {
        return handle_ && handle_->is_io_cancelled();
    }

    void async_connect(event_loop& loop, unix_endpoint const& ep,
                       std::move_only_function<void()> on_connected,
                       std::move_only_function<void(net_error const&)> on_error) {
        if (!on_connected || !on_error) {
            throw net_error("async_connect: колбэки обязательны");
        }
        release_if_last_owner();
        handle_.reset();

        int const fd = sockets_->create_unix_stream_socket();
        sockets_->set_non_blocking(fd);
        attach(fd);
        handle_->begin_io();

        auto h = handle_;
        struct connect_callbacks {
            std::move_only_function<void()> on_connected;
            std::move_only_function<void(net_error const&)> on_error;
        };
        auto cbs = std::make_shared<connect_callbacks>(
            connect_callbacks{std::move(on_connected), std::move(on_error)});
        h->set_cancel_handler([cbs](net_error const& e) { cbs->on_error(e); });

        int const rc = sockets_->start_connect_unix(h->native_handle(), ep);
        if (rc == 0) {
            loop.scheduler().schedule([h, cbs]() mutable {
                h->clear_cancel_handler();
                if (h->is_io_cancelled()) {
                    cbs->on_error(net_error("async_connect: операция отменена"));
                    return;
                }
                std::move(cbs->on_connected)();
            });
            return;
        }

        loop.register_fd(
            h->native_handle(), detail::poll_event::writable,
            [h, &loop, cbs](detail::poll_event) mutable {
                loop.unregister_fd(h->native_handle());
                h->clear_cancel_handler();
                if (h->is_io_cancelled()) {
                    std::move(cbs->on_error)(net_error("async_connect: операция отменена"));
                    return;
                }
                if (!h->is_open()) {
                    std::move(cbs->on_error)(net_error("async_connect: сокет закрыт"));
                    return;
                }
                int const err = h->sockets->socket_error(h->native_handle());
                if (err != 0) {
                    std::move(cbs->on_error)(net_error(std::strerror(err)));
                    return;
                }
                std::move(cbs->on_connected)();
            });
    }

    void async_read_some(event_loop& loop, std::span<char> buffer,
                         std::move_only_function<void(std::size_t)> on_data,
                         std::move_only_function<void()> on_eof,
                         std::move_only_function<void(net_error const&)> on_error) {
        if (!is_open()) {
            on_error(net_error("async_read_some: сокет закрыт"));
            return;
        }
        handle_->begin_io();

        auto cbs = std::make_shared<read_callbacks>(
            read_callbacks{std::move(on_data), std::move(on_eof), std::move(on_error)});

        auto h = handle_;
        try {
            auto n = h->sockets->try_recv(h->native_handle(), buffer);
            if (!n) {
                wait_readable_impl(loop, std::move(h), buffer, cbs);
                return;
            }
            if (*n == 0) {
                std::move(cbs->on_eof)();
            } else {
                std::move(cbs->on_data)(*n);
            }
        } catch (net_error const& e) {
            std::move(cbs->on_error)(e);
        }
    }

    void async_write_all(event_loop& loop, std::vector<char> data,
                         std::move_only_function<void()> on_done,
                         std::move_only_function<void(net_error const&)> on_error) {
        if (!is_open()) {
            on_error(net_error("async_write_all: сокет закрыт"));
            return;
        }
        handle_->begin_io();

        auto state = std::make_shared<write_state>();
        state->handle = handle_;
        state->data = std::move(data);
        state->on_done = std::move(on_done);
        state->on_error = std::move(on_error);

        try_write_impl(loop, state);
    }

private:
    struct read_callbacks {
        std::move_only_function<void(std::size_t)> on_data;
        std::move_only_function<void()> on_eof;
        std::move_only_function<void(net_error const&)> on_error;
    };

    struct write_state {
        std::shared_ptr<detail::socket_handle> handle;
        std::vector<char> data;
        std::size_t offset{0};
        std::move_only_function<void()> on_done;
        std::move_only_function<void(net_error const&)> on_error;
    };

    void attach(int fd) {
        if (fd >= 0) {
            handle_ = std::make_shared<detail::socket_handle>(fd, *sockets_);
            sockets_->set_non_blocking(fd);
        }
    }

    void release_if_last_owner() {
        if (handle_ && handle_.use_count() == 1) {
            handle_->close_fd();
        }
    }

    static void wait_readable_impl(event_loop& loop, std::shared_ptr<detail::socket_handle> h,
                                   std::span<char> buffer,
                                   std::shared_ptr<read_callbacks> const& cbs) {
        int const fd = h->native_handle();
        h->set_cancel_handler([cbs](net_error const& e) { std::move(cbs->on_error)(e); });
        loop.register_fd(
            fd, detail::poll_event::readable,
            [h, &loop, buffer, cbs](detail::poll_event) mutable {
                h->clear_cancel_handler();
                loop.unregister_fd(h->native_handle());
                if (h->is_io_cancelled()) {
                    std::move(cbs->on_error)(net_error("async_read_some: операция отменена"));
                    return;
                }
                if (!h->is_open()) {
                    std::move(cbs->on_error)(net_error("async_read_some: сокет закрыт"));
                    return;
                }
                try {
                    auto n = h->sockets->try_recv(h->native_handle(), buffer);
                    if (!n) {
                        wait_readable_impl(loop, std::move(h), buffer, cbs);
                        return;
                    }
                    if (*n == 0) {
                        std::move(cbs->on_eof)();
                    } else {
                        std::move(cbs->on_data)(*n);
                    }
                } catch (net_error const& e) {
                    std::move(cbs->on_error)(e);
                }
            });
    }

    static void try_write_impl(event_loop& loop, std::shared_ptr<write_state> const& state) {
        auto const& h = state->handle;
        if (!h || !h->is_open()) {
            std::move(state->on_error)(net_error("async_write_all: сокет закрыт"));
            return;
        }
        if (h->is_io_cancelled()) {
            std::move(state->on_error)(net_error("async_write_all: операция отменена"));
            return;
        }

        auto const remaining = std::span<char const>{state->data}.subspan(state->offset);
        try {
            auto n = h->sockets->try_send(h->native_handle(), remaining);
            if (!n) {
                state->handle->set_cancel_handler(
                    [state](net_error const& e) { std::move(state->on_error)(e); });
                loop.register_fd(
                    h->native_handle(), detail::poll_event::writable,
                    [h, &loop, state](detail::poll_event) mutable {
                        loop.unregister_fd(h->native_handle());
                        h->clear_cancel_handler();
                        if (h->is_io_cancelled()) {
                            std::move(state->on_error)(net_error("async_write_all: операция отменена"));
                            return;
                        }
                        try_write_impl(loop, state);
                    });
                return;
            }
            state->offset += *n;
            if (state->offset >= state->data.size()) {
                std::move(state->on_done)();
            } else {
                try_write_impl(loop, state);
            }
        } catch (net_error const& e) {
            std::move(state->on_error)(e);
        }
    }

    detail::socket_backend* sockets_;
    std::shared_ptr<detail::socket_handle> handle_;
};

}  // namespace rrmode::netlib::net
