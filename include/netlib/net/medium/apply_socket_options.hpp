#pragma once

#include <netlib/net/detail/socket_backend.hpp>
#include <netlib/net/medium/socket_options.hpp>

namespace rrmode::netlib::net::medium {

inline void apply_socket_options(int fd, ::rrmode::netlib::net::detail::socket_backend& backend,
                                 socket_options const& opts) {
    if (opts.reuseaddr.value_or(false)) {
        backend.set_reuseaddr(fd);
    }
    if (opts.tcp_nodelay) {
        backend.set_tcp_nodelay(fd, *opts.tcp_nodelay);
    }
    if (opts.keepalive) {
        backend.set_keepalive(fd, *opts.keepalive);
    }
    if (opts.send_buffer_size) {
        backend.set_send_buffer_size(fd, *opts.send_buffer_size);
    }
    if (opts.recv_buffer_size) {
        backend.set_recv_buffer_size(fd, *opts.recv_buffer_size);
    }
    if (opts.linger_seconds) {
        backend.set_linger(fd, *opts.linger_seconds);
    }
}

}  // namespace rrmode::netlib::net::medium
