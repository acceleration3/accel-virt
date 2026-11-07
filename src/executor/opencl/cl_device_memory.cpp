#include "executor/opencl/cl_device_memory.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/byte_ledger.hpp"

namespace av {

namespace {

constexpr cl_mem_flags host_resident = CL_MEM_USE_HOST_PTR | CL_MEM_ALLOC_HOST_PTR;

uint64_t key_of(cl_mem memory)
{
    AV_SCOPE("key_of");
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(memory));
}

}

ClDeviceMemory::ClDeviceMemory(BlockSource &blocks) : account_(), host_(), sizes_(blocks, Pool::paged)
{
    AV_SCOPE("ClDeviceMemory::ClDeviceMemory");
}

void ClDeviceMemory::bind(ByteLedger *vram, ByteLedger *host)
{
    AV_SCOPE("ClDeviceMemory::bind");
    account_.bind(vram);
    host_.bind(host);
}

cl_int ClDeviceMemory::claim(cl_mem_flags flags, uint64_t bytes, uint64_t &claimed)
{
    AV_SCOPE("ClDeviceMemory::claim");
    claimed = 0u;
    if (bytes == 0u || bytes >= host_bit) return CL_SUCCESS;
    if ((flags & host_resident) != 0u) {
        if (!host_.bound()) return CL_SUCCESS;
        if (!host_.take(bytes)) return CL_OUT_OF_HOST_MEMORY;
        claimed = bytes | host_bit;
        return CL_SUCCESS;
    }
    if (!account_.bound()) return CL_SUCCESS;
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
    unclaim(claimed);
    return false;
}

void ClDeviceMemory::unclaim(uint64_t claimed)
{
    AV_SCOPE("ClDeviceMemory::unclaim");
    if ((claimed & host_bit) != 0u) host_.give(claimed & ~host_bit);
    else account_.give(claimed);
}

void ClDeviceMemory::forget(cl_mem memory)
{
    AV_SCOPE("ClDeviceMemory::forget");
    uint64_t bytes = 0u;
    if (sizes_.take(key_of(memory), bytes)) unclaim(bytes);
}

void ClDeviceMemory::clear()
{
    AV_SCOPE("ClDeviceMemory::clear");
    sizes_.clear();
    static_cast<void>(account_.close());
    static_cast<void>(host_.close());
}

uint64_t ClDeviceMemory::held() const
{
    AV_SCOPE("ClDeviceMemory::held");
    return account_.held();
}

uint64_t ClDeviceMemory::host_held() const
{
    AV_SCOPE("ClDeviceMemory::host_held");
    return host_.held();
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
    uint64_t resident = 0u;
    sizes_.for_each([&total, &resident](uint64_t, const uint64_t &bytes) {
        if ((bytes & host_bit) != 0u) resident += bytes & ~host_bit;
        else total += bytes;
    });
    return total == account_.held() && resident == host_.held() && sizes_.invariants_hold();
}

}
