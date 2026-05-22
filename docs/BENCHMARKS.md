# Benchmarks netlib

Опциональные исполняемые файлы для грубой оценки пропускной способности. **Не** являются регрессионными тестами CI: в CI проверяется только сборка.

## Сборка

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DNETLIB_BUILD_BENCHMARKS=ON \
  -DNETLIB_ENABLE_COROUTINES=ON
cmake --build build -j
```

## Запуск

Первый аргумент — число итераций (по умолчанию зависит от бенчмарка).

```bash
./build/benchmarks/schedule_bench 200000
./build/benchmarks/tcp_echo_bench 500
./build/benchmarks/udp_ping_bench 2000
```

Вывод: `имя: N ops, X ms, Y ops/s`.

| Бинарник | Что измеряет |
|----------|----------------|
| `schedule_bench` | `scheduler::schedule` + `thread_pool` (без сети) |
| `tcp_echo_bench` | loopback TCP echo (coro connect/write/read) |
| `udp_ping_bench` | loopback UDP echo (coro send/recv) |

## Интерпретация

- Сравнивайте только **Release** на одной машине.
- TCP/UDP включают reactor, планировщик и копирование буферов — не чистый «сетевой» бенчмарк.
- Для строгих сравнений с Asio/Boost используйте внешние инструменты; здесь — smoke для netlib.

## Зависимости

Только стандартная библиотека и `netlib` — без Google Benchmark.
