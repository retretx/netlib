#pragma once

#include <netlib/net/detail/socket_backend.hpp>
#include <netlib/net/error.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>

namespace rrmode::netlib::net::detail {

class win_socket_backend final : public socket_backend {
public:
    win_socket_backend() {
        WSADATA wsa{};
        if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            throw net_error("WSAStartup failed");
        }
    }

    int create_tcp_socket() override {
        SOCKET const s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET) {
            throw_wsa("socket");
        }
        return to_fd(s);
    }

    int create_udp_socket() override {
        SOCKET const s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == INVALID_SOCKET) {
            throw_wsa("socket UDP");
        }
        return to_fd(s);
    }

    int create_unix_stream_socket() override {
        throw net_error("AF_UNIX stream: не поддерживается в этой сборке Windows");
    }

    void close(int fd) override { ::closesocket(to_socket(fd)); }

    void set_non_blocking(int fd) override {
        u_long mode = 1;
        if (::ioctlsocket(to_socket(fd), FIONBIO, &mode) != 0) {
            throw_wsa("ioctlsocket FIONBIO");
        }
    }

    void set_reuseaddr(int fd) override {
        BOOL yes = TRUE;
        if (::setsockopt(to_socket(fd), SOL_SOCKET, SO_REUSEADDR,
                         reinterpret_cast<char const*>(&yes), sizeof(yes)) != 0) {
            throw_wsa("setsockopt SO_REUSEADDR");
        }
    }

    void set_tcp_nodelay(int fd, bool on) override {
        BOOL v = on ? TRUE : FALSE;
        if (::setsockopt(to_socket(fd), IPPROTO_TCP, TCP_NODELAY,
                         reinterpret_cast<char const*>(&v), sizeof(v)) != 0) {
            throw_wsa("setsockopt TCP_NODELAY");
        }
    }

    void set_keepalive(int fd, bool on) override {
        BOOL v = on ? TRUE : FALSE;
        if (::setsockopt(to_socket(fd), SOL_SOCKET, SO_KEEPALIVE,
                         reinterpret_cast<char const*>(&v), sizeof(v)) != 0) {
            throw_wsa("setsockopt SO_KEEPALIVE");
        }
    }

    void set_send_buffer_size(int fd, int bytes) override {
        if (::setsockopt(to_socket(fd), SOL_SOCKET, SO_SNDBUF,
                         reinterpret_cast<char const*>(&bytes), sizeof(bytes)) != 0) {
            throw_wsa("setsockopt SO_SNDBUF");
        }
    }

    void set_recv_buffer_size(int fd, int bytes) override {
        if (::setsockopt(to_socket(fd), SOL_SOCKET, SO_RCVBUF,
                         reinterpret_cast<char const*>(&bytes), sizeof(bytes)) != 0) {
            throw_wsa("setsockopt SO_RCVBUF");
        }
    }

    void set_linger(int fd, int seconds) override {
        linger lg{};
        lg.l_onoff = 1;
        lg.l_linger = static_cast<u_short>(seconds);
        if (::setsockopt(to_socket(fd), SOL_SOCKET, SO_LINGER,
                         reinterpret_cast<char const*>(&lg), sizeof(lg)) != 0) {
            throw_wsa("setsockopt SO_LINGER");
        }
    }

    int start_connect(int fd, endpoint const& ep) override {
        auto addr = resolve_ipv4(ep, SOCK_STREAM);
        int rc = ::connect(to_socket(fd), reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        if (rc == 0) {
            return 0;
        }
        if (::WSAGetLastError() == WSAEWOULDBLOCK) {
            return -1;
        }
        throw_wsa("connect");
    }

    int socket_error(int fd) override {
        int err = 0;
        int len = sizeof(err);
        if (::getsockopt(to_socket(fd), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err),
                         &len) != 0) {
            throw_wsa("getsockopt SO_ERROR");
        }
        return err;
    }

    std::optional<std::size_t> try_recv(int fd, std::span<char> buf) override {
        int const n = ::recv(to_socket(fd), buf.data(), static_cast<int>(buf.size()), 0);
        if (n < 0) {
            int const e = ::WSAGetLastError();
            if (e == WSAEWOULDBLOCK) {
                return std::nullopt;
            }
            throw_wsa("recv");
        }
        return static_cast<std::size_t>(n);
    }

    std::optional<std::size_t> try_send(int fd, std::span<char const> buf) override {
        int const n = ::send(to_socket(fd), buf.data(), static_cast<int>(buf.size()), 0);
        if (n < 0) {
            int const e = ::WSAGetLastError();
            if (e == WSAEWOULDBLOCK) {
                return std::nullopt;
            }
            throw_wsa("send");
        }
        return static_cast<std::size_t>(n);
    }

    std::optional<std::size_t> try_recvfrom(int fd, std::span<char> buf, endpoint& out) override {
        sockaddr_in addr{};
        int len = sizeof(addr);
        int const n = ::recvfrom(to_socket(fd), buf.data(), static_cast<int>(buf.size()), 0,
                                 reinterpret_cast<sockaddr*>(&addr), &len);
        if (n < 0) {
            int const e = ::WSAGetLastError();
            if (e == WSAEWOULDBLOCK) {
                return std::nullopt;
            }
            throw_wsa("recvfrom");
        }
        out = endpoint_from_sockaddr(addr);
        return static_cast<std::size_t>(n);
    }

    std::optional<std::size_t> try_sendto(int fd, std::span<char const> buf,
                                          endpoint const& dest) override {
        auto addr = resolve_ipv4(dest, SOCK_DGRAM);
        int const n = ::sendto(to_socket(fd), buf.data(), static_cast<int>(buf.size()), 0,
                               reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        if (n < 0) {
            int const e = ::WSAGetLastError();
            if (e == WSAEWOULDBLOCK) {
                return std::nullopt;
            }
            throw_wsa("sendto");
        }
        return static_cast<std::size_t>(n);
    }

    void bind_unix(int /*fd*/, unix_endpoint const& /*ep*/) override {
        throw net_error("AF_UNIX stream: не поддерживается в этой сборке Windows");
    }

    int start_connect_unix(int /*fd*/, unix_endpoint const& /*ep*/) override {
        throw net_error("AF_UNIX stream: не поддерживается в этой сборке Windows");
    }

    std::string get_bound_unix_path(int /*fd*/) override {
        throw net_error("AF_UNIX stream: не поддерживается в этой сборке Windows");
    }

    void bind_endpoint(int fd, endpoint const& ep) override {
        int socktype = SOCK_STREAM;
        int len = sizeof(socktype);
        if (::getsockopt(to_socket(fd), SOL_SOCKET, SO_TYPE, reinterpret_cast<char*>(&socktype),
                         &len) != 0) {
            throw_wsa("getsockopt SO_TYPE");
        }
        auto addr = resolve_ipv4(ep, socktype);
        if (::bind(to_socket(fd), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            throw_wsa("bind");
        }
    }

    void listen_socket(int fd, int backlog) override {
        if (::listen(to_socket(fd), backlog) != 0) {
            throw_wsa("listen");
        }
    }

    std::uint16_t get_local_port(int fd) override {
        sockaddr_in addr{};
        int len = sizeof(addr);
        if (::getsockname(to_socket(fd), reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
            throw_wsa("getsockname");
        }
        return ntohs(addr.sin_port);
    }

    int accept_nonblocking(int fd) override {
        SOCKET client = ::accept(to_socket(fd), nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            if (::WSAGetLastError() == WSAEWOULDBLOCK) {
                return -1;
            }
            throw_wsa("accept");
        }
        return to_fd(client);
    }

private:
    static SOCKET to_socket(int fd) { return static_cast<SOCKET>(static_cast<UINT_PTR>(fd)); }

    static int to_fd(SOCKET s) { return static_cast<int>(static_cast<UINT_PTR>(s)); }

    [[noreturn]] static void throw_wsa(char const* context) {
        throw net_error(std::string{context} + ": WSA error " + std::to_string(::WSAGetLastError()));
    }

    static endpoint endpoint_from_sockaddr(sockaddr_in const& addr) {
        char host[INET_ADDRSTRLEN]{};
        if (::inet_ntop(AF_INET, &addr.sin_addr, host, INET_ADDRSTRLEN) == nullptr) {
            throw_wsa("inet_ntop");
        }
        return endpoint{.host = host, .port = ntohs(addr.sin_port)};
    }

    static sockaddr_in resolve_ipv4(endpoint const& ep, int socktype) {
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = socktype;
        addrinfo* res = nullptr;
        auto const port_str = std::to_string(ep.port);
        int const gai =
            ::getaddrinfo(ep.host.c_str(), port_str.c_str(), &hints, &res);
        if (gai != 0 || !res) {
            throw net_error(std::string{"getaddrinfo: "} + std::to_string(gai));
        }
        sockaddr_in addr{};
        if (res->ai_family != AF_INET || res->ai_addrlen < sizeof(sockaddr_in)) {
            ::freeaddrinfo(res);
            throw net_error("getaddrinfo: ожидался AF_INET");
        }
        std::memcpy(&addr, res->ai_addr, sizeof(sockaddr_in));
        ::freeaddrinfo(res);
        return addr;
    }
};

}  // namespace rrmode::netlib::net::detail
