#pragma once

/// Coroutines API (требует NETLIB_ENABLE_COROUTINES при сборке netlib).
#include <netlib/execution/delay.hpp>
#include <netlib/execution/generator.hpp>
#include <netlib/execution/schedule_awaitable.hpp>
#include <netlib/execution/spawn.hpp>
#include <netlib/execution/task.hpp>
#include <netlib/execution/then.hpp>
#include <netlib/execution/timeout_error.hpp>
#include <netlib/execution/when_all.hpp>
#include <netlib/execution/when_any.hpp>
