#pragma once

#include <netlib/execution/schedule_awaitable.hpp>
#include <netlib/execution/task.hpp>

#include <type_traits>
#include <utility>

namespace rrmode::netlib::execution::detail {

template<typename T, typename F>
    requires(!std::is_void_v<T> && !std::is_void_v<std::invoke_result_t<F, T>>)
task<std::invoke_result_t<F, T>> then_impl(scheduler& sched, task<T> input, F fn) {
    co_await sched;
    T value = co_await std::move(input);
    co_await sched;
    co_return fn(std::move(value));
}

template<typename T, typename F>
    requires(!std::is_void_v<T> && std::is_void_v<std::invoke_result_t<F, T>>)
task<void> then_impl(scheduler& sched, task<T> input, F fn) {
    co_await sched;
    T value = co_await std::move(input);
    co_await sched;
    fn(std::move(value));
}

template<typename F>
    requires(!std::is_void_v<std::invoke_result_t<F>>)
task<std::invoke_result_t<F>> then_void_input_impl(scheduler& sched, task<void> input, F fn) {
    co_await sched;
    co_await std::move(input);
    co_await sched;
    co_return fn();
}

template<typename F>
    requires(std::is_void_v<std::invoke_result_t<F>>)
task<void> then_void_input_void_impl(scheduler& sched, task<void> input, F fn) {
    co_await sched;
    co_await std::move(input);
    co_await sched;
    fn();
}

}  // namespace rrmode::netlib::execution::detail

namespace rrmode::netlib::execution {

template<typename T, typename F>
    requires(!std::is_void_v<T>)
auto then(scheduler& sched, task<T> input, F&& fn) {
    return detail::then_impl(sched, std::move(input), std::forward<F>(fn));
}

template<typename F>
auto then(scheduler& sched, task<void> input, F&& fn) {
    if constexpr (std::is_void_v<std::invoke_result_t<F>>) {
        return detail::then_void_input_void_impl(sched, std::move(input), std::forward<F>(fn));
    } else {
        return detail::then_void_input_impl(sched, std::move(input), std::forward<F>(fn));
    }
}

}  // namespace rrmode::netlib::execution
