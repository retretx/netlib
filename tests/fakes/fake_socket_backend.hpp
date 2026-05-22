#pragma once

#include <netlib/net/detail/socket_backend.hpp>
#include <netlib/net/error.hpp>
#include <netlib/net/unix_endpoint.hpp>

#include <deque>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace netlib::fakes {

class fake_reactor;

/// In-memory сокеты с peer-связью и буферами.
class fake_socket_backend final : public rrmode::netlib::net::detail::socket_backend {
public:
    struct datagram {
        rrmode::netlib::net::endpoint remote;
        std::vector<char> payload;
    };

    struct socket_state {
        std::deque<char> rx;
        std::deque<char> tx;
        int peer{-1};
        bool closed{false};
        bool non_blocking{true};
        bool connect_done{true};
        bool rx_eof{false};
        int so_error{0};
        bool is_listener{false};
        bool is_udp{false};
        bool is_unix{false};
        std::string bound_unix_path;
        std::uint16_t bound_port{0};
        std::vector<int> accept_queue;
        std::deque<datagram> datagram_rx;
        int udp_peer{-1};
    };

    void attach_reactor(fake_reactor& reactor) { reactor_ = &reactor; }

    int create_tcp_socket() override {
        int const fd = ++next_fd_;
        auto& s = sockets_[fd];
        s = socket_state{};
        s.connect_done = false;
        return fd;
    }

    int create_udp_socket() override {
        int const fd = ++next_fd_;
        auto& s = sockets_[fd];
        s = socket_state{};
        s.is_udp = true;
        s.connect_done = true;
        return fd;
    }

    int create_unix_stream_socket() override {
        int const fd = ++next_fd_;
        auto& s = sockets_[fd];
        s = socket_state{};
        s.is_unix = true;
        s.connect_done = false;
        return fd;
    }

    void close(int fd) override {
        auto it = sockets_.find(fd);
        if (it != sockets_.end()) {
            it->second.closed = true;
        }
    }

    void set_non_blocking(int fd) override {
        sockets_[fd].non_blocking = true;
    }

    void set_reuseaddr(int fd) override { applied_[fd].reuseaddr = true; }

    void set_tcp_nodelay(int fd, bool on) override { applied_[fd].tcp_nodelay = on; }

    void set_keepalive(int fd, bool on) override { applied_[fd].keepalive = on; }

    void set_send_buffer_size(int fd, int bytes) override {
        applied_[fd].send_buffer_size = bytes;
    }

    void set_recv_buffer_size(int fd, int bytes) override {
        applied_[fd].recv_buffer_size = bytes;
    }

    void set_linger(int fd, int seconds) override { applied_[fd].linger_seconds = seconds; }

    int start_connect_unix(int fd, rrmode::netlib::net::unix_endpoint const& ep) override {
        auto& s = sockets_[fd];
        if (ep.path == "pending") {
            s.connect_done = false;
            return -1;
        }
        auto const it = unix_listeners_.find(ep.path);
        if (it == unix_listeners_.end()) {
            s.so_error = 111;  // ECONNREFUSED
            s.connect_done = false;
            return -1;
        }
        int const listener_fd = it->second;
        int const server_fd = ++next_fd_;
        auto& server = sockets_[server_fd];
        server = socket_state{};
        server.is_unix = true;
        server.connect_done = true;
        link_peers(fd, server_fd);
        enqueue_accept(listener_fd, server_fd);
        s.connect_done = true;
        s.so_error = 0;
        return 0;
    }

    int start_connect(int fd, rrmode::netlib::net::endpoint const& ep) override {
        auto& s = sockets_[fd];
        if (ep.host == "immediate" || s.connect_done) {
            s.connect_done = true;
            return 0;
        }
        return -1;
    }

    int socket_error(int fd) override { return sockets_[fd].so_error; }

    std::optional<std::size_t> try_recvfrom(int fd, std::span<char> buf,
                                            rrmode::netlib::net::endpoint& out) override {
        auto& s = sockets_.at(fd);
        if (!s.is_udp) {
            throw rrmode::netlib::net::net_error("fake recvfrom: не UDP");
        }
        if (s.closed) {
            throw rrmode::netlib::net::net_error("fake recvfrom: closed");
        }
        if (s.datagram_rx.empty()) {
            if (s.non_blocking) {
                return std::nullopt;
            }
            return std::size_t{0};
        }
        auto dg = std::move(s.datagram_rx.front());
        s.datagram_rx.pop_front();
        out = dg.remote;
        std::size_t const n = std::min(buf.size(), dg.payload.size());
        for (std::size_t i = 0; i < n; ++i) {
            buf[i] = dg.payload[i];
        }
        return n;
    }

    std::optional<std::size_t> try_sendto(int fd, std::span<char const> buf,
                                          rrmode::netlib::net::endpoint const& dest) override {
        auto& s = sockets_.at(fd);
        if (!s.is_udp) {
            throw rrmode::netlib::net::net_error("fake sendto: не UDP");
        }
        if (s.closed) {
            throw rrmode::netlib::net::net_error("fake sendto: closed");
        }
        rrmode::netlib::net::endpoint const src{.host = "127.0.0.1", .port = s.bound_port};
        if (s.udp_peer >= 0) {
            deliver_datagram(s.udp_peer, src, buf);
            return buf.size();
        }
        route_datagram(src, dest, buf);
        return buf.size();
    }

    std::optional<std::size_t> try_recv(int fd, std::span<char> buf) override {
        auto& s = sockets_.at(fd);
        if (s.is_udp) {
            throw rrmode::netlib::net::net_error("fake recv: используйте recvfrom для UDP");
        }
        if (s.closed) {
            throw rrmode::netlib::net::net_error("fake recv: closed");
        }
        if (s.rx.empty()) {
            if (s.rx_eof) {
                return std::size_t{0};
            }
            if (s.non_blocking) {
                return std::nullopt;
            }
            return std::size_t{0};
        }
        std::size_t n = std::min(buf.size(), s.rx.size());
        for (std::size_t i = 0; i < n; ++i) {
            buf[i] = s.rx.front();
            s.rx.pop_front();
        }
        return n;
    }

    std::optional<std::size_t> try_send(int fd, std::span<char const> buf) override {
        auto& s = sockets_.at(fd);
        if (s.is_udp) {
            throw rrmode::netlib::net::net_error("fake send: используйте sendto для UDP");
        }
        if (s.closed) {
            throw rrmode::netlib::net::net_error("fake send: closed");
        }
        if (s.non_blocking && !s.tx.empty()) {
            return std::nullopt;
        }
        for (char c : buf) {
            s.tx.push_back(c);
        }
        deliver_to_peer(fd);
        return buf.size();
    }

    void bind_unix(int fd, rrmode::netlib::net::unix_endpoint const& ep) override {
        auto& s = sockets_[fd];
        s.is_unix = true;
        s.is_listener = true;
        s.bound_unix_path = ep.path;
        unix_listeners_[ep.path] = fd;
    }

    std::string get_bound_unix_path(int fd) override { return sockets_.at(fd).bound_unix_path; }

    void bind_endpoint(int fd, rrmode::netlib::net::endpoint const& ep) override {
        auto& s = sockets_[fd];
        if (s.is_udp) {
            s.bound_port = ep.port == 0 ? ++auto_port_ : ep.port;
            return;
        }
        s.is_listener = true;
        s.bound_port = ep.port == 0 ? ++auto_port_ : ep.port;
    }

    void listen_socket(int fd, int /*backlog*/) override {
        sockets_[fd].is_listener = true;
    }

    std::uint16_t get_local_port(int fd) override { return sockets_.at(fd).bound_port; }

    int accept_nonblocking(int fd) override {
        auto& s = sockets_.at(fd);
        if (s.accept_queue.empty()) {
            return -1;
        }
        int const client = s.accept_queue.front();
        s.accept_queue.erase(s.accept_queue.begin());
        return client;
    }

    // --- helpers для тестов ---

    void link_peers(int a, int b) {
        sockets_[a].peer = b;
        sockets_[b].peer = a;
        sockets_[a].connect_done = true;
        sockets_[b].connect_done = true;
    }

    void link_udp(int a, int b) {
        sockets_[a].udp_peer = b;
        sockets_[b].udp_peer = a;
        sockets_[a].bound_port = sockets_[a].bound_port == 0 ? ++auto_port_ : sockets_[a].bound_port;
        sockets_[b].bound_port = sockets_[b].bound_port == 0 ? ++auto_port_ : sockets_[b].bound_port;
    }

    void push_datagram(int fd, rrmode::netlib::net::endpoint const& remote, std::string_view data) {
        auto& s = sockets_.at(fd);
        datagram dg{.remote = remote, .payload = std::vector<char>{data.begin(), data.end()}};
        s.datagram_rx.push_back(std::move(dg));
        if (reactor_) {
            reactor_->arm(fd, rrmode::netlib::net::detail::poll_event::readable);
        }
    }

    void push_rx(int fd, std::string_view data) {
        for (char c : data) {
            sockets_[fd].rx.push_back(c);
        }
    }

    void complete_connect(int fd) {
        auto& s = sockets_[fd];
        s.connect_done = true;
        s.so_error = 0;
        if (reactor_) {
            reactor_->arm(fd, rrmode::netlib::net::detail::poll_event::writable);
        }
    }

    void set_connect_pending(int fd) { sockets_[fd].connect_done = false; }

    void set_socket_error(int fd, int err) { sockets_[fd].so_error = err; }

    void signal_connect_complete(int fd) {
        sockets_[fd].connect_done = true;
        if (reactor_) {
            reactor_->arm(fd, rrmode::netlib::net::detail::poll_event::writable);
        }
    }

    void enqueue_accept(int listener_fd, int client_fd) {
        sockets_[listener_fd].accept_queue.push_back(client_fd);
        if (reactor_) {
            reactor_->arm(listener_fd, rrmode::netlib::net::detail::poll_event::readable);
        }
    }

    void trigger_readable(int fd) {
        if (reactor_) {
            reactor_->arm(fd, rrmode::netlib::net::detail::poll_event::readable);
        }
    }

    void set_rx_eof(int fd) { sockets_[fd].rx_eof = true; }

    [[nodiscard]] socket_state const& state(int fd) const { return sockets_.at(fd); }

    struct applied_options {
        bool reuseaddr{false};
        std::optional<bool> tcp_nodelay;
        std::optional<bool> keepalive;
        std::optional<int> send_buffer_size;
        std::optional<int> recv_buffer_size;
        std::optional<int> linger_seconds;
    };

    [[nodiscard]] applied_options const& options(int fd) const { return applied_.at(fd); }

private:
    void deliver_datagram(int fd, rrmode::netlib::net::endpoint const& remote,
                          std::span<char const> data) {
        auto& dst = sockets_.at(fd);
        dst.datagram_rx.push_back(
            datagram{.remote = remote, .payload = std::vector<char>{data.begin(), data.end()}});
        if (reactor_) {
            reactor_->arm(fd, rrmode::netlib::net::detail::poll_event::readable);
        }
    }

    void route_datagram(rrmode::netlib::net::endpoint const& src,
                        rrmode::netlib::net::endpoint const& dest, std::span<char const> data) {
        for (auto& [fd, s] : sockets_) {
            if (!s.is_udp || s.closed || s.bound_port != dest.port) {
                continue;
            }
            deliver_datagram(fd, src, data);
        }
    }

    void deliver_to_peer(int fd) {
        int const peer = sockets_[fd].peer;
        if (peer < 0) {
            return;
        }
        auto& dst = sockets_[peer];
        while (!sockets_[fd].tx.empty()) {
            dst.rx.push_back(sockets_[fd].tx.front());
            sockets_[fd].tx.pop_front();
        }
        if (reactor_ && !dst.rx.empty()) {
            reactor_->arm(peer, rrmode::netlib::net::detail::poll_event::readable);
        }
    }

    fake_reactor* reactor_{nullptr};
    int next_fd_{100};
    std::uint16_t auto_port_{20000};
    std::unordered_map<std::string, int> unix_listeners_;
    std::unordered_map<int, socket_state> sockets_;
    std::unordered_map<int, applied_options> applied_;
};

}  // namespace netlib::fakes
