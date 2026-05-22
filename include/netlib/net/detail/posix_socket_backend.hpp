#pragma once

#include <netlib/net/detail/socket_backend.hpp>
#include <netlib/net/error.hpp>

#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <optional>
#include <span>
#include <string>

namespace rrmode::netlib::net::detail {

class posix_socket_backend final : public socket_backend {
public:
    int create_tcp_socket() override {
        int const fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            throw_errno("socket");
        }
        return fd;
    }

    int create_udp_socket() override {
        int const fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0) {
            throw_errno("socket UDP");
        }
        return fd;
    }

    int create_unix_stream_socket() override {
        int const fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
            throw_errno("socket AF_UNIX");
        }
        return fd;
    }

    void close(int fd) override { ::close(fd); }

    void set_non_blocking(int fd) override {
        int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags < 0) {
            throw_errno("fcntl F_GETFL");
        }
        if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            throw_errno("fcntl F_SETFL O_NONBLOCK");
        }
    }

    void set_reuseaddr(int fd) override {
        int yes = 1;
        if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0) {
            throw_errno("setsockopt SO_REUSEADDR");
        }
    }

    void set_tcp_nodelay(int fd, bool on) override {
        int v = on ? 1 : 0;
        if (::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &v, sizeof(v)) != 0) {
            throw_errno("setsockopt TCP_NODELAY");
        }
    }

    void set_keepalive(int fd, bool on) override {
        int v = on ? 1 : 0;
        if (::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &v, sizeof(v)) != 0) {
            throw_errno("setsockopt SO_KEEPALIVE");
        }
    }

    void set_send_buffer_size(int fd, int bytes) override {
        if (::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &bytes, sizeof(bytes)) != 0) {
            throw_errno("setsockopt SO_SNDBUF");
        }
    }

    void set_recv_buffer_size(int fd, int bytes) override {
        if (::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &bytes, sizeof(bytes)) != 0) {
            throw_errno("setsockopt SO_RCVBUF");
        }
    }

    void set_linger(int fd, int seconds) override {
        linger lg{.l_onoff = 1, .l_linger = static_cast<decltype(lg.l_linger)>(seconds)};
        if (::setsockopt(fd, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg)) != 0) {
            throw_errno("setsockopt SO_LINGER");
        }
    }

    int start_connect_unix(int fd, unix_endpoint const& ep) override {
        auto addr = make_unix_addr(ep);
        int rc = ::connect(fd, reinterpret_cast<sockaddr*>(&addr), unix_addr_len(addr));
        if (rc == 0) {
            return 0;
        }
        if (errno == EINPROGRESS) {
            return -1;
        }
        throw_errno("connect unix");
    }

    int start_connect(int fd, endpoint const& ep) override {
        auto addr = resolve_ipv4(ep, SOCK_STREAM);
        int rc = ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        if (rc == 0) {
            return 0;
        }
        if (errno == EINPROGRESS) {
            return -1;
        }
        throw_errno("connect");
    }

    int socket_error(int fd) override {
        int err = 0;
        socklen_t len = sizeof(err);
        if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) != 0) {
            throw_errno("getsockopt SO_ERROR");
        }
        return err;
    }

    std::optional<std::size_t> try_recv(int fd, std::span<char> buf) override {
        ssize_t n = ::recv(fd, buf.data(), buf.size(), 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return std::nullopt;
            }
            throw_errno("recv");
        }
        return static_cast<std::size_t>(n);
    }

    std::optional<std::size_t> try_send(int fd, std::span<char const> buf) override {
        ssize_t n = ::send(fd, buf.data(), buf.size(), MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return std::nullopt;
            }
            throw_errno("send");
        }
        return static_cast<std::size_t>(n);
    }

    std::optional<std::size_t> try_recvfrom(int fd, std::span<char> buf, endpoint& out) override {
        sockaddr_in addr{};
        socklen_t len = sizeof(addr);
        ssize_t const n = ::recvfrom(fd, buf.data(), buf.size(), 0,
                                     reinterpret_cast<sockaddr*>(&addr), &len);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return std::nullopt;
            }
            throw_errno("recvfrom");
        }
        out = endpoint_from_sockaddr(addr);
        return static_cast<std::size_t>(n);
    }

    std::optional<std::size_t> try_sendto(int fd, std::span<char const> buf,
                                          endpoint const& dest) override {
        auto addr = resolve_ipv4(dest, SOCK_DGRAM);
        ssize_t const n =
            ::sendto(fd, buf.data(), buf.size(), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return std::nullopt;
            }
            throw_errno("sendto");
        }
        return static_cast<std::size_t>(n);
    }

    void bind_unix(int fd, unix_endpoint const& ep) override {
        (void)::unlink(ep.path.c_str());
        auto addr = make_unix_addr(ep);
        if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), unix_addr_len(addr)) != 0) {
            throw_errno("bind unix");
        }
    }

    std::string get_bound_unix_path(int fd) override {
        sockaddr_un addr{};
        socklen_t len = sizeof(addr);
        if (::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
            throw_errno("getsockname unix");
        }
        return std::string{addr.sun_path};
    }

    void bind_endpoint(int fd, endpoint const& ep) override {
        int socktype = SOCK_STREAM;
        socklen_t len = sizeof(socktype);
        if (::getsockopt(fd, SOL_SOCKET, SO_TYPE, &socktype, &len) != 0) {
            throw_errno("getsockopt SO_TYPE");
        }
        auto addr = resolve_ipv4(ep, socktype);
        if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            throw_errno("bind");
        }
    }

    void listen_socket(int fd, int backlog) override {
        if (::listen(fd, backlog) != 0) {
            throw_errno("listen");
        }
    }

    std::uint16_t get_local_port(int fd) override {
        sockaddr_in addr{};
        socklen_t len = sizeof(addr);
        if (::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
            throw_errno("getsockname");
        }
        return ntohs(addr.sin_port);
    }

    int accept_nonblocking(int fd) override {
        int client = ::accept(fd, nullptr, nullptr);
        if (client < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return -1;
            }
            throw_errno("accept");
        }
        return client;
    }

private:
    [[noreturn]] static void throw_errno(char const* context) {
        throw net_error(std::string{context} + ": " + std::strerror(errno));
    }

    static endpoint endpoint_from_sockaddr(sockaddr_in const& addr) {
        char host[INET_ADDRSTRLEN]{};
        if (::inet_ntop(AF_INET, &addr.sin_addr, host, sizeof(host)) == nullptr) {
            throw_errno("inet_ntop");
        }
        return endpoint{.host = host, .port = ntohs(addr.sin_port)};
    }

    static sockaddr_un make_unix_addr(unix_endpoint const& ep) {
        if (ep.path.empty()) {
            throw net_error("unix: пустой путь");
        }
        if (ep.path.size() >= sizeof(sockaddr_un::sun_path)) {
            throw net_error("unix: путь слишком длинный");
        }
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::memcpy(addr.sun_path, ep.path.c_str(), ep.path.size());
        return addr;
    }

    static socklen_t unix_addr_len(sockaddr_un const& addr) {
        return static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + std::strlen(addr.sun_path));
    }

    static sockaddr_in resolve_ipv4(endpoint const& ep, int socktype) {
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = socktype;
        addrinfo* res = nullptr;
        auto const port_str = std::to_string(ep.port);
        int const gai = ::getaddrinfo(ep.host.c_str(), port_str.c_str(), &hints, &res);
        if (gai != 0 || !res) {
            throw net_error(std::string{"getaddrinfo: "} + gai_strerror(gai));
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
