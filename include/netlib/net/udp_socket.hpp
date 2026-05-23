#pragma once

#include <netlib/net/detail/default_socket_backend.hpp>
#include <netlib/net/detail/socket_backend.hpp>
#include <netlib/net/detail/socket_handle.hpp>
#include <netlib/net/endpoint.hpp>
#include <netlib/net/error.hpp>
#include <netlib/net/event_loop.hpp>

#include <cstdint>
#include <netlib/detail/move_only_function.hpp>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace rrmode::netlib::net {

/// UDP-сокет (IPv4): bind, sendto/recvfrom через event_loop. Без DNS-модуля и без connect/listen.
class udp_socket {
public:
    explicit udp_socket(detail::socket_backend& sockets = detail::default_socket_backend())
        : sockets_{&sockets} {}

    udp_socket(int fd, detail::socket_backend& sockets = detail::default_socket_backend())
        : sockets_{&sockets} {
        attach(fd);
    }

    explicit udp_socket(std::shared_ptr<detail::socket_handle> handle) : handle_{std::move(handle)} {
        if (handle_ && handle_->sockets != nullptr) {
            sockets_ = handle_->sockets;
        }
    }

    udp_socket(udp_socket const&) = delete;
    udp_socket& operator=(udp_socket const&) = delete;

    udp_socket(udp_socket&& other) noexcept
        : sockets_{other.sockets_}, handle_{std::move(other.handle_)}, bound_port_{other.bound_port_} {
        other.bound_port_ = 0;
    }

    udp_socket& operator=(udp_socket&& other) noexcept {
        if (this != &other) {
            release_if_last_owner();
            sockets_ = other.sockets_;
            handle_ = std::move(other.handle_);
            bound_port_ = other.bound_port_;
            other.bound_port_ = 0;
        }
        return *this;
    }

    ~udp_socket() { release_if_last_owner(); }

    [[nodiscard]] int native_handle() const noexcept {
        return handle_ ? handle_->native_handle() : -1;
    }

    [[nodiscard]] bool is_open() const noexcept { return handle_ && handle_->is_open(); }

    [[nodiscard]] std::shared_ptr<detail::socket_handle> io_handle() const { return handle_; }

    /// Создать UDP-сокет и привязать к endpoint (порт 0 — автоматический).
    void bind(endpoint const& ep) {
        release_if_last_owner();
        handle_.reset();

        int const fd = sockets_->create_udp_socket();
        sockets_->set_non_blocking(fd);
        sockets_->set_reuseaddr(fd);
        sockets_->bind_endpoint(fd, ep);
        bound_port_ = sockets_->get_local_port(fd);
        attach(fd);
    }

    [[nodiscard]] endpoint local_endpoint() const {
        return endpoint{.host = "127.0.0.1", .port = bound_port_};
    }

    void close() {
        if (handle_) {
            handle_->close_fd();
        }
        bound_port_ = 0;
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

    /// Callback API: отправить датаграмму на dest.
    void async_send_to(event_loop& loop, std::vector<char> data, endpoint const& dest,
                      std::move_only_function<void()> on_done,
                      std::move_only_function<void(net_error const&)> on_error) {
        if (!is_open()) {
            on_error(net_error("async_send_to: сокет не открыт"));
            return;
        }
        if (!on_done || !on_error) {
            throw net_error("async_send_to: колбэки обязательны");
        }
        handle_->begin_io();

        auto h = handle_;
        auto cbs = std::make_shared<send_callbacks>(
            send_callbacks{std::move(on_done), std::move(on_error)});
        h->set_cancel_handler([cbs](net_error const& e) { cbs->on_error(e); });

        try_send_to_impl(loop, h, std::move(data), dest, cbs);
    }

    /// Callback API: принять одну датаграмму в buffer (размер буфера — максимум).
    void async_recv_from(event_loop& loop, std::span<char> buffer,
                         std::move_only_function<void(endpoint remote, std::size_t bytes)> on_data,
                         std::move_only_function<void(net_error const&)> on_error) {
        if (!is_open()) {
            on_error(net_error("async_recv_from: сокет не открыт"));
            return;
        }
        if (!on_data || !on_error) {
            throw net_error("async_recv_from: колбэки обязательны");
        }
        handle_->begin_io();

        auto cbs = std::make_shared<recv_callbacks>(
            recv_callbacks{std::move(on_data), std::move(on_error)});
        auto h = handle_;
        h->set_cancel_handler([cbs](net_error const& e) { cbs->on_error(e); });
        try_recv_from_impl(loop, h, buffer, cbs);
    }

private:
    struct send_callbacks {
        std::move_only_function<void()> on_done;
        std::move_only_function<void(net_error const&)> on_error;
    };

    struct recv_callbacks {
        std::move_only_function<void(endpoint, std::size_t)> on_data;
        std::move_only_function<void(net_error const&)> on_error;
    };

    void attach(int fd) {
        if (fd >= 0) {
            handle_ = std::make_shared<detail::socket_handle>(fd, *sockets_);
        }
    }

    void release_if_last_owner() {
        if (handle_ && handle_.use_count() == 1) {
            handle_->close_fd();
        }
    }

    static void try_send_to_impl(event_loop& loop, std::shared_ptr<detail::socket_handle> h,
                                 std::vector<char> data, endpoint const& dest,
                                 std::shared_ptr<send_callbacks> const& cbs) {
        if (!h || !h->is_open()) {
            std::move(cbs->on_error)(net_error("async_send_to: сокет закрыт"));
            return;
        }
        if (h->is_io_cancelled()) {
            std::move(cbs->on_error)(net_error("async_send_to: операция отменена"));
            return;
        }
        try {
            auto n = h->sockets->try_sendto(h->native_handle(), data, dest);
            if (!n) {
                h->set_cancel_handler([cbs](net_error const& e) { cbs->on_error(e); });
                loop.register_fd(
                    h->native_handle(), detail::poll_event::writable,
                    [h, &loop, data = std::move(data), dest, cbs](detail::poll_event) mutable {
                        loop.unregister_fd(h->native_handle());
                        h->clear_cancel_handler();
                        try_send_to_impl(loop, h, std::move(data), dest, cbs);
                    });
                return;
            }
            h->clear_cancel_handler();
            std::move(cbs->on_done)();
        } catch (net_error const& e) {
            h->clear_cancel_handler();
            std::move(cbs->on_error)(e);
        }
    }

    static void try_recv_from_impl(event_loop& loop, std::shared_ptr<detail::socket_handle> h,
                                   std::span<char> buffer,
                                   std::shared_ptr<recv_callbacks> const& cbs) {
        if (!h || !h->is_open()) {
            std::move(cbs->on_error)(net_error("async_recv_from: сокет закрыт"));
            return;
        }
        if (h->is_io_cancelled()) {
            std::move(cbs->on_error)(net_error("async_recv_from: операция отменена"));
            return;
        }
        try {
            endpoint remote{};
            auto n = h->sockets->try_recvfrom(h->native_handle(), buffer, remote);
            if (!n) {
                h->set_cancel_handler([cbs](net_error const& e) { cbs->on_error(e); });
                loop.register_fd(
                    h->native_handle(), detail::poll_event::readable,
                    [h, &loop, buffer, cbs](detail::poll_event) mutable {
                        loop.unregister_fd(h->native_handle());
                        h->clear_cancel_handler();
                        try_recv_from_impl(loop, h, buffer, cbs);
                    });
                return;
            }
            h->clear_cancel_handler();
            std::move(cbs->on_data)(remote, *n);
        } catch (net_error const& e) {
            h->clear_cancel_handler();
            std::move(cbs->on_error)(e);
        }
    }

    detail::socket_backend* sockets_;
    std::shared_ptr<detail::socket_handle> handle_;
    std::uint16_t bound_port_{0};
};

}  // namespace rrmode::netlib::net
