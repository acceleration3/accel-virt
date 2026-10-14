#include "executor/shim/vulkan_buffer.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/vulkan_device_table.hpp"

#include <unistd.h>

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

/* An allocation that succeeded owns the fd even when the bind after it fails;
 * one that failed left it with the caller. A buffer whose device address is
 * taken needs memory allocated for it. */
bool VulkanBuffer::import(const VulkanDeviceTable &table, int fd, VkDeviceSize size, VkBufferUsageFlags usage)
{
    AV_SCOPE("VulkanBuffer::import");
    if (fd < 0) return false;
    const uint32_t types = table.dma_buf_types(fd);
    const VkExternalMemoryBufferCreateInfo external = {VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, nullptr,
                                                       VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    const VkBufferCreateInfo info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, &external, 0u, size, usage,
                                     VK_SHARING_MODE_EXCLUSIVE, 0u, nullptr};
    if (made() || size == 0u || types == 0u || !table.create_buffer(info, buffer_)) {
        close(fd);
        return false;
    }
    table_ = &table;
    const VkMemoryRequirements needs = table.buffer_requirements(buffer_);
    const VkImportMemoryFdInfoKHR imported = {VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR, nullptr,
                                              VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd};
    const VkMemoryAllocateFlagsInfo addressed = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, &imported,
                                                 VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, 0u};
    const void *const chain = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0u
                                  ? static_cast<const void *>(&addressed)
                                  : static_cast<const void *>(&imported);
    /* The memory is the dma-buf's size: a buffer needing more cannot be bound. */
    const uint32_t type = needs.size <= size ? table.memory_type(types & needs.memoryTypeBits) : VK_MAX_MEMORY_TYPES;
    if (table.bind_buffer(buffer_, chain, size, type, memory_)) return true;
    if (memory_ == VK_NULL_HANDLE) close(fd);
    release();
    return false;
}

/* Everything Direct3D 11 binds a buffer as, and its device address, which
 * DXVK takes of storage it renames a buffer to. */
bool VulkanBuffer::import_dma_buf_to_draw(const VulkanDeviceTable &table, int fd, VkDeviceSize size)
{
    AV_SCOPE("VulkanBuffer::import_dma_buf_to_draw");
    constexpr VkBufferUsageFlags drawn =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
        VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    return import(table, fd, size, drawn);
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

}  // namespace av
