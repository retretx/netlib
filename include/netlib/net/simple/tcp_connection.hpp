#pragma once

#include <netlib/net/endpoint.hpp>
#include <netlib/net/error.hpp>
#include <netlib/net/simple/detail/sync_wait.hpp>
#include <netlib/net/simple/io_runtime.hpp>
#include <netlib/net/tcp_socket.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <thread>
#include <vector>

namespace rrmode::netlib::net::simple {

class write_stream;

/// TCP-клиент simple-уровня: sync по умолчанию, async_* по запросу.
class tcp_connection {
public:
    explicit tcp_connection(io_runtime& runtime)
        : runtime_{&runtime}, socket_{runtime.sockets()} {}

    tcp_connection(io_runtime& runtime, tcp_socket socket)
        : runtime_{&runtime}, socket_{std::move(socket)} {}

    tcp_connection(tcp_connection const&) = delete;
    tcp_connection& operator=(tcp_connection const&) = delete;

    tcp_connection(tcp_connection&&) noexcept = default;
    tcp_connection& operator=(tcp_connection&&) noexcept = default;

    [[nodiscard]] bool is_open() const noexcept { return socket_.is_open(); }

    [[nodiscard]] tcp_socket& socket() noexcept { return socket_; }
    [[nodiscard]] tcp_socket const& socket() const noexcept { return socket_; }

    [[nodiscard]] io_runtime& runtime() noexcept { return *runtime_; }

    /// Синхронное подключение (блокирует до завершения или ошибки).
    void connect(endpoint const& ep) {
        detail::sync_wait_state state;
        async_connect(
            ep, [&state]() { state.complete_ok(); },
            [&state](net_error const& e) { state.complete_err(e); });
        detail::wait_completion(state, [this] { runtime_->pump(); });
    }

    void async_connect(endpoint const& ep, std::move_only_function<void()> on_connected,
                       std::move_only_function<void(net_error const&)> on_error) {
        socket_.async_connect(runtime_->loop(), ep, std::move(on_connected), std::move(on_error));
    }

    /// Синхронная запись всех байт.
    void write_all(std::span<char const> data) {
        std::vector<char> buf(data.begin(), data.end());
        detail::sync_wait_state state;
        async_write_all(
            std::move(buf), [&state]() { state.complete_ok(); },
            [&state](net_error const& e) { state.complete_err(e); });
        detail::wait_completion(state, [this] { runtime_->pump(); });
    }

    void async_write_all(std::vector<char> data, std::move_only_function<void()> on_done,
                         std::move_only_function<void(net_error const&)> on_error) {
        socket_.async_write_all(runtime_->loop(), std::move(data), std::move(on_done),
                                std::move(on_error));
    }

    /// Синхронное чтение: возвращает число байт; eof без данных — исключение.
    [[nodiscard]] std::size_t read_some(std::span<char> buffer) {
        struct read_outcome {
            std::size_t bytes{0};
            enum class kind { pending, data, eof, error } status{kind::pending};
            net_error error{"read_some"};
        };
        auto out = std::make_shared<read_outcome>();
        async_read_some(
            buffer,
            [out](std::size_t n) {
                out->bytes = n;
                out->status = read_outcome::kind::data;
            },
            [out]() { out->status = read_outcome::kind::eof; },
            [out](net_error const& e) {
                out->error = e;
                out->status = read_outcome::kind::error;
            });

        while (out->status == read_outcome::kind::pending) {
            runtime_->pump();
        }
        if (out->status == read_outcome::kind::error) {
            throw out->error;
        }
        if (out->status == read_outcome::kind::eof) {
            throw net_error("read_some: eof");
        }
        return out->bytes;
    }

    void async_read_some(std::span<char> buffer,
                         std::move_only_function<void(std::size_t)> on_data,
                         std::move_only_function<void()> on_eof,
                         std::move_only_function<void(net_error const&)> on_error) {
        socket_.async_read_some(runtime_->loop(), buffer, std::move(on_data), std::move(on_eof),
                                std::move(on_error));
    }

    void close() { socket_.close(runtime_->loop()); }

    [[nodiscard]] write_stream write();
    [[nodiscard]] write_stream write(std::size_t auto_flush_threshold);

private:
    io_runtime* runtime_;
    tcp_socket socket_;
};

}  // namespace rrmode::netlib::net::simple
