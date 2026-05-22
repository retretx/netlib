#include <netlib/simple.hpp>

#include <iostream>
#include <span>
#include <string>

int main(int argc, char** argv) {
    auto const port = static_cast<std::uint16_t>(argc > 1 ? std::stoi(argv[1]) : 9001);

    rrmode::netlib::net::simple::io_runtime runtime;
    rrmode::netlib::net::simple::tcp_connection conn{runtime};

    conn.connect({.host = "127.0.0.1", .port = port});
    conn.write() << "hello";
    // flush в destructor write_stream

    char buf[64]{};
    auto const n = conn.read_some(std::span<char>{buf, sizeof(buf)});
    std::cout.write(buf, static_cast<std::streamsize>(n));
    std::cout << '\n';
    return 0;
}
