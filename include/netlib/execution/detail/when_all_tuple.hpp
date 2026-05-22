#pragma once

#include <netlib/execution/detail/when_all_state.hpp>
#include <netlib/execution/scheduler.hpp>
#include <netlib/execution/task.hpp>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace rrmode::netlib::execution::detail {

template<typename Task>
struct task_value_type {
    using type = typename Task::value_type;
};

template<typename Task>
using task_value_t = typename task_value_type<Task>::type;

template<typename... Tasks>
inline constexpr bool all_non_void_tasks_v =
    (!std::is_void_v<task_value_t<Tasks>> && ...);

template<std::size_t I, typename T, typename Tuple>
inline task<void> when_all_tuple_worker_coro(scheduler& sched, task<T> child,
                                             std::shared_ptr<when_all_group_state> group,
                                             std::shared_ptr<Tuple> results) {
    co_await sched;
    try {
        std::get<I>(*results) = co_await std::move(child);
    } catch (...) {
        group->set_exception(std::current_exception());
    }
    group->worker_finished();
}

template<std::size_t I, typename T, typename Tuple>
inline void launch_when_all_tuple_worker(scheduler& sched, task<T> child,
                                         std::shared_ptr<when_all_group_state> group,
                                         std::shared_ptr<Tuple> results) {
    auto h = when_all_tuple_worker_coro<I>(sched, std::move(child), group, results).release();
    h.promise().set_scheduler(sched);
    h.resume();
}

template<typename Tuple, typename... Tasks, std::size_t... Is>
inline void launch_when_all_tuple_pack(scheduler& sched, std::shared_ptr<when_all_group_state> group,
                                       std::shared_ptr<Tuple> results, std::index_sequence<Is...>,
                                       Tasks&&... tasks) {
    (launch_when_all_tuple_worker<Is>(sched, std::forward<Tasks>(tasks), group, results), ...);
}

}  // namespace rrmode::netlib::execution::detail
