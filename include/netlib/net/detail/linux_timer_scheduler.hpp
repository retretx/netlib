#pragma once

#include <netlib/net/detail/poll_event.hpp>
#include <netlib/net/error.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

#if defined(__linux__)
#include <sys/timerfd.h>
#include <unistd.h>
#endif

namespace rrmode::netlib::net::detail {

#if defined(__linux__)

/// timerfd + epoll: точные таймеры без sleep в thread_pool.
class linux_timer_scheduler final : public timer_scheduler {
public:
    explicit linux_timer_scheduler(event_loop& loop) : loop_{loop} {
        timerfd_ = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
        if (timerfd_ < 0) {
            throw net_error("timerfd_create failed");
        }
        ensure_timerfd_registered();
    }

    linux_timer_scheduler(linux_timer_scheduler const&) = delete;
    linux_timer_scheduler& operator=(linux_timer_scheduler const&) = delete;

    ~linux_timer_scheduler() override {
        if (timerfd_ >= 0) {
            loop_.unregister_fd(timerfd_);
            ::close(timerfd_);
            timerfd_ = -1;
        }
    }

    void run_after(std::chrono::milliseconds delay, std::function<void()> fn) override {
        if (!fn) {
            return;
        }
        {
            std::scoped_lock lock{mutex_};
            entries_.push_back(entry{
                .fire_at = std::chrono::steady_clock::now() + delay,
                .callback = std::move(fn),
            });
        }
        arm_next();
    }

    void dispatch_ready() override {
        drain_timerfd();
        dispatch_due();
        arm_next();
    }

private:
    struct entry {
        std::chrono::steady_clock::time_point fire_at;
        std::function<void()> callback;
    };

    void ensure_timerfd_registered() {
        auto handler = [this](poll_event) { on_timerfd_readable(); };
        if (!epoll_registered_) {
            loop_.register_fd(timerfd_, poll_event::readable, std::move(handler));
            epoll_registered_ = true;
        } else {
            loop_.modify_fd(timerfd_, poll_event::readable, std::move(handler));
        }
    }

    void on_timerfd_readable() {
        drain_timerfd();
        dispatch_due();
        arm_next();
        ensure_timerfd_registered();
    }

    void drain_timerfd() {
        std::uint64_t ticks = 0;
        while (true) {
            ssize_t const n = ::read(timerfd_, &ticks, sizeof(ticks));
            if (n < 0 || n < static_cast<ssize_t>(sizeof(ticks))) {
                break;
            }
        }
    }

    void dispatch_due() {
        auto const now = std::chrono::steady_clock::now();
        std::vector<std::function<void()>> due;
        {
            std::scoped_lock lock{mutex_};
            auto out = entries_.begin();
            for (auto it = entries_.begin(); it != entries_.end(); ++it) {
                if (it->fire_at <= now) {
                    due.push_back(std::move(it->callback));
                } else {
                    *out++ = std::move(*it);
                }
            }
            entries_.erase(out, entries_.end());
        }
        for (auto& fn : due) {
            loop_.scheduler().schedule(std::move(fn));
        }
    }

    void arm_next() {
        std::chrono::milliseconds delay{0};
        {
            std::scoped_lock lock{mutex_};
            if (entries_.empty()) {
                disarm();
                return;
            }
            auto const now = std::chrono::steady_clock::now();
            auto next_at = entries_.front().fire_at;
            for (auto const& e : entries_) {
                next_at = std::min(next_at, e.fire_at);
            }
            delay = std::chrono::duration_cast<std::chrono::milliseconds>(next_at - now);
            if (delay.count() < 0) {
                delay = std::chrono::milliseconds{0};
            }
        }

        itimerspec spec{};
        auto const sec = delay.count() / 1000;
        auto const nsec = (delay.count() % 1000) * 1'000'000;
        spec.it_value.tv_sec = sec;
        spec.it_value.tv_nsec = nsec;
        if (::timerfd_settime(timerfd_, 0, &spec, nullptr) != 0) {
            throw net_error("timerfd_settime failed");
        }
    }

    void disarm() {
        itimerspec spec{};
        (void)::timerfd_settime(timerfd_, 0, &spec, nullptr);
    }

    event_loop& loop_;
    int timerfd_{-1};
    bool epoll_registered_{false};
    std::mutex mutex_;
    std::vector<entry> entries_;
};

#endif

}  // namespace rrmode::netlib::net::detail
