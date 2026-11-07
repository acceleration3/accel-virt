#include "executor/vulkan/vram_budget.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

VramBudget::VramBudget(BlockSource &blocks) : host_(), caps_{}, used_{}, allocations_(blocks, Pool::paged)
{
    AV_SCOPE("VramBudget::VramBudget");
}

uint64_t VramBudget::cap_for(uint64_t total) const
{
    AV_SCOPE("VramBudget::cap_for");
    const uint64_t headroom = total / headroom_divisor > headroom_floor ? total / headroom_divisor : headroom_floor;
    return total - headroom >= total / 2u ? total - headroom : total / 2u;
}

void VramBudget::learn(const VkPhysicalDeviceMemoryProperties &host)
{
    AV_SCOPE("VramBudget::learn");
    host_ = host;
    for (uint32_t heap = 0u; heap < VK_MAX_MEMORY_HEAPS; heap++) {
        caps_[heap] = 0u;
        if (heap >= host.memoryHeapCount) continue;
        const bool local = (host.memoryHeaps[heap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0u;
        caps_[heap] = local ? cap_for(host.memoryHeaps[heap].size) : host.memoryHeaps[heap].size;
    }
}

bool VramBudget::capped(uint32_t heap) const
{
    AV_SCOPE("VramBudget::capped");
    return heap < host_.memoryHeapCount && (host_.memoryHeaps[heap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0u;
}

void VramBudget::clamp(VkPhysicalDeviceMemoryProperties &answer) const
{
    AV_SCOPE("VramBudget::clamp");
    for (uint32_t heap = 0u; heap < answer.memoryHeapCount && heap < VK_MAX_MEMORY_HEAPS; heap++)
        if (capped(heap) && caps_[heap] < answer.memoryHeaps[heap].size) answer.memoryHeaps[heap].size = caps_[heap];
}

void VramBudget::report(VkPhysicalDeviceMemoryBudgetPropertiesEXT &budget) const
{
    AV_SCOPE("VramBudget::report");
    for (uint32_t heap = 0u; heap < VK_MAX_MEMORY_HEAPS; heap++) {
        if (heap >= host_.memoryHeapCount) {
            budget.heapBudget[heap] = 0u;
            budget.heapUsage[heap] = 0u;
            continue;
        }
        budget.heapUsage[heap] = used_[heap];
        budget.heapBudget[heap] = caps_[heap] > used_[heap] ? caps_[heap] : used_[heap];
    }
}

bool VramBudget::fits(uint32_t heap, uint64_t size) const
{
    AV_SCOPE("VramBudget::fits");
    if (heap >= VK_MAX_MEMORY_HEAPS || !capped(heap)) return true;
    return size <= caps_[heap] && used_[heap] <= caps_[heap] - size;
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
