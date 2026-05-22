# UNIX domain stream (AF_UNIX)

Локальные потоковые сокеты по **файловому пути** (не abstract `@`). Тот же async-паттерн, что у TCP: `event_loop`, `async_*`, общий `socket_backend`.

## Заголовки

| Класс | Файл |
|-------|------|
| `unix_endpoint` | `net/unix_endpoint.hpp` |
| `unix_stream_socket` | `net/unix_stream_socket.hpp` |
| `unix_stream_acceptor` | `net/unix_stream_acceptor.hpp` |

## Быстрый пример (callback)

```cpp
using namespace rrmode::netlib::net;

event_loop loop;
unix_stream_acceptor acc;
acc.open({.path = "/tmp/myapp.sock"});

acc.async_accept(loop, [&](unix_stream_socket peer) { /* ... */ },
                 [](net_error const& e) { /* ... */ });

unix_stream_socket client;
client.async_connect(loop, {.path = "/tmp/myapp.sock"}, [] { /* connected */ },
                     [](net_error const& e) { /* ... */ });
```

Перед `open` backend делает `unlink` пути на POSIX, чтобы не оставался «висячий» socket-файл после прошлого запуска.

## Платформы

| ОС | Поддержка |
|----|-----------|
| Linux / macOS / WSL | POSIX `AF_UNIX` |
| Windows | `win_socket_backend` бросает `net_error` (нет stream UNIX в этой сборке) |

## Coroutines

При `NETLIB_ENABLE_COROUTINES`:

| Awaitable / helper | Файл |
|--------------------|------|
| `connect_unix_async`, `read_some_unix_async`, `write_all_unix_async`, `accept_unix_async` | `unix_awaitables.hpp` |
| `read_unix_string_async`, `unix_echo_peer`, `unix_serve_echo_once` | `coro_unix.hpp` |
| `unix_connect`, `unix_echo_server_loop`, `read_unix_string_with_timeout` | `unix_coro.hpp` |
| `connect_unix_with_timeout`, `read_*_unix_with_timeout`, `accept_unix_with_timeout` | `timeout.hpp` |

Перегрузка `unix_connect(..., sched, limit)` — обёртка над `connect_unix_with_timeout`.

Umbrella: `#include <netlib/net/coro.hpp>`.

## Ограничения

- Только **SOCK_STREAM** (не datagram UNIX).
- Путь ограничен `sizeof(sockaddr_un::sun_path)` (обычно 108 байт на Linux).
- Нет TLS, DNS, IPv6 — см. [ROADMAP.md](ROADMAP.md).

## Тесты

- Unit (фейк): `tests/unit/net/unix_stream_tests.cpp`
- Integration (POSIX): `tests/integration/unix_stream_loopback_tests.cpp`
