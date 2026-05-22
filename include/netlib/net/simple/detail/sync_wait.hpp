#pragma once

#include <netlib/net/error.hpp>

#include <chrono>
#include <exception>
#include <future>
#include <mutex>

namespace rrmode::netlib::net::simple::detail {

/// Состояние ожидания завершения одной async-операции (sync-обёртка).
struct sync_wait_state {
    std::promise<void> done;
    std::future<void> future{done.get_future()};
    std::exception_ptr error{};

    void complete_ok() {
        try {
            done.set_value();
        } catch (...) {
        }
    }

    void complete_err(net_error const& e) {
        error = std::make_exception_ptr(e);
        complete_ok();
    }
};

/// Крутит pump до готовности future или таймаута.
template <typename PumpFn>
void wait_completion(sync_wait_state& state, PumpFn&& pump,
                     std::chrono::milliseconds timeout = std::chrono::seconds{30}) {
    auto const deadline = std::chrono::steady_clock::now() + timeout;
    while (state.future.wait_for(std::chrono::milliseconds{0}) != std::future_status::ready) {
        if (std::chrono::steady_clock::now() >= deadline) {
            throw net_error("sync_wait: таймаут ожидания операции");
        }
        pump();
    }
    if (state.error) {
        std::rethrow_exception(state.error);
    }
}

}  // namespace rrmode::netlib::net::simple::detail
