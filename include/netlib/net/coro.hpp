#pragma once

/// Coroutine-awaitables для TCP/UDP (требует NETLIB_ENABLE_COROUTINES).
#include <netlib/execution/coroutine.hpp>
#include <netlib/net/awaitables.hpp>
#include <netlib/net/cancellation_token.hpp>
#include <netlib/net/coro_tcp.hpp>
#include <netlib/net/delay.hpp>
#include <netlib/net/timeout.hpp>
#include <netlib/net/tcp_coro.hpp>
#include <netlib/net/udp_awaitables.hpp>
#include <netlib/net/udp_coro.hpp>
#include <netlib/net/unix_awaitables.hpp>
#include <netlib/net/coro_unix.hpp>
#include <netlib/net/unix_coro.hpp>
