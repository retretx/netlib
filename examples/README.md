# Примеры netlib

Подробное описание сценариев и диаграммами: **[docs/EXAMPLES.md](../docs/EXAMPLES.md)**.

Сборка:

```bash
cmake -B build -DNETLIB_BUILD_EXAMPLES=ON -DNETLIB_ENABLE_COROUTINES=ON
cmake --build build
```

## tcp_echo

| Бинарник | Описание |
|----------|----------|
| `tcp_echo_server [port]` | Echo-сервер (по умолчанию порт 9001) |
| `tcp_echo_client [port] [message] [host]` | Клиент (callbacks) |
| `tcp_echo_client_coro [port] [message]` | Клиент (coroutines) |
| `tcp_echo_server_coro [port]` | Coro-сервер (Enter — остановка) |
| `tcp_echo_client_coro_timeout [port] [message]` | Клиент с таймаутами |

## udp_echo

| Бинарник | Описание |
|----------|----------|
| `udp_echo_server [port]` | UDP echo (callbacks, порт 9002) |
| `udp_echo_client_coro [port] [message]` | UDP клиент (coroutines) |
| `udp_echo_server_coro [port]` | UDP coro-сервер (Enter — остановка) |

```bash
./build/examples/udp_echo/udp_echo_server 9002
./build/examples/udp_echo/udp_echo_server_coro 9002
./build/examples/udp_echo/udp_echo_client_coro 9002 hello-udp
```

## unix_echo

| Бинарник | Описание |
|----------|----------|
| `unix_echo_server_coro [path]` | UNIX stream echo (Enter — остановка) |
| `unix_echo_client_coro <path> [message]` | UNIX клиент (coroutines) |
| `unix_echo_client_coro_timeout <path> [message]` | UNIX клиент с таймаутами 5s |

```bash
./build/examples/unix_echo/unix_echo_server_coro /tmp/netlib.sock
./build/examples/unix_echo/unix_echo_client_coro /tmp/netlib.sock hello-unix
./build/examples/unix_echo/unix_echo_client_coro_timeout /tmp/netlib.sock hello-unix
```

Только POSIX (Linux/macOS). На Windows см. unit-тесты с фейками.
