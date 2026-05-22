#pragma once

#include <netlib/execution/scheduler.hpp>
#include <netlib/net/detail/make_reactor.hpp>
#include <netlib/net/detail/reactor_backend.hpp>
#include <netlib/net/detail/timer_scheduler.hpp>
#include <netlib/net/error.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>

namespace rrmode::netlib::net {

/// Цикл событий reactor: wait → dispatch через scheduler.
class event_loop {
public:
    using callback = detail::reactor_backend::callback;

    explicit event_loop(execution::scheduler sched)
        : event_loop(std::move(sched), detail::make_default_reactor()) {}

    event_loop(execution::scheduler sched, std::unique_ptr<detail::reactor_backend> reactor);

    event_loop(event_loop const&) = delete;
    event_loop& operator=(event_loop const&) = delete;

    void register_fd(int fd, detail::poll_event events, callback&& cb);

    void modify_fd(int fd, detail::poll_event events, callback&& cb);

    void unregister_fd(int fd);

    int run_once(std::chrono::milliseconds timeout = std::chrono::milliseconds{0});

    void run();

    void stop() noexcept { stopped_.store(true, std::memory_order_release); }

    /// Запланировать callback через delay (timerfd на Linux, иначе steady_clock).
    void run_after(std::chrono::milliseconds delay, std::function<void()> fn);

    [[nodiscard]] execution::scheduler const& scheduler() const noexcept { return scheduler_; }

    [[nodiscard]] detail::reactor_backend& reactor() noexcept { return *reactor_; }

    [[nodiscard]] bool uses_kernel_timers() const noexcept { return uses_kernel_timers_; }

    /// @deprecated Используйте uses_kernel_timers().
    [[nodiscard]] bool uses_timerfd() const noexcept { return uses_kernel_timers_; }

private:
    execution::scheduler scheduler_;
    std::unique_ptr<detail::reactor_backend> reactor_;
    std::unique_ptr<detail::timer_scheduler> timers_;
    std::atomic<bool> stopped_{false};
    bool uses_kernel_timers_{false};
};

}  // namespace rrmode::netlib::net

#include <netlib/net/detail/fallback_timer_scheduler.hpp>
#if defined(__linux__)
#include <netlib/net/detail/linux_timer_scheduler.hpp>
#elif defined(__APPLE__)
#include <netlib/net/detail/kqueue_timer_scheduler.hpp>
#endif

namespace rrmode::netlib::net::detail {

[[nodiscard]] inline std::unique_ptr<timer_scheduler> make_timer_scheduler(event_loop& loop) {
#if defined(__linux__)
    return std::make_unique<linux_timer_scheduler>(loop);
#elif defined(__APPLE__)
    if (auto* kq = dynamic_cast<kqueue_reactor*>(&loop.reactor())) {
        return std::make_unique<kqueue_timer_scheduler>(*kq, loop.scheduler());
    }
    return std::make_unique<fallback_timer_scheduler>(loop.scheduler());
#else
    return std::make_unique<fallback_timer_scheduler>(loop.scheduler());
#endif
}

}  // namespace rrmode::netlib::net::detail

namespace rrmode::netlib::net {

inline event_loop::event_loop(execution::scheduler sched,
                              std::unique_ptr<detail::reactor_backend> reactor)
    : scheduler_{sched}
    , reactor_{std::move(reactor)}
    , timers_{detail::make_timer_scheduler(*this)} {
    if (!reactor_) {
        throw net_error("event_loop: reactor не задан");
    }
#if defined(__linux__) || defined(__APPLE__)
    uses_kernel_timers_ = true;
#else
    uses_kernel_timers_ = false;
#endif
}

inline void event_loop::register_fd(int fd, detail::poll_event events, callback&& cb) {
    reactor_->add(fd, events, [this, cb = std::move(cb)](detail::poll_event ev) mutable {
        scheduler_.schedule([cb = std::move(cb), ev]() mutable { cb(ev); });
    });
    (void)reactor_->poll_once(std::chrono::milliseconds{0});
}

inline void event_loop::modify_fd(int fd, detail::poll_event events, callback&& cb) {
    reactor_->modify(fd, events, [this, cb = std::move(cb)](detail::poll_event ev) mutable {
        scheduler_.schedule([cb = std::move(cb), ev]() mutable { cb(ev); });
    });
    (void)reactor_->poll_once(std::chrono::milliseconds{0});
}

inline void event_loop::unregister_fd(int fd) { reactor_->remove(fd); }

inline int event_loop::run_once(std::chrono::milliseconds timeout) {
    if (stopped_.load(std::memory_order_acquire)) {
        return 0;
    }
    timers_->dispatch_ready();
    return reactor_->poll_once(timeout);
}

inline void event_loop::run() {
    while (!stopped_.load(std::memory_order_acquire)) {
        run_once(std::chrono::milliseconds{100});
    }
}

inline void event_loop::run_after(std::chrono::milliseconds delay, std::function<void()> fn) {
    timers_->run_after(delay, std::move(fn));
}

}  // namespace rrmode::netlib::net
