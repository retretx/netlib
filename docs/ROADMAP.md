# Roadmap netlib

Документ фиксирует возможности, **намеренно отложенные** после bootstrap соглашений. Порядок — ориентировочный; приоритеты уточняются по мере появления кода и тестов.

См. также [ARCHITECTURE.md](ARCHITECTURE.md).

## v0 — Bootstrap (текущий этап)

- [x] Архитектурный документ и Cursor rules
- [x] Корневой `CMakeLists.txt`, target `netlib::netlib`
- [x] Минимальные заголовки-заглушки и Catch2 smoke-тест

## v1 — Execution foundation

- [x] `cmake/netlib_features.cmake` — probe `NETLIB_HAS_STD_EXECUTION`
- [x] `execution::detail::backend` — std и fallback
- [x] Базовый `thread_pool` / `executor` / `scheduler`
- [x] CI на Linux (GCC, Clang); best-effort Windows (MSVC)

## v1 — Net foundation

- [x] `event_loop` + reactor `detail` (Linux `epoll` первым)
- [x] `tcp_socket`, `tcp_acceptor`, `endpoint` (async connect/write/read)
- [x] Интеграционные тесты с loopback (TCP echo, sync ping, accept/connect)
- [x] TDD: `tests/fakes/`, `tests/unit/`, `socket_backend` / `reactor_backend`
- [x] `async_read_some`: unit-сценарии + integration echo на async (без blocking recv)
- [x] Unit-тесты `tcp_acceptor` на фейках

## C++20 modules (начато)

**Цель:** опциональный модульный интерфейс без отказа от headers.

| Элемент | Статус |
|---------|--------|
| CMake option | `NETLIB_BUILD_MODULES` (default OFF), probe `NETLIB_HAS_MODULES` |
| Модули | `netlib.net.simple`, `netlib.net.medium`, `netlib.net` в `modules/` |
| Targets | `netlib::simple`, `netlib::medium`, `netlib::net_module` |
| Потребление | `import netlib.net.simple;` зеркало `#include <netlib/simple.hpp>` |

**За:** быстрее пересборка у крупных потребителей.  
**Против:** сложность CI, разброс поддержки modules в компиляторах.

## v2 — Coroutines (начато)

- [x] `NETLIB_ENABLE_COROUTINES` + probe toolchain
- [x] `execution::task<T>`, `co_await scheduler`, `sync_wait`
- [x] `net::connect_async`, `write_all_async`, `read_some_async`
- [x] Пример `tcp_echo_client_coro`
- [x] `when_all` для двух task
- [x] `accept_async`, integration `tcp_echo_coro`
- [x] `generator<T>` с `co_await gen.next(sched)`
- [x] `when_all` для `std::vector<task<T>>`
- [x] `spawn` — detached task
- [x] `when_all` для void, void+T, vector&lt;task&lt;void&gt;&gt;
- [x] `then` — последовательная композиция
- [x] `net/coro_tcp.hpp` — read_string, tcp_echo_peer, tcp_serve_echo_once
- [x] variadic `when_all` → `std::tuple` (3+ task, не-void)
- [x] `read_exact_async`, `read_exact_vec_async`
- [x] `delay_async` (scheduler + event_loop), `event_loop::run_after`
- [x] `when_any`, `with_timeout`, `connect_with_timeout` / `read_*_with_timeout`
- [x] timerfd (Linux) для `event_loop::run_after`
- [x] kqueue EVFILT_TIMER в том же kqueue, что сокеты (macOS)
- [x] `tcp_socket::cancel_io` — снятие pending async I/O
- [x] `cancellation_token` / `cancellation_source` для coro awaitables
- [x] `*_with_timeout` + token; `accept_with_timeout`; `tcp_acceptor::cancel_accept`
- [x] `net/tcp_coro.hpp` — tcp_connect, tcp_echo_server_loop, run_until_cancelled
- [x] пример `tcp_echo_server_coro` (graceful shutdown через cancellation_source)
- [x] `when_any` / `with_timeout`: колбэки `on_loser_*` / `on_work_lost` для кооперативной отмены
- [x] integration: `server_coro_shutdown` (cancel выходит из `tcp_echo_server_loop`)
- [x] [API_LAYERS.md](API_LAYERS.md) — callback / awaitables / tcp_coro; пометки на `async_*`
- [x] `read_string_with_timeout`, тесты `accept_with_timeout`, пример `client_coro_timeout`

## Отложено: `std::expected` в hot path

Сейчас: исключения + доменные типы ошибок.

v2: `expected<T, error_code>` для предсказуемых сетевых сбоев без unwind на hot path.

## v1 — Установка

- [x] `cmake --install` + `netlibConfig.cmake` + `find_package(netlib)`
- [x] Документация: [INSTALL.md](INSTALL.md)
- [x] `socket_handle` — удержание fd в async I/O; `io_handle()` для цепочек операций
- [x] `make_default_reactor()` — Linux epoll, macOS kqueue, Windows WSAPoll
- [x] CI: ubuntu (GCC/Clang), macos-latest, windows-latest

## Отложено: Установка (расширения)

- Публикация релизов / vcpkg / Conan

## Отложено: Amalgamation / single-header

**Цель:** опциональный `netlib_single.hpp` для проектов без CMake.

- Генерация скриптом, не ручное копирование.
- Не заменяет основной способ поставки.

## v2 — UDP (транспорт)

- [x] `socket_backend`: `create_udp_socket`, `try_sendto`, `try_recvfrom`
- [x] `udp_socket`: bind, `async_send_to`, `async_recv_from`, `cancel_io`
- [x] unit (fake) + integration loopback (POSIX)
- [x] coroutine awaitables: `send_to_async`, `recv_from_async`, `udp_coro.hpp`
- [x] `send_to_with_timeout` / `recv_from_with_timeout`
- [x] примеры `examples/udp_echo/` (server, server_coro, client_coro)
- [x] integration: `udp_server_coro_shutdown` (cancel выходит из `udp_echo_loop`)

## UNIX domain stream

- [x] `unix_endpoint`, `unix_stream_socket`, `unix_stream_acceptor`
- [x] `socket_backend`: `create_unix_stream_socket`, `bind_unix`, `start_connect_unix`
- [x] POSIX + fake; Windows — явный `net_error`
- [x] unit + integration loopback; [NET_UNIX.md](NET_UNIX.md)
- [x] coroutine awaitables + `examples/unix_echo/`
- [x] `connect_unix_with_timeout`, integration shutdown

## Отложено: Расширения net

- UNIX domain **datagram** (SOCK_DGRAM)
- TLS (только если появится **std** или явное решение о минимальной dep — иначе out of scope)
- DNS async resolver

## v1 — Документация и DX

- [x] `README.md`, `docs/GETTING_STARTED.md`
- [x] Примеры `examples/tcp_echo/` (server + client)
- [x] Umbrella `#include <netlib/netlib.hpp>`
- [x] Документация в `docs/*.md` + PlantUML (`docs/diagrams/`) — без Doxygen/mkdocs
- [x] Benchmarks в `benchmarks/` (`NETLIB_BUILD_BENCHMARKS`, без внешних deps)

## Критерии «готово к v1 release»

Статус и smoke — [V1_RELEASE.md](V1_RELEASE.md).

1. Fallback execution проходит CI без `NETLIB_HAS_STD_EXECUTION`.
2. TCP echo-тест на Linux через reactor + thread pool.
3. ~~Сборка на Windows~~ — WSAPoll + winsock smoke; полноценный IOCP — v2.
4. Нет внешних зависимостей кроме Catch2 в dev.
