module;

#include <netlib/simple.hpp>

export module netlib.net.simple;

export namespace rrmode::netlib::net::simple {
    using io_runtime;
    using tcp_connection;
    using write_stream;
    using writable_chunk;
}  // namespace rrmode::netlib::net::simple

#if defined(NETLIB_ENABLE_COROUTINES) && NETLIB_ENABLE_COROUTINES
export {
    using rrmode::netlib::net::simple::connect_async;
    using rrmode::netlib::net::simple::write_all_async;
    using rrmode::netlib::net::simple::read_some_async;
}
#endif
