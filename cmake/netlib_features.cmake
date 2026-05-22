# Определение возможностей toolchain для netlib::netlib

include(CheckCXXSourceCompiles)

# P2300 / std::execution: __cpp_lib_execution и успешная компиляция заголовка
check_cxx_source_compiles(
    "
#include <version>
#if !defined(__cpp_lib_execution) || (__cpp_lib_execution < 202400L)
#  error \"std::execution недоступен\"
#endif
#include <execution>
int main() { return 0; }
"
    NETLIB_HAS_STD_EXECUTION
)

if(NETLIB_HAS_STD_EXECUTION)
    message(STATUS "netlib: NETLIB_HAS_STD_EXECUTION=ON (std::execution)")
else()
    message(STATUS "netlib: NETLIB_HAS_STD_EXECUTION=OFF (fallback backend)")
endif()

check_cxx_source_compiles(
    "
#include <functional>
int main() {
    std::move_only_function<void()> f;
    (void)f;
    return 0;
}
"
    NETLIB_HAS_MOVE_ONLY_FUNCTION
)

if(NETLIB_HAS_MOVE_ONLY_FUNCTION)
    message(STATUS "netlib: std::move_only_function доступен")
else()
    message(STATUS "netlib: std::move_only_function недоступен — polyfill через -include")
endif()
