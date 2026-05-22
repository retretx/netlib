#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/event_loop.hpp>
#include <netlib/net/detail/default_socket_backend.hpp>
#include <netlib/net/tcp_socket.hpp>

int main() {
    rrmode::netlib::execution::thread_pool pool{1};
    rrmode::netlib::execution::scheduler sched{pool};
    rrmode::netlib::net::event_loop loop{sched};
    rrmode::netlib::net::tcp_socket sock;
    (void)loop;
    (void)sock;
    pool.shutdown();
    return 0;
}
