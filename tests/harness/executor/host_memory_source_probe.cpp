#include "harness/executor/host_memory_source_probe.hpp"

#include "device/abi/av_executor.h"
#include "executor/source_memory.hpp"

namespace av {

HostMemorySourceProbe::HostMemorySourceProbe(HostMemorySource &target) : source(target) {}

Status HostMemorySourceProbe::add(uint32_t domain, uint64_t key, const ExportedMemory &memory)
{
    SourceMemory *made = nullptr;
    return source.memories_.create(made, domain, key, 0u, AV_MEMORY_WRITE_BACK, memory);
}

void HostMemorySourceProbe::drain(SourceMemory &memory)
{
    while (memory.references() != 0u) static_cast<void>(memory.release());
}

void HostMemorySourceProbe::mark_visible(uint32_t types) { source.visible_ |= types; }

ObjectPool<SourceMemory> &HostMemorySourceProbe::pool() { return source.memories_; }

}  // namespace av
