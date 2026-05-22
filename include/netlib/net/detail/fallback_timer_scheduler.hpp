#pragma once

#include <netlib/execution/scheduler.hpp>
#include <netlib/net/detail/timer_scheduler.hpp>

#include <chrono>
#include <functional>
#include <mutex>
#include <vector>

namespace rrmode::netlib::net::detail {

/// Таймеры по steady_clock (fake reactor, Windows, macOS).
class fallback_timer_scheduler final : public timer_scheduler {
public:
    explicit fallback_timer_scheduler(execution::scheduler const& sched) : sched_{sched} {}

    void run_after(std::chrono::milliseconds delay, std::function<void()> fn) override {
        if (!fn) {
            return;
        }
        std::scoped_lock lock{mutex_};
        entries_.push_back(entry{
            .fire_at = std::chrono::steady_clock::now() + delay,
            .callback = std::move(fn),
        });
    }

    void dispatch_ready() override {
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
            sched_.schedule(std::move(fn));
        }
    }

private:
    struct entry {
        std::chrono::steady_clock::time_point fire_at;
        std::function<void()> callback;
    };

    execution::scheduler const& sched_;
    std::mutex mutex_;
    std::vector<entry> entries_;
};

}  // namespace rrmode::netlib::net::detail
