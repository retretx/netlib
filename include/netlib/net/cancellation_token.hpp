#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

namespace rrmode::netlib::net {

/// Токен отмены для coroutine / async I/O (потокобезопасный).
class cancellation_token {
public:
    cancellation_token() : state_{std::make_shared<state>()} {}

    [[nodiscard]] bool is_cancelled() const noexcept {
        return state_->cancelled.load(std::memory_order_acquire);
    }

    /// Запросить отмену и выполнить зарегистрированное действие (например cancel_io).
    void cancel() {
        std::function<void()> action;
        {
            std::scoped_lock lock{state_->mutex};
            state_->cancelled.store(true, std::memory_order_release);
            action = std::move(state_->action);
            state_->action = {};
        }
        if (action) {
            action();
        }
    }

    /// Действие при cancel(); перезаписывает предыдущее.
    void on_cancel(std::function<void()> action) {
        std::scoped_lock lock{state_->mutex};
        state_->action = std::move(action);
    }

    void clear_on_cancel() {
        std::scoped_lock lock{state_->mutex};
        state_->action = {};
    }

private:
    struct state {
        std::atomic<bool> cancelled{false};
        std::mutex mutex;
        std::function<void()> action;
    };

    std::shared_ptr<state> state_;
};

/// Источник токена: удобно передавать в задачи и вызывать cancel().
class cancellation_source {
public:
    [[nodiscard]] cancellation_token& token() noexcept { return token_; }
    [[nodiscard]] cancellation_token const& token() const noexcept { return token_; }

    void cancel() { token_.cancel(); }

private:
    cancellation_token token_;
};

}  // namespace rrmode::netlib::net
