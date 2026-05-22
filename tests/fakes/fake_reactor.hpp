#pragma once

#include <netlib/net/detail/reactor_backend.hpp>

#include <chrono>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace netlib::fakes {

/// In-memory reactor для unit-тестов (без epoll).
class fake_reactor final : public rrmode::netlib::net::detail::reactor_backend {
public:
    struct registration {
        rrmode::netlib::net::detail::poll_event events{};
        callback handler;
    };

    void add(int fd, rrmode::netlib::net::detail::poll_event events, callback&& cb) override {
        std::scoped_lock lock{mutex_};
        registrations_[fd] = registration{events, std::move(cb)};
    }

    void modify(int fd, rrmode::netlib::net::detail::poll_event events, callback&& cb) override {
        add(fd, events, std::move(cb));
    }

    void remove(int fd) override {
        std::scoped_lock lock{mutex_};
        registrations_.erase(fd);
    }

    int poll_once(std::chrono::milliseconds timeout) override {
        (void)timeout;
        return dispatch_pending();
    }

    /// Запланировать событие (вызовется при следующем poll_once).
    void arm(int fd, rrmode::netlib::net::detail::poll_event events) {
        std::scoped_lock lock{mutex_};
        pending_.push_back({fd, events});
    }

    int dispatch_pending() {
        std::vector<std::pair<int, rrmode::netlib::net::detail::poll_event>> batch;
        {
            std::scoped_lock lock{mutex_};
            batch.swap(pending_);
        }
        int handled = 0;
        for (auto const& [fd, ev] : batch) {
            registration reg;
            {
                std::scoped_lock lock{mutex_};
                auto it = registrations_.find(fd);
                if (it == registrations_.end()) {
                    continue;
                }
                reg = registration{it->second.events, std::move(it->second.handler)};
                registrations_.erase(it);
            }
            if (reg.handler) {
                reg.handler(ev);
                ++handled;
            }
        }
        return handled;
    }

    [[nodiscard]] bool is_registered(int fd) const {
        std::scoped_lock lock{mutex_};
        return registrations_.contains(fd);
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<int, registration> registrations_;
    std::vector<std::pair<int, rrmode::netlib::net::detail::poll_event>> pending_;
};

}  // namespace netlib::fakes
