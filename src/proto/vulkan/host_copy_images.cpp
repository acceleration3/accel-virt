#include "proto/vulkan/host_copy_images.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/generated/vulkan/vk_format_layout.h"

namespace av {

namespace {

constexpr VkImageUsageFlags host_transfer_usage = 0x00400000u;

uint64_t blocks_of(uint64_t texels, uint64_t block)
{
    AV_SCOPE("blocks_of");
    return (texels + block - 1u) / block;
}

}

HostCopyImages::HostCopyImages(BlockSource &blocks) : slots_(blocks, Pool::paged)
{
    AV_SCOPE("HostCopyImages::HostCopyImages");
}

bool HostCopyImages::record(uint64_t image, uint64_t device, const VkImageCreateInfo &create)
{
    AV_SCOPE("HostCopyImages::record");
    if (image == 0u) return false;
    if ((create.usage & host_transfer_usage) == 0u) return true;
    const Entry entry{device, create.format, create.arrayLayers};
    Entry *const known = slots_.find(image);
    if (known != nullptr) {
        *known = entry;
        return true;
    }
    uint32_t slot = 0u;
    return slots_.insert(image, entry, slot) == Status::ok;
}

void HostCopyImages::forget(uint64_t image)
{
    AV_SCOPE("HostCopyImages::forget");
    static_cast<void>(slots_.erase(image));
}

void HostCopyImages::forget_device(uint64_t device)
{
    AV_SCOPE("HostCopyImages::forget_device");
    slots_.erase_if([device](uint64_t, Entry &entry) { return entry.device_ == device; });
}

bool HostCopyImages::bytes(uint64_t image, const VkImageSubresourceLayers &layers, VkExtent3D extent,
                           uint32_t row_length, uint32_t image_height, uint64_t &out) const
{
    AV_SCOPE("HostCopyImages::bytes");
    out = 0u;
    const Entry *const entry = slots_.find(image);
    if (entry == nullptr) return false;
    const VkImageAspectFlags aspect = layers.aspectMask;
    if (aspect != VK_IMAGE_ASPECT_COLOR_BIT && aspect != VK_IMAGE_ASPECT_DEPTH_BIT && aspect != VK_IMAGE_ASPECT_STENCIL_BIT)
        return false;
    vkproto::FormatLayout layout = {};
    if (!vkproto::format_layout(entry->format_, static_cast<VkImageAspectFlagBits>(aspect), layout)) return false;
    const uint64_t remaining = entry->array_layers_ > layers.baseArrayLayer ? entry->array_layers_ - layers.baseArrayLayer : 0u;
    const uint64_t layer_count = layers.layerCount == VK_REMAINING_ARRAY_LAYERS ? remaining : layers.layerCount;
    if (layer_count == 0u || layer_count > remaining) return false;
    if (extent.width == 0u || extent.height == 0u || extent.depth == 0u) return true;
    const uint64_t row = row_length != 0u ? row_length : extent.width;
    const uint64_t height = image_height != 0u ? image_height : extent.height;
    if (row < extent.width || height < extent.height) return false;
    const uint64_t row_stride = blocks_of(row, layout.block_width) * layout.block_bytes;
    const uint64_t slice_stride = row_stride * blocks_of(height, layout.block_height);
    const uint64_t slices = layer_count * extent.depth;
    const uint64_t last_rows = blocks_of(extent.height, layout.block_height);
    const uint64_t last_row = blocks_of(extent.width, layout.block_width) * layout.block_bytes;
    uint64_t       total = 0u;
    if (__builtin_mul_overflow(slices - 1u, slice_stride, &total)) return false;
    total += (last_rows - 1u) * row_stride + last_row;
    if (total > most_bytes) return false;
    out = total;
    return true;
}

uint32_t HostCopyImages::live() const
{
    AV_SCOPE("HostCopyImages::live");
    return slots_.count();
}

bool HostCopyImages::invariants_hold() const
{
    AV_SCOPE("HostCopyImages::invariants_hold");
    return slots_.invariants_hold();
}

}
