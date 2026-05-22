#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>

namespace rrmode::netlib::execution::detail {

/// Потокобезопасная очередь задач (fallback backend).
class task_queue {
public:
    task_queue() = default;

    void push(std::function<void()> task) {
        {
            std::scoped_lock lock{mutex_};
            if (closed_) {
                return;
            }
            tasks_.push_back(std::move(task));
        }
        cv_.notify_one();
    }

    /// Извлечь задачу; std::nullopt если очередь закрыта и пуста.
    std::optional<std::function<void()>> try_pop() {
        std::unique_lock lock{mutex_};
        cv_.wait(lock, [this] { return closed_ || !tasks_.empty(); });
        if (tasks_.empty()) {
            return std::nullopt;
        }
        auto task = std::move(tasks_.front());
        tasks_.pop_front();
        return task;
    }

    void close() {
        {
            std::scoped_lock lock{mutex_};
            closed_ = true;
        }
        cv_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::function<void()>> tasks_;
    bool closed_{false};
};

}  // namespace rrmode::netlib::execution::detail
