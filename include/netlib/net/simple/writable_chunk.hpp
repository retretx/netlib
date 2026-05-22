#pragma once

#include <concepts>
#include <cstddef>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace rrmode::netlib::net::simple {

template <typename T>
concept writable_chunk =
    std::same_as<T, char> || std::same_as<T, std::byte> ||
    std::convertible_to<T, std::string_view> ||
    (requires(T const& t) {
        { t.data() } -> std::convertible_to<char const*>;
        { t.size() } -> std::convertible_to<std::size_t>;
    });

namespace detail {

inline void append_chunk(std::vector<char>& buf, char c) { buf.push_back(c); }

inline void append_chunk(std::vector<char>& buf, std::byte b) {
    buf.push_back(static_cast<char>(b));
}

inline void append_chunk(std::vector<char>& buf, std::string_view sv) {
    buf.insert(buf.end(), sv.begin(), sv.end());
}

template <writable_chunk T>
void append_chunk(std::vector<char>& buf, T const& chunk) {
    if constexpr (std::convertible_to<T, std::string_view>) {
        append_chunk(buf, std::string_view{chunk});
    } else {
        auto const* data = chunk.data();
        auto const n = chunk.size();
        buf.insert(buf.end(), data, data + static_cast<std::ptrdiff_t>(n));
    }
}

}  // namespace detail

}  // namespace rrmode::netlib::net::simple
