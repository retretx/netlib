#pragma once

#include <netlib/net/detail/poll_event.hpp>
#include <netlib/net/detail/reactor_backend.hpp>
#include <netlib/net/error.hpp>

#include <chrono>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <poll.h>
#endif

namespace rrmode::netlib::net::detail {

/// Poll/WSAPoll backend (Windows v1 smoke; one-shot dispatch).
class poll_reactor final : public reactor_backend {
public:
    poll_reactor() {
#if defined(_WIN32)
        WSADATA wsa{};
        if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            throw net_error("WSAStartup failed");
        }
        wsa_started_ = true;
#endif
    }

    poll_reactor(poll_reactor const&) = delete;
    poll_reactor& operator=(poll_reactor const&) = delete;

    ~poll_reactor() override {
#if defined(_WIN32)
        if (wsa_started_) {
            ::WSACleanup();
        }
#endif
    }

    void add(int fd, poll_event events, callback&& cb) override {
        std::scoped_lock lock{mutex_};
        registrations_[fd] = registration{events, std::forward<callback>(cb)};
    }

    void modify(int fd, poll_event events, callback&& cb) override {
        add(fd, events, std::move(cb));
    }

    void remove(int fd) override {
        std::scoped_lock lock{mutex_};
        registrations_.erase(fd);
    }

    int poll_once(std::chrono::milliseconds timeout) override {
        std::vector<pollfd> pfds;
        std::vector<int> fds;
        {
            std::scoped_lock lock{mutex_};
            pfds.reserve(registrations_.size());
            fds.reserve(registrations_.size());
            for (auto const& [fd, reg] : registrations_) {
                pollfd pfd{};
                pfd.fd = fd;
                pfd.events = to_poll_events(reg.events);
                if (pfd.events != 0) {
                    pfds.push_back(pfd);
                    fds.push_back(fd);
                }
            }
        }

        if (pfds.empty()) {
            return 0;
        }

        int const rc = poll_wait(pfds.data(), static_cast<int>(pfds.size()), timeout);
        if (rc < 0) {
            throw net_error("poll wait failed");
        }

        std::vector<std::pair<int, callback>> pending;
        {
            std::scoped_lock lock{mutex_};
            for (std::size_t i = 0; i < pfds.size(); ++i) {
                if (pfds[i].revents == 0) {
                    continue;
                }
                int const fd = fds[i];
                auto it = registrations_.find(fd);
                if (it == registrations_.end()) {
                    continue;
                }
                poll_event ev = poll_event::none;
                if ((pfds[i].revents & (POLLIN | POLLRDNORM)) != 0) {
                    ev = ev | poll_event::readable;
                }
                if ((pfds[i].revents & (POLLOUT | POLLWRNORM)) != 0) {
                    ev = ev | poll_event::writable;
                }
                pending.emplace_back(fd, std::move(it->second.handler));
                registrations_.erase(it);
            }
        }

        int handled = 0;
        for (auto& [fd, handler] : pending) {
            poll_event ev = poll_event::none;
            for (std::size_t i = 0; i < pfds.size(); ++i) {
                if (fds[i] != fd || pfds[i].revents == 0) {
                    continue;
                }
                if ((pfds[i].revents & (POLLIN | POLLRDNORM)) != 0) {
                    ev = ev | poll_event::readable;
                }
                if ((pfds[i].revents & (POLLOUT | POLLWRNORM)) != 0) {
                    ev = ev | poll_event::writable;
                }
            }
            if (handler) {
                handler(ev);
                ++handled;
            }
        }
        return handled;
    }

private:
    struct registration {
        poll_event events{};
        callback handler;
    };

    static short to_poll_events(poll_event events) {
        short out = 0;
        if (has_event(events, poll_event::readable)) {
            out = static_cast<short>(out | POLLIN);
        }
        if (has_event(events, poll_event::writable)) {
            out = static_cast<short>(out | POLLOUT);
        }
        return out;
    }

    static int poll_wait(pollfd* fds, int count, std::chrono::milliseconds timeout) {
#if defined(_WIN32)
        return ::WSAPoll(fds, count, static_cast<int>(timeout.count()));
#else
        return ::poll(fds, static_cast<nfds_t>(count), static_cast<int>(timeout.count()));
#endif
    }

#if defined(_WIN32)
    bool wsa_started_{false};
#endif
    std::mutex mutex_;
    std::unordered_map<int, registration> registrations_;
};

}  // namespace rrmode::netlib::net::detail
