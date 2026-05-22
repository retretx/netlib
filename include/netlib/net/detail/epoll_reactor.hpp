#pragma once

#include <netlib/net/detail/poll_event.hpp>
#include <netlib/net/detail/reactor_backend.hpp>
#include <netlib/net/error.hpp>

#include <chrono>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(__linux__)
#include <cerrno>
#include <sys/epoll.h>
#include <unistd.h>
#endif

namespace rrmode::netlib::net::detail {

/// Linux epoll backend.
class epoll_reactor final : public reactor_backend {
public:
    epoll_reactor() {
#if defined(__linux__)
        epfd_ = ::epoll_create1(EPOLL_CLOEXEC);
        if (epfd_ < 0) {
            throw net_error("epoll_create1 failed");
        }
#else
        throw net_error("epoll_reactor: платформа без epoll пока не поддерживается");
#endif
    }

    epoll_reactor(epoll_reactor const&) = delete;
    epoll_reactor& operator=(epoll_reactor const&) = delete;

    ~epoll_reactor() override {
#if defined(__linux__)
        if (epfd_ >= 0) {
            ::close(epfd_);
        }
#endif
    }

    void add(int fd, poll_event events, callback&& cb) override {
#if defined(__linux__)
        std::scoped_lock lock{mutex_};
        epoll_event ev{};
        if (has_event(events, poll_event::readable)) {
            ev.events |= EPOLLIN;
        }
        if (has_event(events, poll_event::writable)) {
            ev.events |= EPOLLOUT;
        }
        ev.data.fd = fd;
        if (::epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) != 0) {
            if (errno == EEXIST && ::epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev) != 0) {
                throw net_error("epoll_ctl MOD failed");
            }
            if (errno != EEXIST) {
                throw net_error("epoll_ctl ADD failed");
            }
        }
        handlers_[fd] = std::forward<callback>(cb);
#else
        (void)fd;
        (void)events;
        (void)cb;
#endif
    }

    void modify(int fd, poll_event events, callback&& cb) override {
#if defined(__linux__)
        std::scoped_lock lock{mutex_};
        epoll_event ev{};
        if (has_event(events, poll_event::readable)) {
            ev.events |= EPOLLIN;
        }
        if (has_event(events, poll_event::writable)) {
            ev.events |= EPOLLOUT;
        }
        ev.data.fd = fd;
        if (::epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev) != 0) {
            throw net_error("epoll_ctl MOD failed");
        }
        handlers_[fd] = std::forward<callback>(cb);
#else
        (void)fd;
        (void)events;
        (void)cb;
#endif
    }

    void remove(int fd) override {
#if defined(__linux__)
        std::scoped_lock lock{mutex_};
        ::epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
        handlers_.erase(fd);
#else
        (void)fd;
#endif
    }

    int poll_once(std::chrono::milliseconds timeout) override {
#if defined(__linux__)
        epoll_event events[64];
        int const n = ::epoll_wait(
            epfd_, events, 64,
            timeout.count() < 0 ? -1 : static_cast<int>(timeout.count()));
        if (n < 0) {
            throw net_error("epoll_wait failed");
        }
        std::vector<std::pair<int, callback>> pending;
        {
            std::scoped_lock lock{mutex_};
            for (int i = 0; i < n; ++i) {
                int const fd = events[i].data.fd;
                auto it = handlers_.find(fd);
                if (it == handlers_.end()) {
                    continue;
                }
                pending.emplace_back(fd, std::move(it->second));
                handlers_.erase(it);
            }
        }
        int handled = 0;
        for (auto& [fd, handler] : pending) {
            poll_event ev = poll_event::none;
            for (int i = 0; i < n; ++i) {
                if (events[i].data.fd == fd) {
                    if (events[i].events & EPOLLIN) {
                        ev = ev | poll_event::readable;
                    }
                    if (events[i].events & EPOLLOUT) {
                        ev = ev | poll_event::writable;
                    }
                }
            }
            handler(ev);
            ++handled;
        }
        return handled;
#else
        (void)timeout;
        return 0;
#endif
    }

private:
#if defined(__linux__)
    int epfd_{-1};
#endif
    std::mutex mutex_;
    std::unordered_map<int, callback> handlers_;
};

using reactor = epoll_reactor;

}  // namespace rrmode::netlib::net::detail
