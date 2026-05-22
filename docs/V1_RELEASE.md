# Критерии v1 release

Чеклист из [ROADMAP.md](ROADMAP.md). Целевой тег: **`v1.0.0`**.

## Обязательные критерии

| # | Критерий | Статус | Доказательство |
|---|----------|--------|----------------|
| 1 | Fallback execution без `NETLIB_HAS_STD_EXECUTION` | выполнено | CI linux GCC/Clang, `thread_pool` |
| 2 | TCP echo через reactor + thread pool (Linux) | выполнено | `tests/integration/tcp_echo_tests.cpp` |
| 3 | Windows: сборка + unit на фейках | выполнено | CI `windows-latest` |
| 4 | Dev-зависимости только Catch2 | выполнено | `FetchContent` в tests |

## Расширения сверх минимального v1 (уже в дереве)

| Область | Статус |
|---------|--------|
| Coroutines (`task`, `when_all`, `when_any`, awaitables) | да |
| macOS kqueue + timer | да |
| `cmake --install` + `find_package` | да |
| UDP transport + coro | да |
| UNIX domain stream + coro + timeouts | да |
| Примеры tcp + udp + unix | да |
| Документация `docs/*.md` + PlantUML | да |
| Benchmarks опционально | да |

## Не входит в v1

| Элемент | Где зафиксировано |
|---------|------------------|
| Doxygen / mkdocs сайт | отменено → [docs/README.md](README.md) |
| Async DNS resolver | [ROADMAP.md](ROADMAP.md) |
| TLS | ROADMAP |
| IOCP полноценный | [PLATFORMS.md](PLATFORMS.md) |
| `std::expected` hot path | ROADMAP v2 |
| vcpkg / Conan publish | ROADMAP |

## Рекомендуемый smoke перед тегом

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DNETLIB_BUILD_TESTS=ON \
  -DNETLIB_BUILD_EXAMPLES=ON \
  -DNETLIB_ENABLE_COROUTINES=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Опционально Release benchmarks — [BENCHMARKS.md](BENCHMARKS.md).

## Версионирование

`project(netlib VERSION 1.0.0)` в корневом `CMakeLists.txt`, зеркало в `include/netlib/config.hpp`.  
Потребители: `find_package(netlib 1.0 CONFIG REQUIRED)` — см. [INSTALL.md](INSTALL.md).

## Публикация тега (после push)

```bash
git tag -a v1.0.0 -m "netlib 1.0.0"
git push origin v1.0.0
```

Опционально — smoke `find_package` после install:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DNETLIB_BUILD_TESTS=OFF
cmake --install build --prefix "$HOME/.local"
cmake -S tests/cmake/consumer_smoke -B /tmp/netlib-consumer \
  -DCMAKE_PREFIX_PATH="$HOME/.local"
cmake --build /tmp/netlib-consumer
```

Перед тегом убедитесь: `find_package(netlib 1.0)` в [INSTALL.md](INSTALL.md) и `tests/cmake/consumer_smoke/`; версия в [config.hpp](../include/netlib/config.hpp) совпадает с `CMakeLists.txt`.

## Связанные документы

- [ROADMAP.md](ROADMAP.md)
- [TESTING.md](TESTING.md)
- [INSTALL.md](INSTALL.md)
