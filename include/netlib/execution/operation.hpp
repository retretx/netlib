#pragma once

#include <atomic>

namespace rrmode::netlib::execution {

/// Отменяемая операция (база для async I/O и composition).
class operation {
public:
    virtual ~operation() = default;

    /// Запросить отмену; идемпотентно.
    void cancel() noexcept { cancelled_.store(true, std::memory_order_release); }

    [[nodiscard]] bool is_cancelled() const noexcept {
        return cancelled_.load(std::memory_order_acquire);
    }

protected:
    operation() = default;

    /// Вызывать в начале work — пропустить, если уже отменено.
    [[nodiscard]] bool throw_if_cancelled() const {
        return is_cancelled();
    }

private:
    std::atomic<bool> cancelled_{false};
};

}  // namespace rrmode::netlib::execution
