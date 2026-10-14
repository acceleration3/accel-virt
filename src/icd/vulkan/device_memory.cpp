#include "icd/vulkan/device_memory.hpp"

#include "core/log/log_sink.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/vulkan/connection.hpp"
#include "icd/vulkan/handle_bits.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"

namespace av::icd {

DeviceMemory::DeviceMemory(BlockSource &memory, Kmt &kmt, Connection &connection, LogSink &log)
    : connection_(connection), log_(log), lines_(log, "avvk: "), types_(kmt, connection.adapter()), learnt_(false),
      visible_(memory)
{
    AV_SCOPE("DeviceMemory::DeviceMemory");
}

DeviceMemory::~DeviceMemory()
{
    AV_SCOPE("DeviceMemory::~DeviceMemory");
    static_cast<void>(release_all());
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
    Visible *made = nullptr;
    if (info.allocationSize == 0u || visible_.create(made) != Status::ok) return refusal();
    const Status opened = connection_.open_memory(
        *made, info.allocationSize, HostMemoryRequest(HostMemoryRequest::Kind::vulkan, info.memoryTypeIndex));
    if (opened != Status::ok) {
        static_cast<void>(visible_.destroy(made));
        lines_.note("vkAllocateMemory: no host-visible memory, status", static_cast<uint64_t>(opened));
        return refusal();
    }
    memory = HandleBits(made->object().handle()).as<VkDeviceMemory>();
    return VK_SUCCESS;
}

VkResult DeviceMemory::refusal() const
{
    AV_SCOPE("DeviceMemory::refusal");
    return connection_.faulted() ? VK_ERROR_DEVICE_LOST : VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

DeviceMemory::Visible *DeviceMemory::find(VkDeviceMemory memory) const
{
    AV_SCOPE("DeviceMemory::find");
    const uint64_t handle = HandleBits(memory).value();
    return const_cast<ObjectPool<Visible> &>(visible_).find_if(
        [handle](const Visible &held) { return held.constructed() && held.object().handle() == handle; });
}

void DeviceMemory::free(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *allocator)
{
    AV_SCOPE("DeviceMemory::free");
    if (memory == VK_NULL_HANDLE) return;
    Visible *const held = find(memory);
    if (held == nullptr) {
        vkproto::av_vkFreeMemory(connection_.transport(), device, memory, allocator);
        return;
    }
    const Status released = connection_.close_memory(*held);
    static_cast<void>(visible_.destroy(held));
    if (released != Status::ok)
        lines_.note("vkFreeMemory: the host refused to release the memory, status", static_cast<uint64_t>(released));
}

VkResult DeviceMemory::map(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags,
                           void *&data) const
{
    AV_SCOPE("DeviceMemory::map");
    const Visible *const held = find(memory);
    if (held == nullptr || flags != 0u || size == 0u) return VK_ERROR_MEMORY_MAP_FAILED;
    const uint64_t bytes = held->object().bytes();
    if (offset >= bytes || (size != VK_WHOLE_SIZE && size > bytes - offset)) return VK_ERROR_MEMORY_MAP_FAILED;
    data = held->object().view() + offset;
    return VK_SUCCESS;
}

Status DeviceMemory::release_all()
{
    AV_SCOPE("DeviceMemory::release_all");
    Status first = Status::ok;
    for (Visible *held = nullptr; (held = visible_.find_if([](const Visible &) { return true; })) != nullptr;) {
        const Status released = connection_.close_memory(*held);
        if (first == Status::ok) first = released;
        static_cast<void>(visible_.destroy(held));
    }
    return first;
}

void DeviceMemory::report(StateWriter &writer) const
{
    AV_SCOPE("DeviceMemory::report");
    writer.flag("types_learnt", learnt_);
    writer.number("host_allocations", visible_.live());
}

/* Every host allocation held is one the host imported, and none is held
 * before the types that say which are host visible were learnt. */
bool DeviceMemory::invariants_hold() const
{
    AV_SCOPE("DeviceMemory::invariants_hold");
    const bool held = visible_.all_of([](const Visible &memory) {
        return memory.constructed() && memory.object().held() && memory.object().invariants_hold();
    });
    return held && (learnt_ || visible_.live() == 0u);
}

}  // namespace av::icd
