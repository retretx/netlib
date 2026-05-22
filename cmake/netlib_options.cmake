# @file netlib_options.cmake
# Все пользовательские опции netlib (CACHE). Подключать сразу после project().

if(DEFINED NETLIB_OPTIONS_INCLUDED)
    return()
endif()
set(NETLIB_OPTIONS_INCLUDED TRUE)

# --- C++ toolchain (потребители и subprojects наследуют при add_subdirectory) ---
set(NETLIB_CXX_STANDARD
    "23"
    CACHE STRING "Стандарт C++ для targets netlib и встроенных примеров/тестов")
set_property(CACHE NETLIB_CXX_STANDARD PROPERTY STRINGS 20 23)

option(NETLIB_CXX_STANDARD_REQUIRED "Требовать NETLIB_CXX_STANDARD строго" ON)
option(NETLIB_CXX_EXTENSIONS "Разрешить расширения компилятора (gnu++…)" OFF)

set(CMAKE_CXX_STANDARD ${NETLIB_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ${NETLIB_CXX_STANDARD_REQUIRED})
set(CMAKE_CXX_EXTENSIONS ${NETLIB_CXX_EXTENSIONS})

# --- Компоненты дерева проекта ---
option(NETLIB_BUILD_TESTS "Собирать Catch2: netlib_unit, netlib_integration" ON)
option(NETLIB_BUILD_EXAMPLES "Собирать examples/ (см. NETLIB_EXAMPLE_BUILD_*)" OFF)
option(NETLIB_BUILD_BENCHMARKS "Собирать benchmarks/ (см. NETLIB_BENCHMARK_BUILD_*)" OFF)
option(NETLIB_BUILD_MODULES "Собирать C++20 modules (netlib::simple, netlib::medium)" OFF)

# --- Установка / packaging ---
option(NETLIB_ENABLE_INSTALL "install() + CMake package config (netlibConfig.cmake)" ON)
set(NETLIB_PACKAGE_COMPATIBILITY
    "SameMajorVersion"
    CACHE STRING "COMPATIBILITY для write_basic_package_version_file")
set_property(CACHE NETLIB_PACKAGE_COMPATIBILITY PROPERTY STRINGS
    AnyNewerVersion SameMajorVersion SameMinorVersion ExactVersion)

# --- Зависимости платформы ---
option(NETLIB_USE_THREADS "find_package(Threads) и линковка Threads::Threads" ON)
option(NETLIB_LINK_WINSOCK "На Windows линковать ws2_32 с netlib::netlib" ON)

# --- std::execution backend (probe в netlib_features.cmake) ---
option(NETLIB_ENABLE_STD_EXECUTION
    "Экспортировать NETLIB_HAS_STD_EXECUTION=1 если probe успешен" ON)

# --- Coroutines (probe в netlib_coroutines.cmake; option ниже в netlib_probes.cmake) ---

# --- Catch2 (только NETLIB_BUILD_TESTS) ---
set(NETLIB_CATCH2_REPOSITORY
    "https://github.com/catchorg/Catch2.git"
    CACHE STRING "FetchContent: репозиторий Catch2")
set(NETLIB_CATCH2_TAG "v3.7.1" CACHE STRING "FetchContent: тег/ветка Catch2")
option(NETLIB_CATCH2_GIT_SHALLOW "FetchContent: GIT_SHALLOW для Catch2" ON)

# --- examples/ (при NETLIB_BUILD_EXAMPLES) ---
option(NETLIB_EXAMPLE_BUILD_TCP "examples/tcp_echo" ON)
option(NETLIB_EXAMPLE_BUILD_UDP "examples/udp_echo" ON)
option(NETLIB_EXAMPLE_BUILD_UNIX "examples/unix_echo (POSIX)" ON)
option(NETLIB_EXAMPLES_REQUIRE_POSIX
    "На Windows не собирать examples даже при NETLIB_BUILD_EXAMPLES=ON" ON)

# --- benchmarks/ (при NETLIB_BUILD_BENCHMARKS) ---
option(NETLIB_BENCHMARK_BUILD_SCHEDULE "benchmarks/schedule_bench (везде)" ON)
option(NETLIB_BENCHMARK_BUILD_NETWORK
    "benchmarks/tcp_echo_bench, udp_ping_bench (POSIX + coroutines)" ON)
option(NETLIB_BENCHMARKS_REQUIRE_POSIX
    "На Windows не собирать сетевые benchmarks" ON)
option(NETLIB_BENCHMARKS_REQUIRE_COROUTINES
    "Сетевые benchmarks только при NETLIB_ENABLE_COROUTINES" ON)

