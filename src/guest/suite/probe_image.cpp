#include "guest/suite/probe_image.hpp"

#include "guest/suite/probe_device.hpp"
#include "guest/suite/probe_outcome.hpp"
#include "guest/suite/vulkan_entry.hpp"

namespace av {

ProbeImage::ProbeImage(ProbeDevice &device)
    : device_(device), image_(VK_NULL_HANDLE), memory_(VK_NULL_HANDLE), view_(VK_NULL_HANDLE) {}

/* Vulkan makes destroying and freeing VK_NULL_HANDLE a no-op. */
ProbeImage::~ProbeImage()
{
    device_.vulkan().destroy_image_view(device_.device(), view_);
    device_.vulkan().destroy_image(device_.device(), image_);
    device_.vulkan().free_memory(device_.device(), memory_);
}

bool ProbeImage::make(uint32_t extent, ProbeOutcome &outcome)
{
    VulkanEntry   &vulkan = device_.vulkan();
    const VkDevice device = device_.device();
    VkImageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.extent = VkExtent3D{extent, extent, 1u};
    info.mipLevels = 1u;
    info.arrayLayers = 1u;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!outcome.accept(vulkan.create_image(device, &info, &image_))) return false;
    VkMemoryRequirements requirements = {};
    vulkan.image_memory_requirements(device, image_, &requirements);
    uint32_t type = 0u;
    if (!choose_type(requirements.memoryTypeBits, type)) {
        outcome.missing();
        return false;
    }
    VkMemoryAllocateInfo allocation = {};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = type;
    if (!outcome.accept(vulkan.allocate_memory(device, &allocation, &memory_))) return false;
    if (!outcome.accept(vulkan.bind_image_memory(device, image_, memory_))) return false;
    return make_view(outcome);
}

bool ProbeImage::choose_type(uint32_t allowed, uint32_t &type) const
{
    return device_.memory_type(allowed, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, type) ||
           device_.memory_type(allowed, 0u, type);
}

bool ProbeImage::make_view(ProbeOutcome &outcome)
{
    VkImageViewCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image = image_;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    info.subresourceRange.levelCount = 1u;
    info.subresourceRange.layerCount = 1u;
    return outcome.accept(device_.vulkan().create_image_view(device_.device(), &info, &view_));
}

VkImage     ProbeImage::image() const { return image_; }
VkImageView ProbeImage::view() const { return view_; }

bool ProbeImage::invariants_hold() const
{
    if (view_ != VK_NULL_HANDLE && memory_ == VK_NULL_HANDLE) return false;
    return memory_ == VK_NULL_HANDLE || image_ != VK_NULL_HANDLE;
}

}  // namespace av
