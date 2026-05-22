#pragma once

/// Simple API: скрытый io_runtime, sync TCP, write_stream.
#include <netlib/net/simple/io_runtime.hpp>
#include <netlib/net/simple/tcp_connection.hpp>
#include <netlib/net/simple/writable_chunk.hpp>
#include <netlib/net/simple/write_stream.hpp>

#if defined(NETLIB_ENABLE_COROUTINES) && NETLIB_ENABLE_COROUTINES
#include <netlib/net/simple/coro.hpp>
#endif
