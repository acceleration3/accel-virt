#include "executor/shim/exportable_image.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/dxvk_vulkan_device.hpp"

namespace av {

ExportableImage::ExportableImage(ExportableImages &owner, VkImage image, VkDeviceMemory memory, uint64_t modifier)
    : TextureHold(owner), image_(image), memory_(memory), modifier_(modifier)
{
    AV_SCOPE("ExportableImage::ExportableImage");
}

VkImage ExportableImage::image() const
{
    AV_SCOPE("ExportableImage::image");
    return image_;
}

VkDeviceMemory ExportableImage::memory() const
{
    AV_SCOPE("ExportableImage::memory");
    return memory_;
}

uint64_t ExportableImage::modifier() const
{
    AV_SCOPE("ExportableImage::modifier");
    return modifier_;
}

bool ExportableImage::rows() const
{
    AV_SCOPE("ExportableImage::rows");
    constexpr uint64_t drm_format_mod_linear = 0u;
    return modifier_ == drm_format_mod_linear;
}

void ExportableImage::free(const DxvkVulkanDevice &vulkan)
{
    AV_SCOPE("ExportableImage::free");
    vulkan.table().destroy_image(image_);
    vulkan.table().free_memory(memory_);
    image_ = VK_NULL_HANDLE;
    memory_ = VK_NULL_HANDLE;
}

}  // namespace av
