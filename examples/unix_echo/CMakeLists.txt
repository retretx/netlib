if(NOT NETLIB_ENABLE_COROUTINES)
    return()
endif()

add_executable(unix_echo_server_coro server_coro.cpp)
add_executable(unix_echo_client_coro client_coro.cpp)
add_executable(unix_echo_client_coro_timeout client_coro_timeout.cpp)

set(UNIX_ECHO_TARGETS unix_echo_server_coro unix_echo_client_coro unix_echo_client_coro_timeout)

foreach(target ${UNIX_ECHO_TARGETS})
    target_link_libraries(${target} PRIVATE netlib::netlib)
    target_compile_features(${target} PRIVATE cxx_std_${NETLIB_CXX_STANDARD})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
endforeach()
