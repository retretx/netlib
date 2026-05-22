cmake_minimum_required(VERSION 3.24)

project(netlib VERSION 1.0.0 LANGUAGES CXX)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/netlib_options.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/netlib_probes.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/netlib_target.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/netlib_install.cmake)

if(NETLIB_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(NETLIB_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(NETLIB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
