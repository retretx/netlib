#pragma once

#include <string>

namespace rrmode::netlib::net {

/// Путь к UNIX domain socket (файловый сокет, не abstract `@`).
struct unix_endpoint {
    std::string path;
};

}  // namespace rrmode::netlib::net
