#pragma once

#include <cstdint>

namespace rrmode::netlib::net::detail {

enum class poll_event : std::uint32_t {
    none = 0,
    readable = 1,
    writable = 2,
};

[[nodiscard]] constexpr poll_event operator|(poll_event a, poll_event b) noexcept {
    return static_cast<poll_event>(static_cast<std::uint32_t>(a) |
                                   static_cast<std::uint32_t>(b));
}

[[nodiscard]] constexpr bool has_event(poll_event mask, poll_event flag) noexcept {
    return (static_cast<std::uint32_t>(mask) & static_cast<std::uint32_t>(flag)) != 0;
}

}  // namespace rrmode::netlib::net::detail
