# Probe: поддержка CXX_MODULES в CMake/компиляторе.
include(CheckCXXSourceCompiles)

set(_netlib_modules_probe
    [=[
    import std;
    int main() { return 0; }
    ]=]
)

if(CMAKE_VERSION VERSION_LESS 3.28)
    set(NETLIB_HAS_MODULES FALSE)
else()
    check_cxx_source_compiles("${_netlib_modules_probe}" NETLIB_HAS_MODULES)
endif()

if(NETLIB_BUILD_MODULES AND NOT NETLIB_HAS_MODULES)
    message(WARNING "NETLIB_BUILD_MODULES=ON, но toolchain не прошёл probe modules — модули отключены")
    set(NETLIB_BUILD_MODULES OFF CACHE BOOL "" FORCE)
endif()
