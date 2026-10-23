#include "executor/shim/vulkan_image.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/owned_fd.hpp"
#include "executor/shim/vulkan_device_table.hpp"

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

bool VulkanImage::create(const VulkanDeviceTable &table, const VkImageCreateInfo &info)
{
    AV_SCOPE("VulkanImage::create");
    if (made() || !table.create_image(info, image_)) return false;
    table_ = &table;
    return true;
}

VkMemoryRequirements VulkanImage::requirements() const
{
    AV_SCOPE("VulkanImage::requirements");
    return table_ != nullptr ? table_->requirements(image_) : VkMemoryRequirements{};
}

bool VulkanImage::bind_exportable(uint32_t type)
{
    AV_SCOPE("VulkanImage::bind_exportable");
    const VkDeviceSize size = requirements().size;
    if (table_ == nullptr || memory_ != VK_NULL_HANDLE || !table_->allocate_exportable(image_, size, type, memory_)) {
        release();
        return false;
    }
    return bound(size);
}

bool VulkanImage::bind_imported(OwnedFd &descriptor, VkExternalMemoryHandleTypeFlagBits handle, VkDeviceSize size,
                                uint32_t type)
{
    AV_SCOPE("VulkanImage::bind_imported");
    const VkMemoryDedicatedAllocateInfo dedicated = {VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, nullptr, image_,
                                                     VK_NULL_HANDLE};
    if (table_ == nullptr || memory_ != VK_NULL_HANDLE ||
        !table_->import_memory(descriptor, handle, size, type, &dedicated, memory_)) {
        release();
        return false;
    }
    return bound(size);
}

bool VulkanImage::bound(VkDeviceSize size)
{
    AV_SCOPE("VulkanImage::bound");
    if (!table_->bind_image(image_, memory_)) {
        release();
        return false;
    }
    size_ = size;
    return true;
}

bool VulkanImage::make_exportable(const VulkanDeviceTable &table, const VkImageCreateInfo &info)
{
    AV_SCOPE("VulkanImage::make_exportable");
    return create(table, info) && bind_exportable(table.memory_type(requirements().memoryTypeBits));
}

bool VulkanImage::import_dma_buf(const VulkanDeviceTable &table, const VkImageCreateInfo &info, OwnedFd &descriptor)
{
    AV_SCOPE("VulkanImage::import_dma_buf");
    if (!create(table, info)) return false;
    const VkMemoryRequirements needs = requirements();
    /* The types this device would import the dma-buf as, where it can say:
     * memory another GPU made is not importable as every type the image fits. */
    const uint32_t importable = table.dma_buf_types(descriptor.fd());
    const uint32_t allowed = needs.memoryTypeBits & (importable != 0u ? importable : ~0u);
    return bind_imported(descriptor, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, needs.size,
                         table.memory_type(allowed));
}

bool VulkanImage::export_dma_buf(OwnedFd &descriptor) const
{
    AV_SCOPE("VulkanImage::export_dma_buf");
    int fd = -1;
    const bool exported =
        table_ != nullptr && table_->memory_fd(memory_, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, fd);
    descriptor.hold(fd);
    return exported;
}

uint64_t VulkanImage::modifier() const
{
    AV_SCOPE("VulkanImage::modifier");
    constexpr uint64_t drm_format_mod_invalid = 0x00ffffffffffffffull;
    return table_ != nullptr ? table_->modifier_of(image_) : drm_format_mod_invalid;
}

VkSubresourceLayout VulkanImage::row_layout() const
{
    AV_SCOPE("VulkanImage::row_layout");
    return table_ != nullptr ? table_->row_layout(image_) : VkSubresourceLayout{};
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
