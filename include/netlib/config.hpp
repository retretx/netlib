#pragma once

/// Версия библиотеки netlib (semver, заголовочная фиксация до install/export).
#define NETLIB_VERSION_MAJOR 1
#define NETLIB_VERSION_MINOR 0
#define NETLIB_VERSION_PATCH 0

namespace rrmode::netlib {

inline constexpr int version_major = NETLIB_VERSION_MAJOR;
inline constexpr int version_minor = NETLIB_VERSION_MINOR;
inline constexpr int version_patch = NETLIB_VERSION_PATCH;

/// true, если при сборке обнаружен std::execution (P2300).
inline constexpr bool has_std_execution =
#if defined(NETLIB_HAS_STD_EXECUTION) && NETLIB_HAS_STD_EXECUTION
    true;
#else
    false;
#endif

}  // namespace rrmode::netlib
