#pragma once

#include <stdexcept>
#include <string>

namespace rrmode::netlib::net {

class net_error : public std::runtime_error {
public:
    explicit net_error(char const* message) : std::runtime_error(message) {}
    explicit net_error(std::string message) : std::runtime_error(std::move(message)) {}
};

}  // namespace rrmode::netlib::net
