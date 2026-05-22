#pragma once

/// Coroutine-обёртки simple-уровня (требует NETLIB_ENABLE_COROUTINES).
#if defined(NETLIB_ENABLE_COROUTINES) && NETLIB_ENABLE_COROUTINES

#include <netlib/execution/task.hpp>
#include <netlib/net/awaitables.hpp>
#include <netlib/net/simple/tcp_connection.hpp>

namespace rrmode::netlib::net::simple {

inline connect_awaitable connect_async(tcp_connection& conn, endpoint ep) {
    return connect_async(conn.socket(), conn.runtime().loop(), ep);
}

inline write_all_awaitable write_all_async(tcp_connection& conn, std::vector<char> data) {
    return write_all_async(conn.socket(), conn.runtime().loop(), std::move(data));
}

inline read_some_awaitable read_some_async(tcp_connection& conn, std::span<char> buffer) {
    return read_some_async(conn.socket(), conn.runtime().loop(), buffer);
}

}  // namespace rrmode::netlib::net::simple

#endif
