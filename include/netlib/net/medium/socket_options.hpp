#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

namespace rrmode::netlib::net::medium {

/// Опциональные параметры TCP-сокета (не заданное поле — не применять).
struct socket_options {
    std::optional<bool> reuseaddr;
    std::optional<bool> tcp_nodelay;
    std::optional<bool> keepalive;
    std::optional<int> send_buffer_size;
    std::optional<int> recv_buffer_size;
    std::optional<int> linger_seconds;
};

}  // namespace rrmode::netlib::net::medium
