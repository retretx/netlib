if(WIN32 AND NETLIB_EXAMPLES_REQUIRE_POSIX)
    message(STATUS "netlib: примеры пропущены на Windows (NETLIB_EXAMPLES_REQUIRE_POSIX=ON)")
    return()
endif()

if(NETLIB_EXAMPLE_BUILD_TCP)
    add_subdirectory(tcp_echo)
endif()

if(NETLIB_EXAMPLE_BUILD_UDP)
    add_subdirectory(udp_echo)
endif()

if(NETLIB_EXAMPLE_BUILD_UNIX)
    add_subdirectory(unix_echo)
endif()
