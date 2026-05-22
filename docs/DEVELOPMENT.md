# Разработка netlib

Руководство для контрибьюторов и локальной отладки.

## Требования

- CMake ≥ 3.24
- C++23 (GCC 13+, Clang 16+, MSVC 19.3x+)
- Linux или WSL2 для полного integration + examples

## Клонирование и сборка

```bash
git clone <url> netlib && cd netlib
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
  -DNETLIB_BUILD_TESTS=ON \
  -DNETLIB_BUILD_EXAMPLES=ON \
  -DNETLIB_ENABLE_COROUTINES=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Опции: [CMAKE_OPTIONS.md](CMAKE_OPTIONS.md).

## TDD-цикл

1. Красный тест в `tests/unit/` на **фейках** (`tests/fakes/`).
2. Реализация в `include/netlib/`.
3. При необходимости — integration в `tests/integration/` (POSIX).
4. `ctest` зелёный.

Подробнее: [TESTING.md](TESTING.md).

## Структура изменений

| Меняете | Проверьте |
|---------|-----------|
| `socket_backend` | `fake_socket_backend` + posix + win |
| `reactor` | `fake_reactor` + integration |
| coroutine | не lambda-coroutine в `when_all` vector |
| публичный API | `docs/HEADERS_REFERENCE.md`, [API_LAYERS.md](API_LAYERS.md) |
| новый заголовок | umbrella / modules зеркало |

## Документация

При изменении поведения обновляйте соответствующий `docs/*.md` и при необходимости `docs/diagrams/*.puml`.  
Оглавление: [README.md](README.md). Doxygen не генерируем.

## Стиль

- Namespace `rrmode::netlib`, snake_case
- Исключения в публичном API
- Тесты Catch2, названия на русском
- `.cursor/rules/netlib-*.mdc` — соглашения для агентов

## CI

Перед push: локально `ctest`. GitHub Actions: [.github/workflows/ci.yml](../.github/workflows/ci.yml).

## Полезные команды

```bash
# один тест
./build/tests/netlib_unit_tests "async_connect завершается сразу"

# integration only
ctest --test-dir build -R netlib_integration -V

# пример вручную
./build/examples/tcp_echo/tcp_echo_server_coro 9001
```

## Связанные документы

- [V1_RELEASE.md](V1_RELEASE.md)
- [BENCHMARKS.md](BENCHMARKS.md)
