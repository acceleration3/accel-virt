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
    : entries_(entries), device_(device), host_device_(host_device), chains_(chains), slots_(), wire_(wire), image_(VK_NULL_HANDLE)
{
    AV_SCOPE("ChainedImage::ChainedImage");
}

ChainedImage::~ChainedImage()
{
    AV_SCOPE("ChainedImage::~ChainedImage");
    release();
}

bool ChainedImage::open(void *window, uint64_t luid, VkFormat format, VkExtent2D extent)
{
    AV_SCOPE("ChainedImage::open");
    if (slots_.opened() || window == nullptr) return false;
    SlotFormat slot(SlotLayout::bgra_top_down);
    if (!slot.take_vk(static_cast<uint32_t>(format))) return false;
    if (!slots_.open(wire_, chains_, window, luid, extent.width, extent.height, slot.layout(), 1u, host_device_)) return false;
    if (make_image(slot, extent)) return true;
    close();
    return false;
}

bool ChainedImage::make_image(const SlotFormat &slot, VkExtent2D extent)
{
    AV_SCOPE("ChainedImage::make_image");
    const VkDeviceMemory memory = HandleBits(slots_.memory(0u)).as<VkDeviceMemory>();
    const ExportableImageInfo described(extent.width, extent.height, 1u, 1u, 1u, slot.dxgi(), slot.bind_flags());
    const auto create = reinterpret_cast<CreateImage>(entries_.entry("vkCreateImage"));
    if (create(device_, &described.info(), nullptr, &image_) != VK_SUCCESS) {
        image_ = VK_NULL_HANDLE;
        return false;
    }
    const auto bind = reinterpret_cast<BindImage>(entries_.entry("vkBindImageMemory"));
    return bind(device_, image_, memory, 0u) == VK_SUCCESS;
}

bool ChainedImage::opened() const
{
    AV_SCOPE("ChainedImage::opened");
    return image_ != VK_NULL_HANDLE;
}

VkImage ChainedImage::slot() const
{
    AV_SCOPE("ChainedImage::slot");
    return image_;
}

void ChainedImage::release()
{
    AV_SCOPE("ChainedImage::release");
    if (image_ != VK_NULL_HANDLE)
        reinterpret_cast<DestroyImage>(entries_.entry("vkDestroyImage"))(device_, image_, nullptr);
    image_ = VK_NULL_HANDLE;
    slots_.release(wire_);
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
    if (image_ != VK_NULL_HANDLE && slots_.memory(0u) == 0u) return false;
    return slots_.invariants_hold();
}

}
