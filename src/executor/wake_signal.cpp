#include "executor/wake_signal.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

WakeSignal::WakeSignal() : signal_(nullptr), context_(nullptr)
{
    AV_SCOPE("WakeSignal::WakeSignal");
}

void WakeSignal::bind(void (*signal)(void *context), void *context)
{
    AV_SCOPE("WakeSignal::bind");
    __atomic_store_n(&context_, context, __ATOMIC_RELEASE);
    __atomic_store_n(&signal_, signal, __ATOMIC_RELEASE);
}

void WakeSignal::clear()
{
    AV_SCOPE("WakeSignal::clear");
    __atomic_store_n(&signal_, nullptr, __ATOMIC_RELEASE);
    __atomic_store_n(&context_, nullptr, __ATOMIC_RELEASE);
}

bool WakeSignal::bound() const
{
    AV_SCOPE("WakeSignal::bound");
    return __atomic_load_n(&signal_, __ATOMIC_ACQUIRE) != nullptr;
}

bool WakeSignal::raise() const
{
    AV_SCOPE("WakeSignal::raise");
    void (*const signal)(void *) = __atomic_load_n(&signal_, __ATOMIC_ACQUIRE);
    void *const context = __atomic_load_n(&context_, __ATOMIC_ACQUIRE);
    if (signal == nullptr) return false;
    signal(context);
    return true;
}

}
