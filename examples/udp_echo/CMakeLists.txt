add_executable(udp_echo_server server.cpp)
add_executable(udp_echo_client_coro client_coro.cpp)

set(UDP_ECHO_TARGETS udp_echo_server)

if(NETLIB_ENABLE_COROUTINES)
    add_executable(udp_echo_server_coro server_coro.cpp)
    list(APPEND UDP_ECHO_TARGETS udp_echo_client_coro udp_echo_server_coro)
endif()

foreach(target ${UDP_ECHO_TARGETS})
    target_link_libraries(${target} PRIVATE netlib::netlib)
    target_compile_features(${target} PRIVATE cxx_std_${NETLIB_CXX_STANDARD})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
endforeach()
