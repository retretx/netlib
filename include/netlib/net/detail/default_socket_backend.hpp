#pragma once

#include <netlib/net/detail/socket_backend.hpp>

#if defined(_WIN32)
#include <netlib/net/detail/win_socket_backend.hpp>
#else
#include <netlib/net/detail/posix_socket_backend.hpp>
#endif

namespace rrmode::netlib::net::detail {

inline socket_backend& default_socket_backend() {
#if defined(_WIN32)
    static win_socket_backend instance;
#else
    static posix_socket_backend instance;
#endif
    return instance;
}

}  // namespace rrmode::netlib::net::detail
