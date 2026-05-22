#pragma once

#include <netlib/net/detail/reactor_backend.hpp>
#include <netlib/net/error.hpp>

#include <memory>

#if defined(__linux__)
#include <netlib/net/detail/epoll_reactor.hpp>
#elif defined(__APPLE__)
#include <netlib/net/detail/kqueue_reactor.hpp>
#elif defined(_WIN32)
#include <netlib/net/detail/poll_reactor.hpp>
#endif

namespace rrmode::netlib::net::detail {

/// Платформенный reactor: Linux epoll, macOS kqueue, Windows WSAPoll.
[[nodiscard]] inline std::unique_ptr<reactor_backend> make_default_reactor() {
#if defined(__linux__)
    return std::make_unique<epoll_reactor>();
#elif defined(__APPLE__)
    return std::make_unique<kqueue_reactor>();
#elif defined(_WIN32)
    return std::make_unique<poll_reactor>();
#else
    throw net_error("make_default_reactor: платформа не поддерживается");
#endif
}

}  // namespace rrmode::netlib::net::detail
