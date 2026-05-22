# @file netlib_target.cmake
# INTERFACE target netlib::netlib. Требует netlib_options.cmake и netlib_probes.cmake.

add_library(netlib INTERFACE)
add_library(netlib::netlib ALIAS netlib)

target_include_directories(netlib INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(netlib INTERFACE cxx_std_${NETLIB_CXX_STANDARD})

if(NOT NETLIB_HAS_MOVE_ONLY_FUNCTION)
    target_compile_options(netlib INTERFACE
        "$<$<COMPILE_LANGUAGE:CXX>:-include=${CMAKE_CURRENT_SOURCE_DIR}/include/netlib/detail/move_only_function.hpp>"
    )
endif()

if(NETLIB_ENABLE_COROUTINES)
    target_compile_definitions(netlib INTERFACE NETLIB_ENABLE_COROUTINES=1)
    message(STATUS "netlib: NETLIB_ENABLE_COROUTINES=ON")
endif()

if(NETLIB_STD_EXECUTION_ACTIVE)
    target_compile_definitions(netlib INTERFACE NETLIB_HAS_STD_EXECUTION=1)
endif()

if(NETLIB_USE_THREADS)
    find_package(Threads REQUIRED)
    target_link_libraries(netlib INTERFACE Threads::Threads)
endif()

if(WIN32 AND NETLIB_LINK_WINSOCK)
    target_link_libraries(netlib INTERFACE ws2_32)
endif()

if(NETLIB_BUILD_MODULES AND NETLIB_HAS_MODULES)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/modules ${CMAKE_BINARY_DIR}/modules)
    message(STATUS "netlib: NETLIB_BUILD_MODULES=ON")
endif()
