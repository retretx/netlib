# FAQ

## Почему нет Doxygen?

Документация живёт в `docs/*.md` рядом с кодом, с PlantUML для диаграмм. Так проще ревьюить в Git и не дублировать комментарии в заголовках. См. [README.md](README.md).

## Почему исключения, а не `std::expected`?

v1: идиоматичный C++ API и простые тесты с `REQUIRE_THROWS_AS`. `expected` на hot path — [ROADMAP.md](ROADMAP.md) v2.

## Есть ли async DNS?

**Нет.** При connect/bind/sendto вызывается синхронный `getaddrinfo` внутри `socket_backend`. Отдельного resolver loop нет.

## Почему `timeout_error` vs «операция отменена»?

При `with_timeout` отмена token в гонке `when_any` может дать `net_error` раньше таймаута. Для `*_with_timeout` отмена делается в `catch` после `timeout_error`. См. [CANCELLATION_AND_TIMEOUT.md](CANCELLATION_AND_TIMEOUT.md).

## Как graceful shutdown сервера?

`cancellation_source` + `tcp_echo_server_loop` / `udp_echo_loop`, примеры `*_server_coro.cpp`. Enter → `shutdown.cancel()`.

## GitHub не показывает PlantUML

Нужно открыть `.puml` в IDE или [plantuml.com](https://www.plantuml.com/plantuml/uml/), либо экспорт PNG — [diagrams/README.md](diagrams/README.md).

## Windows: почему нет examples/integration TCP?

v1: WSAPoll smoke + unit на фейках. Полноценный POSIX integration — Linux/macOS. См. [PLATFORMS.md](PLATFORMS.md).

## Как добавить тест на новую фичу?

Сначала unit на `fake_socket_backend` / `fake_reactor`, затем integration. [TESTING.md](TESTING.md), [DEVELOPMENT.md](DEVELOPMENT.md).
