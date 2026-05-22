add_executable(tcp_echo_server server.cpp)
add_executable(tcp_echo_client client.cpp)
add_executable(tcp_echo_simple_client simple_client.cpp)
add_executable(tcp_echo_medium_server medium_server.cpp)

set(TCP_ECHO_TARGETS
    tcp_echo_server
    tcp_echo_client
    tcp_echo_simple_client
    tcp_echo_medium_server
)

if(NETLIB_ENABLE_COROUTINES)
    add_executable(tcp_echo_client_coro client_coro.cpp)
    add_executable(tcp_echo_server_coro server_coro.cpp)
    add_executable(tcp_echo_client_coro_timeout client_coro_timeout.cpp)
    list(APPEND TCP_ECHO_TARGETS tcp_echo_client_coro tcp_echo_server_coro
        tcp_echo_client_coro_timeout)
endif()

foreach(target ${TCP_ECHO_TARGETS})
    target_link_libraries(${target} PRIVATE netlib::netlib)
    target_compile_features(${target} PRIVATE cxx_std_${NETLIB_CXX_STANDARD})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
endforeach()
