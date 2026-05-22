# @file netlib_probes.cmake
# try_compile / probe toolchain. Требует netlib_options.cmake.

include(${CMAKE_CURRENT_LIST_DIR}/netlib_coroutines.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/netlib_features.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/netlib_modules.cmake)

option(NETLIB_ENABLE_COROUTINES
    "Публичный API coroutines (task, co_await); требует probe NETLIB_HAS_COROUTINES"
    ${NETLIB_HAS_COROUTINES})

if(NETLIB_ENABLE_COROUTINES AND NOT NETLIB_HAS_COROUTINES)
    message(FATAL_ERROR "NETLIB_ENABLE_COROUTINES=ON, но toolchain без coroutines")
endif()

if(NETLIB_ENABLE_STD_EXECUTION AND NETLIB_HAS_STD_EXECUTION)
    set(NETLIB_STD_EXECUTION_ACTIVE TRUE)
else()
    set(NETLIB_STD_EXECUTION_ACTIVE FALSE)
    if(NETLIB_ENABLE_STD_EXECUTION AND NOT NETLIB_HAS_STD_EXECUTION)
        message(STATUS "netlib: NETLIB_ENABLE_STD_EXECUTION=ON, но probe std::execution не прошёл — fallback")
    endif()
endif()
