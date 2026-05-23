#pragma once

// Polyfill std::move_only_function when the standard library does not provide it
// (e.g. libc++ on Apple Clang in CI). Included from headers that use std::move_only_function.

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <version>

#if !defined(NETLIB_POLYFILL_MOVE_ONLY_FUNCTION) && defined(__cpp_lib_move_only_function) \
    && (__cpp_lib_move_only_function >= 202110L)
// std::move_only_function is available.
#else

namespace rrmode::netlib::detail {

template<typename>
class move_only_function;

template<typename R, typename... Args>
class move_only_function<R(Args...)> {
    struct impl_base {
        virtual ~impl_base() = default;
        virtual R call(Args... args) = 0;
    };

    template<typename F>
    struct impl final : impl_base {
        F fn;

        explicit impl(F&& f) : fn(std::move(f)) {}

        R call(Args... args) override {
            if constexpr (std::is_void_v<R>) {
                fn(std::forward<Args>(args)...);
            } else {
                return fn(std::forward<Args>(args)...);
            }
        }
    };

    std::unique_ptr<impl_base> impl_{};

public:
    move_only_function() noexcept = default;

    move_only_function(std::nullptr_t) noexcept {}

    move_only_function(move_only_function&& other) noexcept = default;
    move_only_function& operator=(move_only_function&& other) noexcept = default;

    move_only_function(move_only_function const&) = delete;
    move_only_function& operator=(move_only_function const&) = delete;

    template<typename F>
        requires std::is_invocable_r_v<R, F&, Args...> && (!std::is_same_v<std::decay_t<F>, move_only_function>)
    move_only_function(F&& f) : impl_{std::make_unique<impl<std::decay_t<F>>>(std::forward<F>(f))} {}

    explicit operator bool() const noexcept { return static_cast<bool>(impl_); }

    R operator()(Args... args) const {
        if (!impl_) {
            throw std::bad_function_call{};
        }
        return impl_->call(std::forward<Args>(args)...);
    }
};

}  // namespace rrmode::netlib::detail

namespace std {
using rrmode::netlib::detail::move_only_function;
}  // namespace std

#endif
