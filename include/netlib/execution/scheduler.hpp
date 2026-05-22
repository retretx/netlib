#pragma once

#include <netlib/execution/executor.hpp>

#include <functional>
#include <memory>

namespace rrmode::netlib::execution {

/// Планировщик: доставляет завершения через связанный executor.
class scheduler {
public:
    explicit scheduler(executor& ex) noexcept : executor_{&ex} {}

    scheduler(scheduler const&) = default;
    scheduler& operator=(scheduler const&) = default;

    /// Запланировать выполнение на executor (обёртка для move-only задач).
    void schedule(std::move_only_function<void()> fn) const {
        auto task =
            std::make_shared<std::move_only_function<void()>>(std::move(fn));
        executor_->post([task] { (*task)(); });
    }

    executor& get_executor() const noexcept { return *executor_; }

private:
    executor* executor_;
};

}  // namespace rrmode::netlib::execution
