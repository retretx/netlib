# PlantUML-диаграммы

| Файл | Описание | Упоминание в docs |
|------|----------|-------------------|
| [layers.puml](layers.puml) | simple / medium / full / execution | [ARCHITECTURE.md](../ARCHITECTURE.md) |
| [tcp_async_connect.puml](tcp_async_connect.puml) | `async_connect` | [NET_TCP.md](../NET_TCP.md) |
| [coro_awaitable.puml](coro_awaitable.puml) | `connect_async` | [COROUTINES.md](../COROUTINES.md) |
| [when_any_timeout.puml](when_any_timeout.puml) | `with_timeout` | [CANCELLATION_AND_TIMEOUT.md](../CANCELLATION_AND_TIMEOUT.md) |

Дополнительные диаграммы встроены в markdown (не дублируются здесь):

- [API_LAYERS.md](../API_LAYERS.md) — стек уровней API
- [EXECUTION.md](../EXECUTION.md) — компоненты execution
- [ERRORS.md](../ERRORS.md) — иерархия исключений
- [EXAMPLES.md](../EXAMPLES.md) — `io_runner`, выбор примера
- [CMAKE_OPTIONS.md](../CMAKE_OPTIONS.md) — поток configure

## Экспорт PNG (локально)

```bash
# при установленном plantuml.jar или пакете plantuml
plantuml -tpng docs/diagrams/*.puml
# → PNG рядом с .puml
```

Для CI/docs-сайта PNG не храним в репозитории — только исходники `.puml`.

## Рендер онлайн

Скопировать содержимое файла в [PlantUML Web Server](https://www.plantuml.com/plantuml/uml/).
