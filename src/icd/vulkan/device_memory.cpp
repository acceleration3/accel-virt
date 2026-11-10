#include "icd/vulkan/device_memory.hpp"

#include "core/log/driver_log.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/transport/host_allocation.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/vulkan/connection.hpp"
#include "icd/vulkan/handle_bits.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"

namespace av::icd {

DeviceMemory::DeviceMemory(Kmt &kmt, Connection &connection, vkproto::HandleTranslation &handles, DriverLog &log)
    : connection_(connection), handles_(handles), lines_(log), types_(kmt, connection.adapter()), learnt_(false), placing_(false)
{
    AV_SCOPE("DeviceMemory::DeviceMemory");
}

bool DeviceMemory::learn(VkPhysicalDevice physical)
{
    AV_SCOPE("DeviceMemory::learn");
    if (learnt_) return true;
    VkPhysicalDeviceMemoryProperties host = {};
    vkproto::av_vkGetPhysicalDeviceMemoryProperties(connection_.transport(), handles_, physical, &host);
    if (connection_.lost()) {
        lines_.write("vkAllocateMemory: the device's memory types cannot be learnt on a lost device");
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

VkResult DeviceMemory::allocate(VkPhysicalDevice physical, VkDevice device, const VkMemoryAllocateInfo &info,
                                const VkAllocationCallbacks *allocator, VkDeviceMemory &memory)
{
    AV_SCOPE("DeviceMemory::allocate");
    if (!learn(physical)) return refusal();
    for (auto *link = static_cast<const VkBaseInStructure *>(info.pNext); link != nullptr; link = link->pNext)
        if (link->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT)
            return lend(info, *reinterpret_cast<const VkImportMemoryHostPointerInfoEXT *>(link), memory);
    if (placing_ && types_.placed(info.memoryTypeIndex)) return place(device, info, allocator, memory);
    if (!types_.host_visible(info.memoryTypeIndex)) {
        return vkproto::av_vkAllocateMemory(connection_.transport(), handles_, device, &info, allocator, &memory);
    }
    if (info.allocationSize == 0u) return refusal();
    HostAllocation *made = nullptr;
    const Status    opened = connection_.channel().open_memory(
        info.allocationSize, HostMemoryRequest(HostMemoryRequest::Kind::vulkan, info.memoryTypeIndex), this, made);
    if (opened != Status::ok) {
        lines_.note("vkAllocateMemory: no host-visible memory, status", static_cast<uint64_t>(opened));
        return refusal(opened);
    }
    memory = HandleBits(made->handle()).as<VkDeviceMemory>();
    return VK_SUCCESS;
}

VkResult DeviceMemory::place(VkDevice device, const VkMemoryAllocateInfo &info, const VkAllocationCallbacks *allocator,
                             VkDeviceMemory &memory)
{
    AV_SCOPE("DeviceMemory::place");
    if (info.allocationSize == 0u) return refusal();
    const VkResult made = vkproto::av_vkAllocateMemory(connection_.transport(), handles_, device, &info, allocator, &memory);
    if (made != VK_SUCCESS) return made;
    HostAllocation *view = nullptr;
    const Status    opened = connection_.channel().place_memory(
        info.allocationSize, HostMemoryRequest(HostMemoryRequest::Kind::placed, info.memoryTypeIndex), this,
        HandleBits(memory).value(), view);
    if (opened == Status::ok) return VK_SUCCESS;
    lines_.note("vkAllocateMemory: the device mapped no placed memory, status", static_cast<uint64_t>(opened));
    vkproto::av_vkFreeMemory(connection_.transport(), handles_, device, memory, allocator);
    memory = VK_NULL_HANDLE;
    return refusal(opened);
}

void DeviceMemory::enable_placement()
{
    AV_SCOPE("DeviceMemory::enable_placement");
    placing_ = true;
}

bool DeviceMemory::placing() const
{
    AV_SCOPE("DeviceMemory::placing");
    return placing_;
}

VkResult DeviceMemory::lend(const VkMemoryAllocateInfo &info, const VkImportMemoryHostPointerInfoEXT &import,
                            VkDeviceMemory &memory)
{
    AV_SCOPE("DeviceMemory::lend");
    const auto at = reinterpret_cast<uintptr_t>(import.pHostPointer);
    if (import.handleType != VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT || at == 0u ||
        at % host_pointer_alignment != 0u || info.allocationSize == 0u ||
        info.allocationSize % host_pointer_alignment != 0u || info.memoryTypeIndex >= 32u ||
        (types_.lendable_types() & (1u << info.memoryTypeIndex)) == 0u)
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    HostAllocation *made = nullptr;
    const Status    lent = connection_.channel().adopt_memory(
        import.pHostPointer, info.allocationSize, HostMemoryRequest(HostMemoryRequest::Kind::vulkan, info.memoryTypeIndex),
        this, made);
    if (lent == Status::no_memory) return VK_ERROR_OUT_OF_HOST_MEMORY;
    if (lent != Status::ok) {
        lines_.note("vkAllocateMemory: the device took no host pointer's pages, status", static_cast<uint64_t>(lent));
        return connection_.lost() ? VK_ERROR_DEVICE_LOST : VK_ERROR_INVALID_EXTERNAL_HANDLE;
    }
    memory = HandleBits(made->handle()).as<VkDeviceMemory>();
    return VK_SUCCESS;
}

VkResult DeviceMemory::host_pointer_types(VkPhysicalDevice physical, VkExternalMemoryHandleTypeFlagBits type,
                                          const void *pointer, uint32_t &types)
{
    AV_SCOPE("DeviceMemory::host_pointer_types");
    types = 0u;
    if (type != VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT || pointer == nullptr ||
        reinterpret_cast<uintptr_t>(pointer) % host_pointer_alignment != 0u)
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    if (!learn(physical)) return refusal();
    types = types_.lendable_types();
    return VK_SUCCESS;
}

VkResult DeviceMemory::refusal() const
{
    AV_SCOPE("DeviceMemory::refusal");
    return connection_.lost() ? VK_ERROR_DEVICE_LOST : VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

VkResult DeviceMemory::refusal(Status refused) const
{
    AV_SCOPE("DeviceMemory::refusal");
    return refused == Status::no_memory ? VK_ERROR_OUT_OF_HOST_MEMORY : refusal();
}

HostAllocation *DeviceMemory::find(VkDeviceMemory memory) const
{
    AV_SCOPE("DeviceMemory::find");
    return connection_.channel().find_memory(HandleBits(memory).value(), this);
}

void DeviceMemory::free(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *allocator)
{
    AV_SCOPE("DeviceMemory::free");
    if (memory == VK_NULL_HANDLE) return;
    HostAllocation *const held = find(memory);
    if (held == nullptr) {
        vkproto::av_vkFreeMemory(connection_.transport(), handles_, device, memory, allocator);
        return;
    }
    const bool    placed = held->placed();
    const Status released = connection_.channel().close_memory(*held);
    if (placed) vkproto::av_vkFreeMemory(connection_.transport(), handles_, device, memory, allocator);
    if (released != Status::ok)
        lines_.note("vkFreeMemory: the host refused to release the memory, status", static_cast<uint64_t>(released));
}

VkResult DeviceMemory::map(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags,
                           void *&data) const
{
    AV_SCOPE("DeviceMemory::map");
    const HostAllocation *const held = find(memory);
    if (held == nullptr || flags != 0u || offset >= held->bytes()) return VK_ERROR_MEMORY_MAP_FAILED;
    uint8_t *const view = held->view_at(offset, size == VK_WHOLE_SIZE ? held->bytes() - offset : size);
    if (view == nullptr) return VK_ERROR_MEMORY_MAP_FAILED;
    data = view;
    return VK_SUCCESS;
}

Status DeviceMemory::release_all()
{
    AV_SCOPE("DeviceMemory::release_all");
    return connection_.channel().close_owned(this);
}

void DeviceMemory::report(StateWriter &writer) const
{
    AV_SCOPE("DeviceMemory::report");
    writer.flag("types_learnt", learnt_);
}

bool DeviceMemory::invariants_hold() const
{
    AV_SCOPE("DeviceMemory::invariants_hold");
    return types_.invariants_hold();
}

}
