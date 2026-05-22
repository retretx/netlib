#pragma once

/// Обратная совместимость: свободные функции делегируют в posix_socket_backend.
#include <netlib/net/detail/default_socket_backend.hpp>

namespace rrmode::netlib::net::detail {

inline int create_tcp_socket() { return default_socket_backend().create_tcp_socket(); }
inline void close_socket(int fd) { default_socket_backend().close(fd); }
inline void set_non_blocking(int fd) { default_socket_backend().set_non_blocking(fd); }
inline void set_reuseaddr(int fd) { default_socket_backend().set_reuseaddr(fd); }
inline int start_connect(int fd, endpoint const& ep) {
    return default_socket_backend().start_connect(fd, ep);
}
inline int socket_error(int fd) { return default_socket_backend().socket_error(fd); }
inline std::optional<std::size_t> try_recv(int fd, std::span<char> buf) {
    return default_socket_backend().try_recv(fd, buf);
}
inline std::optional<std::size_t> try_send(int fd, std::span<char const> buf) {
    return default_socket_backend().try_send(fd, buf);
}
inline void bind_endpoint(int fd, endpoint const& ep) {
    default_socket_backend().bind_endpoint(fd, ep);
}
inline void listen_socket(int fd, int backlog = 128) {
    default_socket_backend().listen_socket(fd, backlog);
}
inline std::uint16_t get_local_port(int fd) {
    return default_socket_backend().get_local_port(fd);
}
inline int accept_nonblocking(int fd) { return default_socket_backend().accept_nonblocking(fd); }

}  // namespace rrmode::netlib::net::detail
