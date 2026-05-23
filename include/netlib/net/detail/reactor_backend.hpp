#pragma once

#include <netlib/net/detail/poll_event.hpp>
#include <netlib/detail/move_only_function.hpp>

#include <chrono>

namespace rrmode::netlib::net::detail {

/// Абстракция reactor (epoll в prod, fake в unit-тестах).
class reactor_backend {
public:
    using callback = std::move_only_function<void(poll_event)>;

    virtual ~reactor_backend() = default;

    virtual void add(int fd, poll_event events, callback&& cb) = 0;
    virtual void modify(int fd, poll_event events, callback&& cb) = 0;
    virtual void remove(int fd) = 0;
    virtual int poll_once(std::chrono::milliseconds timeout) = 0;
};

}  // namespace rrmode::netlib::net::detail
