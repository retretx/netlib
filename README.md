# netlib

**v1.0.0** — header-only C++23 библиотека: слой **execution** (планирование задач) и **net** (TCP, UDP, UNIX stream, reactor).

- Namespace: `rrmode::netlib::{execution,net}`
- Зависимости: только стандартная библиотека + платформенный код в `::detail`
- Платформы: Linux (epoll), macOS (kqueue), Windows (WSAPoll + unit-тесты)

## Быстрый старт

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DNETLIB_BUILD_TESTS=ON -DNETLIB_BUILD_EXAMPLES=ON
cmake --build build -j
ctest --test-dir build
```

Терминал 1:

```bash
./build/examples/tcp_echo/tcp_echo_server 9001
```

Терминал 2:

```bash
./build/examples/tcp_echo/tcp_echo_client 9001 hello
# ответ: hello
```

## Документация

Полный набор — **[docs/README.md](docs/README.md)** (Markdown + PlantUML, без Doxygen).

| Раздел | Файлы |
|--------|--------|
| Старт | [GETTING_STARTED](docs/GETTING_STARTED.md), [INSTALL](docs/INSTALL.md), [API_LAYERS](docs/API_LAYERS.md) |
| Архитектура | [ARCHITECTURE](docs/ARCHITECTURE.md), [EXECUTION](docs/EXECUTION.md), [NET_REACTOR](docs/NET_REACTOR.md) |
| Протоколы | [NET_TCP](docs/NET_TCP.md), [NET_UDP](docs/NET_UDP.md), [NET_UNIX](docs/NET_UNIX.md) |
| Coroutines | [COROUTINES](docs/COROUTINES.md), [CANCELLATION_AND_TIMEOUT](docs/CANCELLATION_AND_TIMEOUT.md), [LIFECYCLE](docs/LIFECYCLE.md) |
| Справочник | [HEADERS_REFERENCE](docs/HEADERS_REFERENCE.md) |
| Сборка / платформы | [CMAKE_OPTIONS](docs/CMAKE_OPTIONS.md), [PLATFORMS](docs/PLATFORMS.md) |
| Ошибки | [ERRORS](docs/ERRORS.md) |
| Примеры | [EXAMPLES](docs/EXAMPLES.md) |
| Разработка / релиз | [DEVELOPMENT](docs/DEVELOPMENT.md), [V1_RELEASE](docs/V1_RELEASE.md), [CHANGELOG](../CHANGELOG.md) |
| Эксплуатация | [TESTING](docs/TESTING.md), [BENCHMARKS](docs/BENCHMARKS.md), [ROADMAP](docs/ROADMAP.md) |
| Диаграммы | [docs/diagrams/](docs/diagrams/) |

## Подключение в CMake

```cmake
add_subdirectory(path/to/netlib)  # или find_package(netlib)
target_link_libraries(app PRIVATE netlib::netlib)
```

Три уровня API (см. [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)):

```cpp
#include <netlib/simple.hpp>   // простой sync + write_stream
#include <netlib/medium.hpp>   // socket_options, io_context
#include <netlib/netlib.hpp>   // full: event_loop, колбэки
```

Coroutines (если включены при сборке):

```cpp
#include <netlib/execution/coroutine.hpp>
#include <netlib/net/coro.hpp>
// co_await connect_async(sock, loop, ep);
```

## Лицензия

MIT — см. [LICENSE](LICENSE).
