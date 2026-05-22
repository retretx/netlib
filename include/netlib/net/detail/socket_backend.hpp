#pragma once

#include <netlib/net/endpoint.hpp>
#include <netlib/net/unix_endpoint.hpp>

#include <cstdint>
#include <string>
#include <optional>
#include <span>

namespace rrmode::netlib::net::detail {

/// Абстракция ОС-сокетов (подменяется fake в unit-тестах).
class socket_backend {
public:
    virtual ~socket_backend() = default;

    virtual int create_tcp_socket() = 0;
    virtual int create_udp_socket() = 0;
    virtual int create_unix_stream_socket() = 0;
    virtual void close(int fd) = 0;
    virtual void set_non_blocking(int fd) = 0;
    virtual void set_reuseaddr(int fd) = 0;

    virtual void set_tcp_nodelay(int /*fd*/, bool /*on*/) {}
    virtual void set_keepalive(int /*fd*/, bool /*on*/) {}
    virtual void set_send_buffer_size(int /*fd*/, int /*bytes*/) {}
    virtual void set_recv_buffer_size(int /*fd*/, int /*bytes*/) {}
    virtual void set_linger(int /*fd*/, int /*seconds*/) {}

    /// 0 — готово; -1 — EINPROGRESS.
    virtual int start_connect(int fd, endpoint const& ep) = 0;
    virtual int socket_error(int fd) = 0;

    virtual std::optional<std::size_t> try_recv(int fd, std::span<char> buf) = 0;
    virtual std::optional<std::size_t> try_send(int fd, std::span<char const> buf) = 0;

    /// UDP: -1 / nullopt — EAGAIN. out — адрес отправителя.
    virtual std::optional<std::size_t> try_recvfrom(int fd, std::span<char> buf, endpoint& out) = 0;
    virtual std::optional<std::size_t> try_sendto(int fd, std::span<char const> buf,
                                                  endpoint const& dest) = 0;

    virtual void bind_endpoint(int fd, endpoint const& ep) = 0;
    virtual void bind_unix(int fd, unix_endpoint const& ep) = 0;
    virtual int start_connect_unix(int fd, unix_endpoint const& ep) = 0;
    virtual std::string get_bound_unix_path(int fd) = 0;
    virtual void listen_socket(int fd, int backlog = 128) = 0;
    virtual std::uint16_t get_local_port(int fd) = 0;

    /// -1 — EAGAIN; иначе fd клиента.
    virtual int accept_nonblocking(int fd) = 0;
};

}  // namespace rrmode::netlib::net::detail
