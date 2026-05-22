#pragma once

#include <execution>
#include <functional>

namespace rrmode::netlib::execution::detail::std_backend {

/// Заглушка моста к P2300: полная интеграция sender/receiver — v1.1+.
/// Сейчас фиксируем доступность заголовка и inline_scheduler для будущих адаптеров.
inline constexpr bool available = true;

// Проверка компиляции std::execution в TU, где подключён backend.
using probe_scheduler = std::execution::inline_scheduler;

}  // namespace rrmode::netlib::execution::detail::std_backend
