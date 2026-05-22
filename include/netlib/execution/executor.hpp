#pragma once

#include <functional>

namespace rrmode::netlib::execution {

/// Абстракция «куда отправить работу».
class executor {
public:
    virtual ~executor() = default;

    /// Поставить задачу в очередь; не блокирует вызывающий поток.
    virtual void post(std::function<void()> fn) = 0;

    /// Запросить остановку (идемпотентно). Реализация определяет семантику drain.
    virtual void request_stop() {}

protected:
    executor() = default;
};

}  // namespace rrmode::netlib::execution
