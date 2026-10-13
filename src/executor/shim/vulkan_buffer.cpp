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
 * one that failed left it with the caller. */
bool VulkanBuffer::import_dma_buf(const VulkanDeviceTable &table, int fd, VkDeviceSize size)
{
    AV_SCOPE("VulkanBuffer::import_dma_buf");
    if (fd < 0) return false;
    const uint32_t types = table.dma_buf_types(fd);
    const VkExternalMemoryBufferCreateInfo external = {VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, nullptr,
                                                       VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    const VkBufferCreateInfo info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, &external, 0u, size,
                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 0u, nullptr};
    if (made() || size == 0u || types == 0u || !table.create_buffer(info, buffer_)) {
        close(fd);
        return false;
    }
    table_ = &table;
    const VkMemoryRequirements needs = table.buffer_requirements(buffer_);
    const VkImportMemoryFdInfoKHR imported = {VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR, nullptr,
                                              VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd};
    /* The memory is the dma-buf's size: a buffer needing more cannot be bound. */
    const uint32_t type = needs.size <= size ? table.memory_type(types & needs.memoryTypeBits) : VK_MAX_MEMORY_TYPES;
    if (table.bind_buffer(buffer_, &imported, size, type, memory_)) return true;
    if (memory_ == VK_NULL_HANDLE) close(fd);
    release();
    return false;
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
