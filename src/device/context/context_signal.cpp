#include "device/context/context_signal.hpp"

#include "core/trace/function_scope.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/host/host.hpp"
#include "device/threading/worker.hpp"

namespace av {

ContextSignal::ContextSignal(Host &host, Worker &worker, FenceReporter &fences)
    : host_(host), worker_(worker), fences_(fences), pending_(false), raised_(0u), delivered_(0u)
{
    AV_SCOPE("ContextSignal::ContextSignal");
}

void ContextSignal::raise()
{
    AV_SCOPE("ContextSignal::raise");
    __atomic_fetch_add(&raised_, uint64_t{1}, __ATOMIC_RELAXED);
    __atomic_store_n(&pending_, true, __ATOMIC_RELEASE);
    worker_.wake();
}

void ContextSignal::drain()
{
    AV_SCOPE("ContextSignal::drain");
    host_.require_worker("ContextSignal::drain");
    if (!__atomic_exchange_n(&pending_, false, __ATOMIC_ACQUIRE)) return;
    delivered_++;
    fences_.raise();
}

bool ContextSignal::pending() const
{
    AV_SCOPE("ContextSignal::pending");
    return __atomic_load_n(&pending_, __ATOMIC_ACQUIRE);
}

uint64_t ContextSignal::raised() const
{
    AV_SCOPE("ContextSignal::raised");
    return __atomic_load_n(&raised_, __ATOMIC_RELAXED);
}

uint64_t ContextSignal::delivered() const
{
    AV_SCOPE("ContextSignal::delivered");
    return delivered_;
}

bool ContextSignal::invariants_hold() const
{
    AV_SCOPE("ContextSignal::invariants_hold");
    const uint64_t raised = __atomic_load_n(&raised_, __ATOMIC_RELAXED);
    return delivered_ <= raised && (!__atomic_load_n(&pending_, __ATOMIC_RELAXED) || raised != 0u);
}

}
