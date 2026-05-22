# Changelog

Формат основан на [Keep a Changelog](https://keepachangelog.com/ru/1.1.0/).

## [Unreleased]

## [1.0.0] — 2026-05-22

Первый стабильный релиз поверх bootstrap `0.1.0`. Semver: `find_package(netlib 1.0)`.

### Added

- UNIX domain stream: callback + coro + `*_with_timeout`, shutdown integration test, `examples/unix_echo/`
- `read_unix_string_with_timeout`, `unix_echo_client_coro_timeout`
- UDP: `udp_socket`, coroutine awaitables, примеры `udp_echo/`
- Coroutine: `when_any`, `on_loser_*`, `tcp_echo_server_loop`, `udp_echo_server_coro`
- Benchmarks: `NETLIB_BUILD_BENCHMARKS` (`schedule_bench`, `tcp_echo_bench`, `udp_ping_bench`)
- Документация: `docs/*.md`, PlantUML в `docs/diagrams/`

### Documentation

- [docs/README.md](docs/README.md) — оглавление без Doxygen
- [docs/V1_RELEASE.md](docs/V1_RELEASE.md) — чеклист v1

## [0.1.0] — bootstrap

- Header-only `netlib::netlib`, execution fallback, TCP epoll, Catch2 tests
