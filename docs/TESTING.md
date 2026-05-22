# Тестирование netlib

См. также архитектуру фейков в контексте reactor: [NET_REACTOR.md](NET_REACTOR.md).

## Подход: TDD

1. **Красный** — пишем unit-тесты на русском (`TEST_CASE` / `SECTION`) с фейками ОС/сети; тест падает.
2. **Зелёный** — минимальная реализация в `include/netlib/`.
3. **Рефакторинг** — без изменения поведения, пока тесты зелёные.

Новая фича **без** unit-тестов на фейках не мержится. Интеграция с реальным epoll/сокетами — отдельный слой, после unit.

## Пирамида

```text
        ┌─────────────────────┐
        │  integration (мало) │  реальный epoll, loopback TCP
        ├─────────────────────┤
        │  unit (много)       │  fake_socket + fake_reactor
        └─────────────────────┘
```

| Слой | Где | Зависимости |
|------|-----|-------------|
| Unit | `tests/unit/**` | `tests/fakes/**`, без `sys/epoll.h` в сценариях |
| Integration | `tests/integration/**` | `netlib::netlib`, Linux/POSIX |

## Фейки (test doubles)

### `fake_socket_backend`

Имитирует сокеты: fd, буферы RX/TX, пары «клиент↔сервер», `connect` / `EAGAIN`, EOF.

Тесты управляют данными явно:

```cpp
fake.push_rx(client_fd, "hello");
fake.trigger_readable(client_fd);
```

### `fake_reactor`

Имитирует epoll: регистрация `readable`/`writable`, `trigger(fd, event)`, `poll_once`.

Связан с `fake_socket_backend`: при `try_send` данные попадают к peer и планируется `readable`.

### Правила фейков

- Живут только в [`tests/fakes/`](../tests/fakes/); **не** в `include/netlib/`.
- Прод-код зависит от абстракций [`socket_backend`](../include/netlib/net/detail/socket_backend.hpp), [`reactor_backend`](../include/netlib/net/detail/reactor_backend.hpp).
- Юниты **не** включают `posix_socket.hpp` / `epoll` напрямую.

## Инъекция в прод-код

```cpp
// event_loop: reactor подставляется (по умолчанию epoll)
event_loop loop{sched, std::make_unique<detail::epoll_reactor>()};

// tcp_socket / tcp_acceptor: socket_backend&
tcp_socket sock{fake_sockets};
```

Для тестов — [`test_context`](../tests/fakes/test_context.hpp): `thread_pool` + `scheduler` + `event_loop` на фейках.

## Структура каталогов

```text
tests/
├── fakes/           # fake_reactor, fake_socket_backend, test_context
├── unit/            # один файл ≈ одна фича, много сценариев
│   └── net/
└── integration/     # бывшие tcp_echo, eventfd, …
```

## Именование сценариев

Формат: `"<тип> <действие> <условие>"`.

Примеры:

- `"async_read_some возвращает данные если они уже в буфере"`
- `"async_read_some вызывает on_eof при нулевом чтении"`
- `"async_connect завершается при trigger writable и SO_ERROR=0"`

Чем больше граничных сценариев на фичу — тем лучше.

## CMake

- `netlib_unit_tests` — только unit + fakes.
- `netlib_integration_tests` — POSIX/Linux интеграция.
- `NETLIB_BUILD_TESTS` включает оба.

## Чеклист PR с фичей

- [ ] Unit-тесты написаны **до** или вместе с кодом, падают без реализации
- [ ] Сценарии: успех, ошибка, пустой/граничный ввод, отмена/закрытие
- [ ] Фейки, без реальной сети в unit
- [ ] Интеграционный тест (если затронут I/O), помечен `integration`
- [ ] `ctest` / CI зелёный

См. [ARCHITECTURE.md](ARCHITECTURE.md), [ROADMAP.md](ROADMAP.md).
