#pragma once

#include <netlib/execution/error.hpp>

namespace rrmode::netlib::execution {

/// Операция не завершилась в отведённый срок.
class timeout_error : public execution_error {
public:
    explicit timeout_error(char const* message) : execution_error(message) {}
    explicit timeout_error(std::string message) : execution_error(std::move(message)) {}
};

}  // namespace rrmode::netlib::execution
