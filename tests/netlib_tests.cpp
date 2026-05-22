#include <netlib/config.hpp>
#include <netlib/execution/detail/backend.hpp>
#include <netlib/execution/thread_pool.hpp>
#include <netlib/net/endpoint.hpp>
#include <netlib/net/tcp_socket.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("заголовки netlib компилируются") {
    rrmode::netlib::execution::thread_pool pool{1};
    REQUIRE(pool.thread_count() == 1);
    pool.shutdown();
    REQUIRE(rrmode::netlib::version_major == 1);
    REQUIRE(rrmode::netlib::version_minor == 0);
    REQUIRE(rrmode::netlib::version_patch == 0);
}

TEST_CASE("backend отражает NETLIB_HAS_STD_EXECUTION") {
    REQUIRE(rrmode::netlib::has_std_execution ==
            rrmode::netlib::execution::detail::use_std_execution);
}

TEST_CASE("tcp_socket создаётся закрытым") {
    rrmode::netlib::net::tcp_socket sock;
    REQUIRE_FALSE(sock.is_open());
}
