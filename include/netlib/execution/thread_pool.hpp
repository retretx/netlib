#pragma once

#include <netlib/execution/detail/task_queue.hpp>
#include <netlib/execution/error.hpp>
#include <netlib/execution/executor.hpp>

#include <atomic>
#include <cstddef>
#include <functional>
#include <thread>
#include <vector>

namespace rrmode::netlib::execution {

/// Пул потоков с очередью задач (fallback executor).
class thread_pool : public executor {
public:
    explicit thread_pool(std::size_t thread_count = 0) {
        if (thread_count == 0) {
            thread_count = std::thread::hardware_concurrency();
        }
        if (thread_count == 0) {
            thread_count = 1;
        }
        workers_.reserve(thread_count);
        for (std::size_t i = 0; i < thread_count; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }

    thread_pool(thread_pool const&) = delete;
    thread_pool& operator=(thread_pool const&) = delete;

    ~thread_pool() override { shutdown(); }

    void post(std::function<void()> fn) override {
        if (stopped_.load(std::memory_order_acquire)) {
            throw execution_error("post в остановленный thread_pool");
        }
        queue_.push(std::move(fn));
    }

    void request_stop() override { shutdown(); }

    /// Остановить пул: закрыть очередь, дождаться потоков.
    void shutdown() {
        bool expected = false;
        if (!stopped_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            return;
        }
        queue_.close();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
    }

    std::size_t thread_count() const noexcept { return workers_.size(); }

private:
    void worker_loop() {
        for (;;) {
            auto task = queue_.try_pop();
            if (!task) {
                return;
            }
            (*task)();
        }
    }

    detail::task_queue queue_;
    std::vector<std::thread> workers_;
    std::atomic<bool> stopped_{false};
};

}  // namespace rrmode::netlib::execution
