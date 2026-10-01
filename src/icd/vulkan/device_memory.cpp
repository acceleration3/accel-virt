#include "icd/vulkan/device_memory.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/vulkan/connection.hpp"
#include "icd/common/log_sink.hpp"
#include "icd/vulkan/handle_bits.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"

namespace av::icd {

namespace {

constexpr size_t line_bytes = 160u;


}  // namespace

DeviceMemory::DeviceMemory(BlockSource &memory, Kmt &kmt, Connection &connection, LogSink &log)
    : connection_(connection), log_(log), types_(kmt, connection.adapter()), learnt_(false),
      chunks_(memory, connection)
      {
          AV_SCOPE("DeviceMemory::DeviceMemory");
      }

void DeviceMemory::note(const char *what, uint64_t value)
{
    AV_SCOPE("DeviceMemory::note");
    char       line[line_bytes] = {};
    TextBuffer text(line, sizeof line);
    text.append("avvk: ");
    text.append(what);
    text.append(" ");
    text.append_decimal(value);
    log_.write(text.text());
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
        note("vkAllocateMemory: the device's memory types cannot be learnt, status", static_cast<uint64_t>(built));
        return false;
    }
    learnt_ = true;
    return true;
}

VkResult DeviceMemory::allocate(VkPhysicalDevice physical, VkDevice device, const VkMemoryAllocateInfo &info,
                                const VkAllocationCallbacks *allocator, VkDeviceMemory &memory)
{
    AV_SCOPE("DeviceMemory::allocate");
    if (!learn(physical)) return refusal();
    if (!types_.host_visible(info.memoryTypeIndex)) {
        return vkproto::av_vkAllocateMemory(connection_.transport(), device, &info, allocator, &memory);
    }
    ChunkPlacement *placed = nullptr;
    const Status    status = chunks_.allocate(info.allocationSize, info.memoryTypeIndex, placed);
    if (status != Status::ok) {
        note("vkAllocateMemory: no host-visible memory, status", static_cast<uint64_t>(status));
        return refusal();
    }
    memory = HandleBits(placed->memory()).as<VkDeviceMemory>();
    return VK_SUCCESS;
}

VkResult DeviceMemory::refusal() const
{
    AV_SCOPE("DeviceMemory::refusal");
    return connection_.faulted() ? VK_ERROR_DEVICE_LOST : VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

void DeviceMemory::free(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *allocator)
{
    AV_SCOPE("DeviceMemory::free");
    if (memory == VK_NULL_HANDLE) return;
    ChunkPlacement *const placed = chunks_.find(HandleBits(memory).value());
    if (placed == nullptr) {
        vkproto::av_vkFreeMemory(connection_.transport(), device, memory, allocator);
        return;
    }
    const Status released = chunks_.free(*placed);
    if (released != Status::ok) note("vkFreeMemory: the import's release was refused, status", static_cast<uint64_t>(released));
}

VkResult DeviceMemory::map(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags,
                           void *&data) const
{
    AV_SCOPE("DeviceMemory::map");
    const ChunkPlacement *const placed = chunks_.find(HandleBits(memory).value());
    if (placed == nullptr || flags != 0u || size == 0u || offset >= placed->bytes()) return VK_ERROR_MEMORY_MAP_FAILED;
    if (size != VK_WHOLE_SIZE && size > placed->bytes() - offset) return VK_ERROR_MEMORY_MAP_FAILED;
    data = const_cast<unsigned char *>(placed->view() + offset);
    return VK_SUCCESS;
}

Status DeviceMemory::release_all()
{
    AV_SCOPE("DeviceMemory::release_all");
    return chunks_.release_all();
}

void DeviceMemory::report(StateWriter &writer) const
{
    AV_SCOPE("DeviceMemory::report");
    writer.flag("types_learnt", learnt_);
    writer.number("chunks", chunks_.chunks());
    writer.number("placements", chunks_.placements());
}

bool DeviceMemory::invariants_hold() const
{
    AV_SCOPE("DeviceMemory::invariants_hold");
    return chunks_.invariants_hold() && (learnt_ || chunks_.placements() == 0u);
}

}  // namespace av::icd
