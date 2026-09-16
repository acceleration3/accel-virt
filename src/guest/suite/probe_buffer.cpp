#include "guest/suite/probe_buffer.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/suite/probe_device.hpp"
#include "guest/suite/probe_outcome.hpp"
#include "guest/suite/vulkan_entry.hpp"

namespace av {

ProbeBuffer::ProbeBuffer(ProbeDevice &device)
    : device_(device), buffer_(VK_NULL_HANDLE), memory_(VK_NULL_HANDLE), bytes_(nullptr)
    {
        AV_SCOPE("ProbeBuffer::ProbeBuffer");
    }

/* Vulkan makes destroying and freeing VK_NULL_HANDLE a no-op. */
ProbeBuffer::~ProbeBuffer()
{
    AV_SCOPE("ProbeBuffer::~ProbeBuffer");
    device_.vulkan().destroy_buffer(device_.device(), buffer_);
    device_.vulkan().free_memory(device_.device(), memory_);
}

bool ProbeBuffer::make(VkDeviceSize bytes, VkBufferUsageFlags usage, ProbeOutcome &outcome)
{
    AV_SCOPE("ProbeBuffer::make");
    VulkanEntry   &vulkan = device_.vulkan();
    const VkDevice device = device_.device();
    VkBufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = bytes;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (!outcome.accept(vulkan.create_buffer(device, &info, &buffer_))) return false;
    VkMemoryRequirements requirements = {};
    vulkan.buffer_memory_requirements(device, buffer_, &requirements);
    uint32_t type = 0u;
    if (!device_.memory_type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, type)) {
        outcome.missing();
        return false;
    }
    VkMemoryAllocateInfo allocation = {};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = type;
    if (!outcome.accept(vulkan.allocate_memory(device, &allocation, &memory_))) return false;
    if (!outcome.accept(vulkan.bind_buffer_memory(device, buffer_, memory_))) return false;
    void *mapped = nullptr;
    if (!outcome.accept(vulkan.map_memory(device, memory_, &mapped))) return false;
    bytes_ = static_cast<unsigned char *>(mapped);
    return true;
}

VkBuffer       ProbeBuffer::buffer() const
{
    AV_SCOPE("ProbeBuffer::buffer");
    return buffer_;
}
unsigned char *ProbeBuffer::bytes() const
{
    AV_SCOPE("ProbeBuffer::bytes");
    return bytes_;
}

bool ProbeBuffer::invariants_hold() const
{
    AV_SCOPE("ProbeBuffer::invariants_hold");
    if (bytes_ != nullptr && memory_ == VK_NULL_HANDLE) return false;
    return memory_ == VK_NULL_HANDLE || buffer_ != VK_NULL_HANDLE;
}

}  // namespace av
