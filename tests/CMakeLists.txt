include(FetchContent)

set(_netlib_catch2_shallow "${NETLIB_CATCH2_GIT_SHALLOW}")
if(_netlib_catch2_shallow)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY ${NETLIB_CATCH2_REPOSITORY}
        GIT_TAG ${NETLIB_CATCH2_TAG}
        GIT_SHALLOW TRUE)
else()
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY ${NETLIB_CATCH2_REPOSITORY}
        GIT_TAG ${NETLIB_CATCH2_TAG})
endif()
FetchContent_MakeAvailable(Catch2)

find_package(Threads REQUIRED)

# --- unit (фейки, TDD) ---
set(NETLIB_UNIT_SOURCES
    unit/net/tcp_socket_read_tests.cpp
        unit/net/udp_socket_tests.cpp
        unit/net/udp_coro_tests.cpp
    unit/net/tcp_socket_connect_tests.cpp
    unit/net/tcp_socket_write_tests.cpp
    unit/net/tcp_acceptor_tests.cpp
    unit/net/unix_stream_tests.cpp
    unit/net/tcp_socket_lifetime_tests.cpp
    unit/net/simple_tcp_connection_tests.cpp
    unit/net/medium_socket_options_tests.cpp
)
if(NETLIB_ENABLE_COROUTINES)
    list(APPEND NETLIB_UNIT_SOURCES
        unit/net/simple_coro_tests.cpp
        unit/execution/task_tests.cpp
        unit/execution/when_all_tests.cpp
        unit/execution/when_all_vector_tests.cpp
        unit/execution/when_all_void_tests.cpp
        unit/execution/when_all_tuple_tests.cpp
        unit/execution/generator_tests.cpp
        unit/execution/spawn_tests.cpp
        unit/execution/then_tests.cpp
        unit/execution/delay_tests.cpp
        unit/execution/when_any_tests.cpp
        unit/execution/when_any_abort_tests.cpp
        unit/net/connect_awaitable_tests.cpp
        unit/net/accept_awaitable_tests.cpp
        unit/net/coro_tcp_tests.cpp
        unit/net/read_exact_tests.cpp
        unit/net/delay_loop_tests.cpp
        unit/net/connect_timeout_tests.cpp
        unit/net/connect_unix_timeout_tests.cpp
        unit/net/read_unix_timeout_tests.cpp
        unit/net/cancel_io_tests.cpp
        unit/net/cancellation_coro_tests.cpp
        unit/net/timeout_cancel_tests.cpp
        unit/net/accept_cancel_tests.cpp
        unit/net/tcp_coro_tests.cpp
        unit/net/accept_timeout_tests.cpp
        unit/net/unix_awaitable_tests.cpp
        unit/net/read_timeout_tests.cpp
    )
endif()

add_executable(netlib_unit_tests ${NETLIB_UNIT_SOURCES})
target_include_directories(netlib_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/fakes
)
target_link_libraries(netlib_unit_tests PRIVATE
    netlib::netlib
    Catch2::Catch2WithMain
    Threads::Threads
)
add_test(NAME netlib_unit COMMAND netlib_unit_tests)

# --- integration (реальный epoll / POSIX) ---
set(NETLIB_INTEGRATION_SOURCES
    netlib_tests.cpp
    execution_tests.cpp
    integration/net_tests.cpp
    integration/tcp_echo_tests.cpp
    integration/simple_tcp_tests.cpp
    integration/timerfd_tests.cpp
    integration/udp_loopback_tests.cpp
    integration/unix_stream_loopback_tests.cpp
)
if(NETLIB_ENABLE_COROUTINES)
    list(APPEND NETLIB_INTEGRATION_SOURCES
        integration/tcp_echo_coro_tests.cpp
        integration/server_coro_shutdown_tests.cpp
        integration/udp_echo_coro_tests.cpp
        integration/unix_echo_coro_tests.cpp
        integration/unix_server_coro_shutdown_tests.cpp
        integration/udp_server_coro_shutdown_tests.cpp
    )
endif()

add_executable(netlib_integration_tests ${NETLIB_INTEGRATION_SOURCES})
target_link_libraries(netlib_integration_tests PRIVATE
    netlib::netlib
    Catch2::Catch2WithMain
    Threads::Threads
)
add_test(NAME netlib_integration COMMAND netlib_integration_tests)
