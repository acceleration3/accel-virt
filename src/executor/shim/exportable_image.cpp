#include "executor/shim/exportable_image.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/dxvk_vulkan_device.hpp"

namespace av {

ExportableImage::ExportableImage(ExportableImages &owner, VkImage image, VkDeviceMemory memory)
    : TextureHold(owner), image_(image), memory_(memory)
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

void ExportableImage::free(const DxvkVulkanDevice &vulkan)
{
    AV_SCOPE("ExportableImage::free");
    vulkan.destroy_image(image_);
    vulkan.free_memory(memory_);
    image_ = VK_NULL_HANDLE;
    memory_ = VK_NULL_HANDLE;
}

}  // namespace av
