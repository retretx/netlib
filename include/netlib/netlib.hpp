#pragma once

/// Точка входа netlib: execution + net (без detail).
#include <netlib/config.hpp>
#include <netlib/execution/executor.hpp>
#include <netlib/execution/scheduler.hpp>
#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/endpoint.hpp>
#include <netlib/net/error.hpp>
#include <netlib/net/event_loop.hpp>
#include <netlib/net/tcp_acceptor.hpp>
#include <netlib/net/tcp_socket.hpp>
#include <netlib/net/udp_socket.hpp>

#if defined(NETLIB_ENABLE_COROUTINES) && NETLIB_ENABLE_COROUTINES
#include <netlib/execution/coroutine.hpp>
#include <netlib/net/coro.hpp>
#endif
