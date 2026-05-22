#pragma once

#include <netlib/execution/executor.hpp>

#include <functional>
#include <utility>

namespace rrmode::netlib::execution::detail {

#if defined(NETLIB_HAS_STD_EXECUTION) && NETLIB_HAS_STD_EXECUTION
inline constexpr bool use_std_execution = true;
#else
inline constexpr bool use_std_execution = false;
#endif

/// Единая точка dispatch: сейчас всегда через executor::post (fallback path).
inline void schedule(executor& ex, std::function<void()> fn) { ex.post(std::move(fn)); }

}  // namespace rrmode::netlib::execution::detail

#if defined(NETLIB_HAS_STD_EXECUTION) && NETLIB_HAS_STD_EXECUTION
#include <netlib/execution/detail/std/backend.hpp>
#endif
