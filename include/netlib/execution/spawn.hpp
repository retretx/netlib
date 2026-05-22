#pragma once

#include <netlib/execution/schedule_awaitable.hpp>
#include <netlib/execution/task.hpp>

namespace rrmode::netlib::execution {

namespace detail {

template<typename T>
task<void> spawn_runner(scheduler& sched, task<T> child) {
    co_await sched;
    if constexpr (std::is_void_v<T>) {
        co_await std::move(child);
    } else {
        (void)co_await std::move(child);
    }
}

template<typename T>
void launch_detached(scheduler& sched, task<T> child) {
    auto h = spawn_runner(sched, std::move(child)).release();
    h.promise().set_scheduler(sched);
    h.resume();
}

}  // namespace detail

/// Запустить task без ожидания; coroutine уничтожается по завершении.
template<typename T>
void spawn(scheduler& sched, task<T> child) {
    detail::launch_detached(sched, std::move(child));
}

}  // namespace rrmode::netlib::execution
