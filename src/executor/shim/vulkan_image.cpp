#include "executor/shim/vulkan_image.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/vulkan_device_table.hpp"

#include <unistd.h>

namespace av {

VulkanImage::VulkanImage() : table_(nullptr), image_(VK_NULL_HANDLE), memory_(VK_NULL_HANDLE), size_(0u)
{
    AV_SCOPE("VulkanImage::VulkanImage");
}

VulkanImage::~VulkanImage()
{
    AV_SCOPE("VulkanImage::~VulkanImage");
    release();
}

bool VulkanImage::make_exportable(const VulkanDeviceTable &table, const VkImageCreateInfo &info)
{
    AV_SCOPE("VulkanImage::make_exportable");
    const VkExportMemoryAllocateInfo exported = {VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO, nullptr,
                                                 VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    bool took = false;
    return bind(table, info, &exported, took);
}

bool VulkanImage::import_dma_buf(const VulkanDeviceTable &table, const VkImageCreateInfo &info, int fd)
{
    AV_SCOPE("VulkanImage::import_dma_buf");
    if (fd < 0) return false;
    const VkImportMemoryFdInfoKHR imported = {VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR, nullptr,
                                              VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd};
    bool took = false;
    if (bind(table, info, &imported, took)) return true;
    if (!took) close(fd);
    return false;
}

bool VulkanImage::bind(const VulkanDeviceTable &table, const VkImageCreateInfo &info, const void *chain, bool &took)
{
    AV_SCOPE("VulkanImage::bind");
    took = false;
    if (made() || !table.create_image(info, image_)) return false;
    table_ = &table;
    const VkMemoryRequirements needs = table.requirements(image_);
    if (!table.bind_dedicated(image_, chain, needs.size, table.memory_type(needs.memoryTypeBits), memory_)) {
        took = memory_ != VK_NULL_HANDLE;
        release();
        return false;
    }
    size_ = needs.size;
    return true;
}

bool VulkanImage::export_dma_buf(int &fd) const
{
    AV_SCOPE("VulkanImage::export_dma_buf");
    fd = -1;
    return table_ != nullptr && table_->memory_fd(memory_, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd);
}

void VulkanImage::release()
{
    AV_SCOPE("VulkanImage::release");
    if (table_ != nullptr) {
        table_->destroy_image(image_);
        table_->free_memory(memory_);
    }
    table_ = nullptr;
    image_ = VK_NULL_HANDLE;
    memory_ = VK_NULL_HANDLE;
    size_ = 0u;
}

bool VulkanImage::made() const
{
    AV_SCOPE("VulkanImage::made");
    return image_ != VK_NULL_HANDLE;
}

VkImage VulkanImage::image() const
{
    AV_SCOPE("VulkanImage::image");
    return image_;
}

VkDeviceMemory VulkanImage::memory() const
{
    AV_SCOPE("VulkanImage::memory");
    return memory_;
}

VkDeviceSize VulkanImage::size() const
{
    AV_SCOPE("VulkanImage::size");
    return size_;
}

}  // namespace av
