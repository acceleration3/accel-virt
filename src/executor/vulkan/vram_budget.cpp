#include "executor/vulkan/vram_budget.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/byte_ledger.hpp"

namespace av {

VramBudget::VramBudget(BlockSource &blocks) : host_(), account_(), host_account_(), used_{}, allocations_(blocks, Pool::paged)
{
    AV_SCOPE("VramBudget::VramBudget");
}

void VramBudget::bind(ByteLedger *vram, ByteLedger *host)
{
    AV_SCOPE("VramBudget::bind");
    account_.bind(vram);
    host_account_.bind(host);
}

void VramBudget::learn(const VkPhysicalDeviceMemoryProperties &host)
{
    AV_SCOPE("VramBudget::learn");
    host_ = host;
}

bool VramBudget::capped(uint32_t heap) const
{
    AV_SCOPE("VramBudget::capped");
    return heap < host_.memoryHeapCount && (host_.memoryHeaps[heap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0u;
}

bool VramBudget::system(uint32_t heap) const
{
    AV_SCOPE("VramBudget::system");
    return heap < host_.memoryHeapCount && !capped(heap);
}

void VramBudget::clamp(VkPhysicalDeviceMemoryProperties &answer) const
{
    AV_SCOPE("VramBudget::clamp");
    const uint64_t cap = account_.advertised();
    const uint64_t host_cap = host_account_.advertised();
    for (uint32_t heap = 0u; heap < answer.memoryHeapCount && heap < VK_MAX_MEMORY_HEAPS; heap++) {
        const uint64_t limit = capped(heap) ? cap : host_cap;
        if (limit < answer.memoryHeaps[heap].size) answer.memoryHeaps[heap].size = limit;
    }
}

void VramBudget::report(VkPhysicalDeviceMemoryBudgetPropertiesEXT &budget) const
{
    AV_SCOPE("VramBudget::report");
    const uint64_t cap = account_.cap();
    const uint64_t ledger_used = account_.used();
    const uint64_t host_cap = host_account_.cap();
    const uint64_t host_used = host_account_.used();
    for (uint32_t heap = 0u; heap < VK_MAX_MEMORY_HEAPS; heap++) {
        if (heap >= host_.memoryHeapCount) {
            budget.heapBudget[heap] = 0u;
            budget.heapUsage[heap] = 0u;
            continue;
        }
        const bool     local = capped(heap);
        const uint64_t size = host_.memoryHeaps[heap].size;
        const uint64_t usage = local ? ledger_used : (host_account_.bound() ? host_used : used_[heap]);
        const uint64_t ceiling = local ? cap : host_cap;
        const uint64_t limit = ceiling < size ? ceiling : size;
        budget.heapUsage[heap] = usage;
        budget.heapBudget[heap] = limit > usage ? limit : usage;
    }
}

bool VramBudget::admit(uint32_t heap, uint64_t size)
{
    AV_SCOPE("VramBudget::admit");
    if (heap >= VK_MAX_MEMORY_HEAPS) return true;
    if (capped(heap)) return account_.take(size);
    return !system(heap) || host_account_.take(size);
}

void VramBudget::refund(uint32_t heap, uint64_t size)
{
    AV_SCOPE("VramBudget::refund");
    if (heap >= VK_MAX_MEMORY_HEAPS) return;
    if (capped(heap)) account_.give(size);
    else if (system(heap)) host_account_.give(size);
}

Status VramBudget::commit(uint32_t heap, uint64_t memory, uint64_t size)
{
    AV_SCOPE("VramBudget::commit");
    if (heap >= VK_MAX_MEMORY_HEAPS) return Status::ok;
    uint32_t       slot = 0u;
    const Status   inserted = allocations_.insert(memory, Allocation{size, heap}, slot);
    if (inserted != Status::ok) return inserted;
    used_[heap] += size;
    return Status::ok;
}

void VramBudget::release(uint64_t memory)
{
    AV_SCOPE("VramBudget::release");
    Allocation gone = {};
    if (!allocations_.take(memory, gone)) return;
    used_[gone.heap_] -= gone.size_;
    refund(gone.heap_, gone.size_);
}

void VramBudget::close()
{
    AV_SCOPE("VramBudget::close");
    static_cast<void>(account_.close());
    static_cast<void>(host_account_.close());
}

uint64_t VramBudget::held() const
{
    AV_SCOPE("VramBudget::held");
    return account_.held();
}

uint64_t VramBudget::host_held() const
{
    AV_SCOPE("VramBudget::host_held");
    return host_account_.held();
}

uint64_t VramBudget::used(uint32_t heap) const
{
    AV_SCOPE("VramBudget::used");
    return heap < VK_MAX_MEMORY_HEAPS ? used_[heap] : 0u;
}

bool VramBudget::invariants_hold() const
{
    AV_SCOPE("VramBudget::invariants_hold");
    uint64_t total = 0u;
    for (uint32_t heap = 0u; heap < VK_MAX_MEMORY_HEAPS; heap++) total += used_[heap];
    uint64_t held = 0u;
    allocations_.for_each([&held](uint64_t, const Allocation &one) { held += one.size_; });
    return total == held && allocations_.invariants_hold();
}

}
