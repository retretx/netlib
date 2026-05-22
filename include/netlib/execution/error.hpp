#pragma once

#include <stdexcept>
#include <string>

namespace rrmode::netlib::execution {

/// Ошибка слоя execution (остановка пула, неверное состояние).
class execution_error : public std::runtime_error {
public:
    explicit execution_error(char const* message)
        : std::runtime_error(message) {}
    explicit execution_error(std::string message)
        : std::runtime_error(std::move(message)) {}
};

}  // namespace rrmode::netlib::execution
