# Установка и find_package

## Сборка из исходников

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DNETLIB_BUILD_EXAMPLES=ON
cmake --build build -j
sudo cmake --install build --prefix /usr/local
```

Опции CMake: см. [CMAKE_OPTIONS.md](CMAKE_OPTIONS.md) (`NETLIB_BUILD_TESTS`, `NETLIB_BUILD_EXAMPLES`, `NETLIB_ENABLE_COROUTINES`, …).

## Потребление через CMake

```cmake
find_package(netlib 1.0 CONFIG REQUIRED)

add_executable(app main.cpp)
target_link_libraries(app PRIVATE netlib::netlib)
target_compile_features(app PRIVATE cxx_std_23)
```

## FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    netlib
    GIT_REPOSITORY https://github.com/your-org/netlib.git
    GIT_TAG v1.0.0
)
FetchContent_MakeAvailable(netlib)

target_link_libraries(app PRIVATE netlib::netlib)
```

## Платформы

Подробная матрица: [PLATFORMS.md](PLATFORMS.md).

## Lifetime async-операций

`tcp_socket` удерживает fd через внутренний `socket_handle` до завершения операций.

## Уровни API

| CMake target | Include / module |
|--------------|------------------|
| `netlib::netlib` | `#include <netlib/netlib.hpp>` / `import netlib.net;` |
| `netlib::simple` | `#include <netlib/simple.hpp>` / `import netlib.net.simple;` (требует `NETLIB_BUILD_MODULES`) |
| `netlib::medium` | `#include <netlib/medium.hpp>` / `import netlib.net.medium;` |

```bash
cmake -B build -DNETLIB_BUILD_MODULES=ON   # если toolchain прошёл probe
```
Для цепочек read→write после уничтожения объекта используйте `io_handle()`:

```cpp
auto io = sock.io_handle();
sock.async_read_some(loop, buf, [io, &loop](std::size_t n) {
    tcp_socket{io}.async_write_all(loop, ...);
}, ...);
```

Не используйте `sock = std::move(server_sock)` в списке захвата колбэка **до** вызова `async_read_some` — move выполняется при создании лямбды.
