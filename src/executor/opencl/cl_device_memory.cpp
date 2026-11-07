#include "executor/opencl/cl_device_memory.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/vram_ledger.hpp"

namespace av {

namespace {

constexpr cl_mem_flags host_resident = CL_MEM_USE_HOST_PTR | CL_MEM_ALLOC_HOST_PTR;

uint64_t key_of(cl_mem memory)
{
    AV_SCOPE("key_of");
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(memory));
}

}

ClDeviceMemory::ClDeviceMemory(BlockSource &blocks) : account_(), sizes_(blocks, Pool::paged)
{
    AV_SCOPE("ClDeviceMemory::ClDeviceMemory");
}

void ClDeviceMemory::bind(VramLedger *ledger)
{
    AV_SCOPE("ClDeviceMemory::bind");
    account_.bind(ledger);
}

cl_int ClDeviceMemory::claim(cl_mem_flags flags, uint64_t bytes, uint64_t &claimed)
{
    AV_SCOPE("ClDeviceMemory::claim");
    claimed = 0u;
    if (!account_.bound() || bytes == 0u || (flags & host_resident) != 0u) return CL_SUCCESS;
    if (!account_.take(bytes)) return CL_MEM_OBJECT_ALLOCATION_FAILURE;
    claimed = bytes;
    return CL_SUCCESS;
}

bool ClDeviceMemory::keep(cl_mem memory, uint64_t claimed)
{
    AV_SCOPE("ClDeviceMemory::keep");
    if (claimed == 0u) return true;
    uint32_t     slot = 0u;
    const Status kept = sizes_.insert(key_of(memory), claimed, slot);
    if (kept == Status::ok) return true;
    account_.give(claimed);
    return false;
}

void ClDeviceMemory::unclaim(uint64_t claimed)
{
    AV_SCOPE("ClDeviceMemory::unclaim");
    account_.give(claimed);
}

void ClDeviceMemory::forget(cl_mem memory)
{
    AV_SCOPE("ClDeviceMemory::forget");
    uint64_t bytes = 0u;
    if (sizes_.take(key_of(memory), bytes)) account_.give(bytes);
}

void ClDeviceMemory::clear()
{
    AV_SCOPE("ClDeviceMemory::clear");
    sizes_.clear();
    static_cast<void>(account_.close());
}

uint64_t ClDeviceMemory::held() const
{
    AV_SCOPE("ClDeviceMemory::held");
    return account_.held();
}

uint64_t ClDeviceMemory::cap() const
{
    AV_SCOPE("ClDeviceMemory::cap");
    return account_.cap();
}

uint64_t ClDeviceMemory::free_bytes() const
{
    AV_SCOPE("ClDeviceMemory::free_bytes");
    return account_.free_bytes();
}

uint32_t ClDeviceMemory::objects() const
{
    AV_SCOPE("ClDeviceMemory::objects");
    return sizes_.count();
}

bool ClDeviceMemory::invariants_hold() const
{
    AV_SCOPE("ClDeviceMemory::invariants_hold");
    uint64_t total = 0u;
    sizes_.for_each([&total](uint64_t, const uint64_t &bytes) { total += bytes; });
    return total == account_.held() && sizes_.invariants_hold();
}

}
