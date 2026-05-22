#pragma once

#include <netlib/net/error.hpp>
#include <netlib/net/simple/detail/sync_wait.hpp>
#include <netlib/net/simple/tcp_connection.hpp>
#include <netlib/net/simple/writable_chunk.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

namespace rrmode::netlib::net::simple {

/// RAII-буфер записи: operator<< накапливает, flush отправляет.
class write_stream {
public:
    struct options {
        std::size_t auto_flush_threshold{0};
    };

    explicit write_stream(tcp_connection& conn) : conn_{&conn} {}

    write_stream(tcp_connection& conn, options opts) : conn_{&conn}, opts_{opts} {}

    write_stream(write_stream&& other) noexcept
        : conn_{other.conn_},
          opts_{other.opts_},
          buffer_{std::move(other.buffer_)},
          moved_from_{other.moved_from_},
          flush_error_{other.flush_error_} {
        other.moved_from_ = true;
    }

    write_stream& operator=(write_stream&& other) noexcept {
        if (this != &other) {
            try_flush_on_destroy();
            conn_ = other.conn_;
            opts_ = other.opts_;
            buffer_ = std::move(other.buffer_);
            moved_from_ = other.moved_from_;
            flush_error_ = other.flush_error_;
            other.moved_from_ = true;
        }
        return *this;
    }

    write_stream(write_stream const&) = delete;
    write_stream& operator=(write_stream const&) = delete;

    ~write_stream() { try_flush_on_destroy(); }

    template <writable_chunk T>
    write_stream& operator<<(T const& chunk) {
        detail::append_chunk(buffer_, chunk);
        if (opts_.auto_flush_threshold > 0 && buffer_.size() >= opts_.auto_flush_threshold) {
            flush();
        }
        return *this;
    }

    /// Синхронная отправка буфера.
    void flush() {
        if (buffer_.empty() || !conn_) {
            return;
        }
        auto data = std::move(buffer_);
        buffer_.clear();
        try {
            conn_->write_all(data);
        } catch (...) {
            flush_error_ = std::current_exception();
            throw;
        }
    }

    void async_flush(std::move_only_function<void()> on_done,
                     std::move_only_function<void(net_error const&)> on_error) {
        if (buffer_.empty()) {
            if (on_done) {
                on_done();
            }
            return;
        }
        conn_->async_write_all(std::move(buffer_), std::move(on_done), std::move(on_error));
        buffer_.clear();
    }

    [[nodiscard]] bool has_error() const noexcept { return flush_error_ != nullptr; }

    void rethrow_if_error() const {
        if (flush_error_) {
            std::rethrow_exception(flush_error_);
        }
    }

private:
    void try_flush_on_destroy() noexcept {
        if (moved_from_ || buffer_.empty() || !conn_) {
            return;
        }
        try {
            flush();
        } catch (...) {
            flush_error_ = std::current_exception();
        }
    }

    tcp_connection* conn_{nullptr};
    options opts_{};
    std::vector<char> buffer_;
    bool moved_from_{false};
    std::exception_ptr flush_error_{};
};

inline write_stream tcp_connection::write() { return write_stream{*this}; }

inline write_stream tcp_connection::write(std::size_t auto_flush_threshold) {
    write_stream::options opts;
    opts.auto_flush_threshold = auto_flush_threshold;
    return write_stream{*this, opts};
}

}  // namespace rrmode::netlib::net::simple
