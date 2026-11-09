#include "icd/vulkan/chained_image.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/common/slot_format.hpp"
#include "icd/common/window_chains.hpp"
#include "icd/vulkan/entry_lookup.hpp"
#include "icd/vulkan/handle_bits.hpp"
#include "proto/vulkan/exportable_image_info.hpp"

namespace av::icd {

namespace {

using CreateImage = VkResult(VKAPI_PTR *)(VkDevice, const VkImageCreateInfo *, const VkAllocationCallbacks *, VkImage *);
using DestroyImage = void(VKAPI_PTR *)(VkDevice, VkImage, const VkAllocationCallbacks *);
using BindImage = VkResult(VKAPI_PTR *)(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize);

}

ChainedImage::ChainedImage(EntryLookup &entries, VkDevice device, uint64_t host_device, WindowChains &chains,
                           ClientTransport &wire)
    : entries_(entries), device_(device), host_device_(host_device), chains_(chains), slots_(), wire_(wire), images_{}
{
    AV_SCOPE("ChainedImage::ChainedImage");
}

ChainedImage::~ChainedImage()
{
    AV_SCOPE("ChainedImage::~ChainedImage");
    release();
}

bool ChainedImage::open(void *window, uint64_t luid, VkFormat format, VkExtent2D extent, uint32_t count)
{
    AV_SCOPE("ChainedImage::open");
    if (slots_.opened() || window == nullptr) return false;
    SlotFormat slot(SlotLayout::bgra_top_down);
    if (!slot.take_vk(static_cast<uint32_t>(format))) return false;
    if (!slots_.open(wire_, chains_, window, luid, extent.width, extent.height, slot.layout(), count, host_device_)) return false;
    bool made = true;
    for (uint32_t index = 0u; made && index < count; index++) made = make_image(slot, extent, index);
    if (made) return true;
    close();
    return false;
}

bool ChainedImage::make_image(const SlotFormat &slot, VkExtent2D extent, uint32_t index)
{
    AV_SCOPE("ChainedImage::make_image");
    const VkDeviceMemory memory = HandleBits(slots_.memory(index)).as<VkDeviceMemory>();
    const ExportableImageInfo described(extent.width, extent.height, 1u, 1u, 1u, slot.dxgi(), slot.bind_flags());
    const auto create = reinterpret_cast<CreateImage>(entries_.entry("vkCreateImage"));
    if (create(device_, &described.info(), nullptr, &images_[index]) != VK_SUCCESS) {
        images_[index] = VK_NULL_HANDLE;
        return false;
    }
    const auto bind = reinterpret_cast<BindImage>(entries_.entry("vkBindImageMemory"));
    return bind(device_, images_[index], memory, 0u) == VK_SUCCESS;
}

bool ChainedImage::opened() const
{
    AV_SCOPE("ChainedImage::opened");
    return slots_.opened() && images_[0] != VK_NULL_HANDLE;
}

VkImage ChainedImage::slot(uint32_t index) const
{
    AV_SCOPE("ChainedImage::slot");
    return index < ChainSlots::max_slots ? images_[index] : VK_NULL_HANDLE;
}

void ChainedImage::release()
{
    AV_SCOPE("ChainedImage::release");
    destroy_images();
    slots_.release(wire_);
}

void ChainedImage::destroy_images()
{
    AV_SCOPE("ChainedImage::destroy_images");
    for (VkImage &image : images_) {
        if (image != VK_NULL_HANDLE)
            reinterpret_cast<DestroyImage>(entries_.entry("vkDestroyImage"))(device_, image, nullptr);
        image = VK_NULL_HANDLE;
    }
}

void ChainedImage::close()
{
    AV_SCOPE("ChainedImage::close");
    release();
    slots_.close();
}

bool ChainedImage::invariants_hold() const
{
    AV_SCOPE("ChainedImage::invariants_hold");
    for (uint32_t index = 0u; index < ChainSlots::max_slots; index++)
        if (images_[index] != VK_NULL_HANDLE && slots_.memory(index) == 0u) return false;
    return slots_.invariants_hold();
}

}
