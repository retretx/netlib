include(CheckCXXSourceCompiles)

check_cxx_source_compiles(
    "
#include <coroutine>
struct task {
    struct promise_type {
        task get_return_object() { return {}; }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
};
task f() { co_return; }
int main() { return 0; }
"
    NETLIB_HAS_COROUTINES
)

if(NETLIB_HAS_COROUTINES)
    message(STATUS "netlib: coroutines доступны (NETLIB_ENABLE_COROUTINES)")
else()
    message(STATUS "netlib: coroutines недоступны в toolchain")
endif()
