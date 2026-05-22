# Быстрый старт

## Уровни API

| Уровень | Include | Старт |
|---------|---------|-------|
| Simple | `#include <netlib/simple.hpp>` | `simple::io_runtime` + `simple::tcp_connection` |
| Medium | `#include <netlib/medium.hpp>` | `medium::io_context` + `socket_options` |
| Full | `#include <netlib/netlib.hpp>` | `thread_pool` + `event_loop` + колбэки |

Ниже — **Full API**. Для простого клиента см. раздел Simple.  
Полное оглавление: **[README.md](README.md)**.  
Слои API: [API_LAYERS.md](API_LAYERS.md). Coroutines: [COROUTINES.md](COROUTINES.md). UDP: [NET_UDP.md](NET_UDP.md).

## Simple: минимальный клиент

```cpp
#include <netlib/simple.hpp>

int main() {
    rrmode::netlib::net::simple::io_runtime runtime;
    rrmode::netlib::net::simple::tcp_connection conn{runtime};
    conn.connect({.host = "127.0.0.1", .port = 9001});
    conn.write() << 'h' << 'i';
    // destructor write_stream отправит буфер
}
```

Не вызывайте sync-методы (`connect`, `write_all`, `read_some`) из колбэков I/O-потока — риск взаимной блокировки.

## Full: минимальный echo-клиент

```cpp
#include <netlib/netlib.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

int main() {
    rrmode::netlib::execution::thread_pool pool{2};
    rrmode::netlib::execution::scheduler sched{pool};
    rrmode::netlib::net::event_loop loop{sched};

    std::atomic<bool> stop_io{false};
    std::thread io{[&] {
        while (!stop_io.load()) {
            loop.run_once(std::chrono::milliseconds{10});
        }
    }};

    rrmode::netlib::net::tcp_socket client;
    std::atomic<bool> done{false};

    client.async_connect(
        loop, {.host = "127.0.0.1", .port = 9001},
        [&] {
            client.async_write_all(
                loop, std::vector<char>{'h', 'i'}, [&] { done.store(true); },
                [](auto const& e) { std::cerr << e.what(); });
        },
        [](auto const& e) { std::cerr << e.what(); });

    while (!done.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    stop_io = true;
    loop.stop();
    io.join();
    pool.shutdown();
}
```

Полные примеры: `examples/tcp_echo/`.

## Обязательные компоненты

1. **thread_pool** — worker-потоки для `scheduler`.
2. **event_loop** — reactor; `run_once()` в отдельном потоке (или в main).
3. **tcp_socket / tcp_acceptor** — async connect, read, write.

## Lifetime

`tcp_socket` должен жить до завершения async-операций, **или** используйте `io_handle()`:

```cpp
auto io = sock.io_handle();
sock.async_read_some(loop, buf, [io, &loop](std::size_t) {
    rrmode::netlib::net::tcp_socket{io}.async_write_all(loop, ...);
}, ...);
```

Не пишите `[s = std::move(sock)]` в колбэке **до** вызова `async_*` — move выполняется при создании лямбды.

## Сборка

```bash
cmake -B build -DNETLIB_BUILD_EXAMPLES=ON
cmake --build build
```

См. [INSTALL.md](INSTALL.md) для `find_package`.

## Coroutines (NETLIB_ENABLE_COROUTINES)

```bash
cmake -B build -DNETLIB_ENABLE_COROUTINES=ON   # по умолчанию ON, если компилятор умеет co_await
```

```cpp
#include <netlib/execution/coroutine.hpp>
#include <netlib/net/coro.hpp>

using rrmode::netlib::execution::task;
using rrmode::netlib::execution::sync_wait;

task<std::string> fetch(scheduler& sched, event_loop& loop, endpoint ep) {
    tcp_socket s;
    co_await sched;
    co_await connect_async(s, loop, ep);
    // ...
    co_return "ok";
}
```

Примеры: `examples/tcp_echo/tcp_echo_client_coro`, `tcp_echo_server_coro` (Enter — остановка).

```cpp
auto [a, b] = co_await when_all(sched, task_a, task_b);
auto [x, y, z] = co_await when_all(sched, task_x, task_y, task_z);  // 3+ → tuple
co_await read_exact_async(socket, loop, std::span<char>{buf, len});
co_await delay_async(sched, std::chrono::milliseconds{100});
co_await delay_async(loop, std::chrono::milliseconds{100});  // Linux: timerfd + epoll
loop.run_after(std::chrono::milliseconds{100}, [] { /* callback */ });
co_await connect_with_timeout(socket, loop, ep, sched, std::chrono::seconds{5}, &src);
co_await accept_with_timeout(acceptor, loop, sched, std::chrono::seconds{5}, &src);
co_await with_timeout(sched, slow_task(sched), std::chrono::milliseconds{500});
co_await when_any(sched, task_a, task_b, nullptr, [] { /* отмена проигравшего */ });
co_await with_timeout(sched, work, limit, nullptr, [] { /* on_work_lost */ });
socket.cancel_io(loop);  // снять pending connect/read/write

cancellation_source src;
co_await connect_async(socket, loop, ep, &src.token());
src.cancel();  // → cancel_io + net_error
auto client_msg = co_await when_all(sched, server_task, client_task);  // void + T → T
co_await when_all(sched, std::vector<task<void>>{...});
auto doubled = co_await then(sched, produce(sched, 1), [](int x) { return x * 2; });

co_await tcp_serve_echo_once(acceptor, loop);
co_await tcp_echo_server_loop(acceptor, loop, &shutdown.token());
co_await udp_send_string(udp, loop, "ping", {.host = "127.0.0.1", .port = 9002});
auto udp_reply = co_await udp_recv_string(udp, loop);
auto msg = co_await read_string_async(socket, loop);
co_await accept_async(acceptor, loop);
spawn(sched, background_task(sched));

generator<int> g = iota(sched, 10);
while (auto v = co_await g.next(sched)) { /* *v */ }
```
