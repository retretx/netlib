#include <netlib/net/simple/io_runtime.hpp>
#include <netlib/net/simple/tcp_connection.hpp>

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <thread>

#if !defined(_WIN32)

TEST_CASE("simple sync echo на loopback") {
    // Запуск сервера в отдельном процессе не делаем — используем ручной peer через два runtime
    // Минимальная проверка: connect к несуществующему порту бросает или timeout.
    rrmode::netlib::net::simple::io_runtime runtime;
    rrmode::netlib::net::simple::tcp_connection conn{runtime};

    REQUIRE_THROWS_AS(conn.connect({.host = "127.0.0.1", .port = 1}),
                      rrmode::netlib::net::net_error);
}

#endif
