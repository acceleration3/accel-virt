#include "executor/shim/vulkan_buffer.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/owned_fd.hpp"
#include "executor/shim/vulkan_device_table.hpp"

namespace av {

VulkanBuffer::VulkanBuffer() : table_(nullptr), buffer_(VK_NULL_HANDLE), memory_(VK_NULL_HANDLE)
{
    AV_SCOPE("VulkanBuffer::VulkanBuffer");
}

VulkanBuffer::~VulkanBuffer()
{
    AV_SCOPE("VulkanBuffer::~VulkanBuffer");
    release();
}

bool VulkanBuffer::import(const VulkanDeviceTable &table, OwnedFd &descriptor, VkDeviceSize size,
                          VkBufferUsageFlags usage, Placement placement)
{
    AV_SCOPE("VulkanBuffer::import");
    const uint32_t types = table.dma_buf_types(descriptor.fd());
    const VkExternalMemoryBufferCreateInfo external = {VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, nullptr,
                                                       VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    const VkBufferCreateInfo info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, &external, 0u, size, usage,
                                     VK_SHARING_MODE_EXCLUSIVE, 0u, nullptr};
    if (made() || size == 0u || types == 0u || !table.create_buffer(info, buffer_)) return false;
    table_ = &table;
    const VkMemoryRequirements          needs = table.buffer_requirements(buffer_);
    const VkMemoryDedicatedAllocateInfo dedicated = {VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, nullptr,
                                                     VK_NULL_HANDLE, buffer_};
    const VkMemoryAllocateFlagsInfo     addressed = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, &dedicated,
                                                     VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, 0u};
    const void *const next = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0u
                                 ? static_cast<const void *>(&addressed)
                                 : static_cast<const void *>(&dedicated);
    const uint32_t allowed = types & needs.memoryTypeBits;
    const uint32_t chosen = placement == Placement::coherent ? table.coherent_memory_type(allowed)
                                                              : table.memory_type(allowed);
    const uint32_t type = needs.size <= size ? chosen : VK_MAX_MEMORY_TYPES;
    if (table.import_memory(descriptor, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, size, type, next, memory_) &&
        table.bind_buffer(buffer_, memory_))
        return true;
    release();
    return false;
}

bool VulkanBuffer::import_dma_buf_to_draw(const VulkanDeviceTable &table, OwnedFd &descriptor, VkDeviceSize size)
{
    AV_SCOPE("VulkanBuffer::import_dma_buf_to_draw");
    constexpr VkBufferUsageFlags drawn =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
        VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    return import(table, descriptor, size, drawn, Placement::fastest);
}

bool VulkanBuffer::import_dma_buf_to_receive(const VulkanDeviceTable &table, OwnedFd &descriptor, VkDeviceSize size)
{
    AV_SCOPE("VulkanBuffer::import_dma_buf_to_receive");
    return import(table, descriptor, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, Placement::coherent);
}

void VulkanBuffer::release()
{
    AV_SCOPE("VulkanBuffer::release");
    if (table_ != nullptr) {
        table_->destroy_buffer(buffer_);
        table_->free_memory(memory_);
    }
    table_ = nullptr;
    buffer_ = VK_NULL_HANDLE;
    memory_ = VK_NULL_HANDLE;
}

bool VulkanBuffer::made() const
{
    AV_SCOPE("VulkanBuffer::made");
    return buffer_ != VK_NULL_HANDLE;
}

VkBuffer VulkanBuffer::buffer() const
{
    AV_SCOPE("VulkanBuffer::buffer");
    return buffer_;
}

}
