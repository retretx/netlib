module;

#include <netlib/netlib.hpp>

export module netlib.net;

export namespace rrmode::netlib::execution {
    using executor;
    using scheduler;
    using thread_pool;
    using operation;
}

export namespace rrmode::netlib::net {
    using endpoint;
    using event_loop;
    using net_error;
    using tcp_socket;
    using tcp_acceptor;
}

#if defined(NETLIB_ENABLE_COROUTINES) && NETLIB_ENABLE_COROUTINES
export {
    using rrmode::netlib::execution::task;
    using rrmode::netlib::execution::sync_wait;
}
#endif
