#pragma once

#include <netlib/execution/scheduler.hpp>
#include <netlib/net/detail/poll_event.hpp>
#include <netlib/net/detail/reactor_backend.hpp>
#include <netlib/net/error.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(__APPLE__)
#include <sys/event.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace rrmode::netlib::net::detail {

/// BSD/macOS kqueue: сокеты + EVFILT_TIMER в одном kqueue.
class kqueue_reactor final : public reactor_backend {
public:
    static constexpr std::uintptr_t timer_ident = 0x7e71;

    kqueue_reactor() {
#if defined(__APPLE__)
        kq_ = ::kqueue();
        if (kq_ < 0) {
            throw net_error("kqueue failed");
        }
#else
        throw net_error("kqueue_reactor: только macOS/BSD");
#endif
    }

    kqueue_reactor(kqueue_reactor const&) = delete;
    kqueue_reactor& operator=(kqueue_reactor const&) = delete;

    ~kqueue_reactor() override {
#if defined(__APPLE__)
        {
            std::scoped_lock lock{mutex_};
            disarm_timer_unlocked();
        }
        if (kq_ >= 0) {
            ::close(kq_);
        }
#endif
    }

    void add(int fd, poll_event events, callback&& cb) override {
#if defined(__APPLE__)
        std::scoped_lock lock{mutex_};
        handlers_[fd] = std::forward<callback>(cb);
        apply_filters_locked(fd, events);
#else
        (void)fd;
        (void)events;
        (void)cb;
#endif
    }

    void modify(int fd, poll_event events, callback&& cb) override {
#if defined(__APPLE__)
        std::scoped_lock lock{mutex_};
        clear_filters_locked(fd);
        handlers_[fd] = std::forward<callback>(cb);
        apply_filters_locked(fd, events);
#else
        (void)fd;
        (void)events;
        (void)cb;
#endif
    }

    void remove(int fd) override {
#if defined(__APPLE__)
        std::scoped_lock lock{mutex_};
        clear_filters_locked(fd);
        handlers_.erase(fd);
#else
        (void)fd;
#endif
    }

    int poll_once(std::chrono::milliseconds timeout) override {
#if defined(__APPLE__)
        struct kevent events[64];
        timespec ts{};
        timespec* tsp = nullptr;
        if (timeout.count() >= 0) {
            ts.tv_sec = static_cast<time_t>(timeout.count() / 1000);
            ts.tv_nsec = static_cast<long>((timeout.count() % 1000) * 1'000'000);
            tsp = &ts;
        }

        int const n = ::kevent(kq_, nullptr, 0, events, 64, tsp);
        if (n < 0) {
            throw net_error("kevent wait failed");
        }

        bool timer_fired = false;
        std::unordered_map<int, poll_event> fired;
        for (int i = 0; i < n; ++i) {
            if (events[i].filter == EVFILT_TIMER &&
                events[i].ident == static_cast<uintptr_t>(timer_ident)) {
                timer_fired = true;
                continue;
            }
            int const fd = static_cast<int>(events[i].ident);
            poll_event ev = poll_event::none;
            if (events[i].filter == EVFILT_READ) {
                ev = ev | poll_event::readable;
            }
            if (events[i].filter == EVFILT_WRITE) {
                ev = ev | poll_event::writable;
            }
            fired[fd] = fired.contains(fd) ? (fired[fd] | ev) : ev;
        }

        if (timer_fired) {
            drain_timer_kevents();
            std::vector<std::function<void()>> due;
            {
                std::scoped_lock lock{mutex_};
                dispatch_due_timers_locked(&due);
                arm_timer_unlocked();
            }
            for (auto& fn : due) {
                fn();
            }
        }

        std::vector<std::pair<int, callback>> pending;
        {
            std::scoped_lock lock{mutex_};
            for (auto const& [fd, ev] : fired) {
                auto it = handlers_.find(fd);
                if (it == handlers_.end()) {
                    continue;
                }
                pending.emplace_back(fd, std::move(it->second));
                handlers_.erase(it);
                clear_filters_locked(fd);
            }
        }

        int handled = timer_fired ? 1 : 0;
        for (auto& [fd, handler] : pending) {
            poll_event ev = poll_event::none;
            auto it = fired.find(fd);
            if (it != fired.end()) {
                ev = it->second;
            }
            if (handler) {
                handler(ev);
                ++handled;
            }
        }
        return handled;
#else
        (void)timeout;
        return 0;
#endif
    }

#if defined(__APPLE__)
    void run_after(std::chrono::milliseconds delay, std::function<void()> fn) {
        if (!fn) {
            return;
        }
        std::scoped_lock lock{mutex_};
        timer_entries_.push_back(timer_entry{
            .fire_at = std::chrono::steady_clock::now() + delay,
            .callback = std::move(fn),
        });
        arm_timer_unlocked();
    }

    void dispatch_timers(execution::scheduler const& sched) {
        std::vector<std::function<void()>> due;
        {
            std::scoped_lock lock{mutex_};
            dispatch_due_timers_locked(&due);
            arm_timer_unlocked();
        }
        for (auto& fn : due) {
            sched.schedule(std::move(fn));
        }
    }
#endif

private:
#if defined(__APPLE__)
    struct timer_entry {
        std::chrono::steady_clock::time_point fire_at;
        std::function<void()> callback;
    };

    void drain_timer_kevents() {
        struct kevent extra[8];
        timespec ts{};
        for (;;) {
            int const n = ::kevent(kq_, nullptr, 0, extra, 8, &ts);
            if (n <= 0) {
                break;
            }
        }
    }

    void dispatch_due_timers_locked(std::vector<std::function<void()>>* out) {
        auto const now = std::chrono::steady_clock::now();
        auto out_it = timer_entries_.begin();
        for (auto it = timer_entries_.begin(); it != timer_entries_.end(); ++it) {
            if (it->fire_at <= now) {
                out->push_back(std::move(it->callback));
            } else {
                *out_it++ = std::move(*it);
            }
        }
        timer_entries_.erase(out_it, timer_entries_.end());
    }

    void arm_timer_unlocked() {
        if (timer_entries_.empty()) {
            disarm_timer_unlocked();
            return;
        }
        auto const now = std::chrono::steady_clock::now();
        auto next_at = timer_entries_.front().fire_at;
        for (auto const& e : timer_entries_) {
            next_at = std::min(next_at, e.fire_at);
        }
        auto delay_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(next_at - now);
        if (delay_ns.count() < 0) {
            delay_ns = std::chrono::nanoseconds{0};
        }

        disarm_timer_unlocked();
        struct kevent change{};
        EV_SET(&change, timer_ident, EVFILT_TIMER, EV_ADD | EV_ONESHOT, NOTE_NSECONDS,
               static_cast<std::uint64_t>(delay_ns.count()), nullptr);
        if (::kevent(kq_, &change, 1, nullptr, 0, nullptr) != 0) {
            throw net_error("kqueue timer: kevent EVFILT_TIMER failed");
        }
        timer_armed_ = true;
    }

    void disarm_timer_unlocked() {
        if (!timer_armed_) {
            return;
        }
        struct kevent change{};
        EV_SET(&change, timer_ident, EVFILT_TIMER, EV_DELETE, 0, 0, nullptr);
        (void)::kevent(kq_, &change, 1, nullptr, 0, nullptr);
        timer_armed_ = false;
    }

    void clear_filters_locked(int fd) {
        auto it = registered_.find(fd);
        if (it == registered_.end()) {
            return;
        }
        struct kevent changes[2];
        int n = 0;
        if (has_event(it->second, poll_event::readable)) {
            EV_SET(&changes[n++], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
        }
        if (has_event(it->second, poll_event::writable)) {
            EV_SET(&changes[n++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
        }
        if (n > 0) {
            (void)::kevent(kq_, changes, n, nullptr, 0, nullptr);
        }
        registered_.erase(it);
    }

    void apply_filters_locked(int fd, poll_event events) {
        struct kevent changes[2];
        int n = 0;
        if (has_event(events, poll_event::readable)) {
            EV_SET(&changes[n++], fd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
        }
        if (has_event(events, poll_event::writable)) {
            EV_SET(&changes[n++], fd, EVFILT_WRITE, EV_ADD, 0, 0, nullptr);
        }
        if (n > 0) {
            if (::kevent(kq_, changes, n, nullptr, 0, nullptr) != 0) {
                throw net_error("kevent ADD failed");
            }
        }
        registered_[fd] = events;
    }

    int kq_{-1};
    bool timer_armed_{false};
    std::unordered_map<int, poll_event> registered_;
    std::vector<timer_entry> timer_entries_;
#endif
    std::mutex mutex_;
    std::unordered_map<int, callback> handlers_;
};

}  // namespace rrmode::netlib::net::detail
