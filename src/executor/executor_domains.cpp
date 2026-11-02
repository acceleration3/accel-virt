#include "executor/executor_domains.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av {

ExecutorDomains::ExecutorDomains() : ids_{}, gpus_{}, next_(0u)
{
    AV_SCOPE("ExecutorDomains::ExecutorDomains");
}

uint32_t ExecutorDomains::open(uint32_t gpu_index)
{
    AV_SCOPE("ExecutorDomains::open");
    const uint32_t entry = entry_of(0u);
    if (entry == max_domains) return 0u;
    next_++;
    __atomic_store_n(&gpus_[entry], gpu_index, __ATOMIC_RELAXED);
    __atomic_store_n(&ids_[entry], next_, __ATOMIC_RELEASE);
    return next_;
}

bool ExecutorDomains::close(uint32_t domain, uint32_t &gpu_index)
{
    AV_SCOPE("ExecutorDomains::close");
    gpu_index = 0u;
    if (domain == 0u) return false;
    const uint32_t entry = entry_of(domain);
    if (entry == max_domains) return false;
    gpu_index = __atomic_load_n(&gpus_[entry], __ATOMIC_RELAXED);
    __atomic_store_n(&ids_[entry], 0u, __ATOMIC_RELEASE);
    return true;
}

bool ExecutorDomains::gpu_of(uint32_t domain, uint32_t &gpu_index) const
{
    AV_SCOPE("ExecutorDomains::gpu_of");
    gpu_index = 0u;
    if (domain == 0u) return false;
    const uint32_t entry = entry_of(domain);
    if (entry == max_domains) return false;
    gpu_index = __atomic_load_n(&gpus_[entry], __ATOMIC_RELAXED);
    return true;
}

uint32_t ExecutorDomains::live() const
{
    AV_SCOPE("ExecutorDomains::live");
    uint32_t open = 0u;
    for (const uint32_t &id : ids_)
        if (__atomic_load_n(&id, __ATOMIC_ACQUIRE) != 0u) open++;
    return open;
}

void ExecutorDomains::report(StateWriter &writer) const
{
    AV_SCOPE("ExecutorDomains::report");
    for (uint32_t entry = 0u; entry < max_domains; entry++) {
        const uint32_t id = __atomic_load_n(&ids_[entry], __ATOMIC_ACQUIRE);
        if (id == 0u) continue;
        writer.open("domain", id);
        writer.number("gpu", __atomic_load_n(&gpus_[entry], __ATOMIC_RELAXED));
        writer.close();
    }
}

uint32_t ExecutorDomains::entry_of(uint32_t domain) const
{
    AV_SCOPE("ExecutorDomains::entry_of");
    for (uint32_t entry = 0u; entry < max_domains; entry++)
        if (__atomic_load_n(&ids_[entry], __ATOMIC_ACQUIRE) == domain) return entry;
    return max_domains;
}

}
