#pragma once

#include <netlib/execution/scheduler.hpp>

#include <condition_variable>
#include <coroutine>
#include <exception>
#include <mutex>
#include <optional>

namespace rrmode::netlib::execution::detail {

struct sync_wait_context {
    std::mutex mutex;
    std::condition_variable cv;
    bool done{false};

    void notify() {
        {
            std::scoped_lock lock{mutex};
            done = true;
        }
        cv.notify_one();
    }

    void wait() {
        std::unique_lock lock{mutex};
        cv.wait(lock, [this] { return done; });
    }
};

struct scheduler_holder {
    scheduler* sched{nullptr};

    void set_scheduler(scheduler& s) noexcept { sched = &s; }
    [[nodiscard]] scheduler* get_scheduler() const noexcept { return sched; }

    void inherit_scheduler(scheduler* parent) noexcept {
        if (sched == nullptr && parent != nullptr) {
            sched = parent;
        }
    }
};

inline void resume_on_scheduler(scheduler_holder& holder, std::coroutine_handle<> cont,
                                sync_wait_context* sync_ctx,
                                std::coroutine_handle<> self = {}) {
    if (sync_ctx != nullptr) {
        sync_ctx->notify();
        return;
    }
    if (cont) {
        if (holder.sched != nullptr) {
            holder.sched->schedule([cont]() mutable { cont.resume(); });
        } else {
            cont.resume();
        }
        return;
    }
    if (self) {
        self.destroy();
    }
}

}  // namespace rrmode::netlib::execution::detail
