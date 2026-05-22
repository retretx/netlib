#pragma once

#include <netlib/execution/scheduler.hpp>
#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/detail/default_socket_backend.hpp>
#include <netlib/net/detail/socket_backend.hpp>
#include <netlib/net/event_loop.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <thread>

namespace rrmode::netlib::net::simple {

namespace net_detail = ::rrmode::netlib::net::detail;

/// Скрытый I/O runtime: thread_pool + scheduler + event_loop + фоновый poll.
class io_runtime {
public:
    struct config {
        std::size_t thread_count{2};
        std::chrono::milliseconds poll_tick{10};
    };

    io_runtime() : io_runtime(config{}, net_detail::default_socket_backend()) {}

    io_runtime(config cfg, net_detail::socket_backend& sockets)
        : owns_all_{true},
          owned_pool_{std::make_unique<execution::thread_pool>(cfg.thread_count)},
          sched_{*owned_pool_},
          owned_loop_{std::make_unique<event_loop>(sched_)},
          loop_{*owned_loop_},
          sockets_{&sockets},
          poll_tick_{cfg.poll_tick} {
        start_io_thread();
    }

    /// Для unit-тестов: внешние pool/loop, без фонового потока (вызывайте pump()).
    io_runtime(execution::thread_pool& pool, execution::scheduler& sched, event_loop& loop,
               net_detail::socket_backend& sockets)
        : owns_all_{false},
          sched_{sched},
          loop_{loop},
          sockets_{&sockets} {
        (void)pool;
    }

    io_runtime(io_runtime const&) = delete;
    io_runtime& operator=(io_runtime const&) = delete;

    ~io_runtime() { shutdown(); }

    void shutdown() {
        if (shutdown_.exchange(true)) {
            return;
        }
        loop().stop();
        if (io_thread_.joinable()) {
            io_thread_.join();
        }
        if (owns_all_ && owned_pool_) {
            owned_pool_->shutdown();
        }
    }

    [[nodiscard]] event_loop& loop() noexcept { return loop_; }
    [[nodiscard]] execution::scheduler const& scheduler() const noexcept { return sched_; }
    [[nodiscard]] net_detail::socket_backend& sockets() noexcept { return *sockets_; }

    void pump() {
        loop().run_once(std::chrono::milliseconds{0});
        std::this_thread::sleep_for(std::chrono::microseconds{100});
    }

    void pump_tick() { loop().run_once(poll_tick_); }

private:
    void start_io_thread() {
        io_thread_ = std::thread([this] {
            while (!shutdown_.load(std::memory_order_acquire)) {
                pump_tick();
            }
        });
    }

    bool owns_all_{false};
    std::unique_ptr<execution::thread_pool> owned_pool_;
    execution::scheduler sched_;
    std::unique_ptr<event_loop> owned_loop_;
    event_loop& loop_;
    net_detail::socket_backend* sockets_{nullptr};
    std::chrono::milliseconds poll_tick_{10};
    std::atomic<bool> shutdown_{false};
    std::thread io_thread_;
};

}  // namespace rrmode::netlib::net::simple
