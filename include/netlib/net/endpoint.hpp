#pragma once

#include <cstdint>
#include <string>

namespace rrmode::netlib::net {

/// Адрес host:port (заглушка v0).
struct endpoint {
    std::string host;
    std::uint16_t port{0};
};

}  // namespace rrmode::netlib::net
