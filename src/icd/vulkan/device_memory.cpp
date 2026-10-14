#include "icd/vulkan/device_memory.hpp"

#include "core/log/log_sink.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/transport/host_allocation.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/vulkan/connection.hpp"
#include "icd/vulkan/handle_bits.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"

namespace av::icd {

DeviceMemory::DeviceMemory(Kmt &kmt, Connection &connection, LogSink &log)
    : connection_(connection), log_(log), lines_(log, "avvk: "), types_(kmt, connection.adapter()), learnt_(false),
      held_{}, count_(0u)
{
    AV_SCOPE("DeviceMemory::DeviceMemory");
}

bool DeviceMemory::learn(VkPhysicalDevice physical)
{
    AV_SCOPE("DeviceMemory::learn");
    if (learnt_) return true;
    VkPhysicalDeviceMemoryProperties host = {};
    vkproto::av_vkGetPhysicalDeviceMemoryProperties(connection_.transport(), physical, &host);
    if (connection_.faulted()) {
        log_.write("avvk: vkAllocateMemory: the device's memory types cannot be learnt on a lost device");
        return false;
    }
    const Status built = types_.build(host);
    if (built != Status::ok) {
        lines_.note("vkAllocateMemory: the device's memory types cannot be learnt, status", static_cast<uint64_t>(built));
        return false;
    }
    learnt_ = true;
    return true;
}

/* One host allocation of exactly the type asked: a driver that sub-allocates
 * does so itself, as on real hardware, and memory a guest frees is the host's
 * again at once (docs/host-memory.md). */
VkResult DeviceMemory::allocate(VkPhysicalDevice physical, VkDevice device, const VkMemoryAllocateInfo &info,
                                const VkAllocationCallbacks *allocator, VkDeviceMemory &memory)
{
    AV_SCOPE("DeviceMemory::allocate");
    if (!learn(physical)) return refusal();
    if (!types_.host_visible(info.memoryTypeIndex)) {
        return vkproto::av_vkAllocateMemory(connection_.transport(), device, &info, allocator, &memory);
    }
    const uint32_t slot = slot_of(nullptr);
    if (info.allocationSize == 0u || slot == max_allocations) return refusal();
    HostAllocation *made = nullptr;
    const Status    opened = connection_.open_memory(
        info.allocationSize, HostMemoryRequest(HostMemoryRequest::Kind::vulkan, info.memoryTypeIndex), made);
    if (opened != Status::ok) {
        lines_.note("vkAllocateMemory: no host-visible memory, status", static_cast<uint64_t>(opened));
        return refusal();
    }
    held_[slot] = made;
    count_++;
    memory = HandleBits(made->handle()).as<VkDeviceMemory>();
    return VK_SUCCESS;
}

VkResult DeviceMemory::refusal() const
{
    AV_SCOPE("DeviceMemory::refusal");
    return connection_.faulted() ? VK_ERROR_DEVICE_LOST : VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

uint32_t DeviceMemory::slot_of(const HostAllocation *allocation) const
{
    AV_SCOPE("DeviceMemory::slot_of");
    for (uint32_t slot = 0u; slot < max_allocations; slot++)
        if (held_[slot] == allocation) return slot;
    return max_allocations;
}

HostAllocation *DeviceMemory::find(VkDeviceMemory memory) const
{
    AV_SCOPE("DeviceMemory::find");
    const uint64_t handle = HandleBits(memory).value();
    for (HostAllocation *const held : held_)
        if (held != nullptr && held->handle() == handle) return held;
    return nullptr;
}

void DeviceMemory::free(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *allocator)
{
    AV_SCOPE("DeviceMemory::free");
    if (memory == VK_NULL_HANDLE) return;
    HostAllocation *const held = find(memory);
    if (held == nullptr) {
        vkproto::av_vkFreeMemory(connection_.transport(), device, memory, allocator);
        return;
    }
    held_[slot_of(held)] = nullptr;
    count_--;
    const Status released = connection_.close_memory(*held);
    if (released != Status::ok)
        lines_.note("vkFreeMemory: the host refused to release the memory, status", static_cast<uint64_t>(released));
}

VkResult DeviceMemory::map(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags,
                           void *&data) const
{
    AV_SCOPE("DeviceMemory::map");
    const HostAllocation *const held = find(memory);
    if (held == nullptr || flags != 0u || size == 0u) return VK_ERROR_MEMORY_MAP_FAILED;
    const uint64_t bytes = held->bytes();
    if (offset >= bytes || (size != VK_WHOLE_SIZE && size > bytes - offset)) return VK_ERROR_MEMORY_MAP_FAILED;
    data = held->view() + offset;
    return VK_SUCCESS;
}

Status DeviceMemory::release_all()
{
    AV_SCOPE("DeviceMemory::release_all");
    Status first = Status::ok;
    for (HostAllocation *&held : held_) {
        if (held == nullptr) continue;
        const Status released = connection_.close_memory(*held);
        if (first == Status::ok) first = released;
        held = nullptr;
    }
    count_ = 0u;
    return first;
}

void DeviceMemory::report(StateWriter &writer) const
{
    AV_SCOPE("DeviceMemory::report");
    writer.flag("types_learnt", learnt_);
    writer.number("host_allocations", count_);
}

/* Every host allocation held is one the host imported, and none is held
 * before the types that say which are host visible were learnt. */
bool DeviceMemory::invariants_hold() const
{
    AV_SCOPE("DeviceMemory::invariants_hold");
    uint32_t held = 0u;
    for (const HostAllocation *const memory : held_) {
        if (memory == nullptr) continue;
        if (!memory->held()) return false;
        held++;
    }
    return held == count_ && (learnt_ || count_ == 0u);
}

}  // namespace av::icd
