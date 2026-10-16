#include "icd/vulkan/chained_image.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/common/window_chains.hpp"
#include "icd/vulkan/entry_lookup.hpp"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/vulkan/exportable_image_info.hpp"

namespace av::icd {

namespace {

using CreateImage = VkResult(VKAPI_PTR *)(VkDevice, const VkImageCreateInfo *, const VkAllocationCallbacks *, VkImage *);
using DestroyImage = void(VKAPI_PTR *)(VkDevice, VkImage, const VkAllocationCallbacks *);
using BindImage = VkResult(VKAPI_PTR *)(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize);

/* DXGI_FORMAT and D3D11_BIND_FLAG values: what the slot's texture is. */
constexpr uint32_t dxgi_r8g8b8a8_unorm = 28u;
constexpr uint32_t dxgi_b8g8r8a8_unorm = 87u;
constexpr uint32_t slot_binds = 0x8u | 0x20u;

/* A swapchain format, and the slot that holds its bytes as they are: the
 * linear format of its channel order, which an sRGB image copies into
 * unconverted. */
struct SlotFormat {
    VkFormat   format;
    uint32_t   dxgi;
    SlotLayout layout;
};

constexpr SlotFormat slot_formats[] = {
    {VK_FORMAT_B8G8R8A8_UNORM, dxgi_b8g8r8a8_unorm, SlotLayout::bgra_top_down},
    {VK_FORMAT_B8G8R8A8_SRGB, dxgi_b8g8r8a8_unorm, SlotLayout::bgra_top_down},
    {VK_FORMAT_R8G8B8A8_UNORM, dxgi_r8g8b8a8_unorm, SlotLayout::rgba_top_down},
    {VK_FORMAT_R8G8B8A8_SRGB, dxgi_r8g8b8a8_unorm, SlotLayout::rgba_top_down},
};

}  // namespace

ChainedImage::ChainedImage(EntryLookup &entries, VkDevice device, WindowChains &chains, ClientTransport &wire)
    : entries_(entries), device_(device), chains_(chains), slots_(), wire_(wire), image_(VK_NULL_HANDLE)
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
    const SlotFormat *slot = nullptr;
    for (const SlotFormat &candidate : slot_formats)
        if (candidate.format == format) slot = &candidate;
    if (slot == nullptr) return false;
    if (!slots_.open(wire_, chains_, window, luid, extent.width, extent.height, slot->layout, 1u)) return false;
    if (make_image(slot->dxgi, extent)) return true;
    close();
    return false;
}

/* The bytes are the slot's surface as the guest counts them, its rows end to
 * end; the device opened the whole of the host's memory behind them, which
 * the host's tiling of the same image fits (test_d3d_vulkan_interop_device). */
bool ChainedImage::make_image(uint32_t dxgi_format, VkExtent2D extent)
{
    AV_SCOPE("ChainedImage::make_image");
    VkDeviceMemory memory = VK_NULL_HANDLE;
    /* Not branched on: the slots refused an open that answered no handle,
     * and the conversion of a non-zero one is a copy of the same width. */
    static_cast<void>(vkproto::wire_to_handle(slots_.memory(0u), memory));
    const ExportableImageInfo described(extent.width, extent.height, 1u, 1u, 1u, dxgi_format, slot_binds);
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

bool ChainedImage::show()
{
    AV_SCOPE("ChainedImage::show");
    return opened() && slots_.present(0u);
}

/* The memory is the device's to free: releasing the object does
 * (VulkanBackend::release_object), as a vkFreeMemory of it would not. */
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

/* An image only over a slot's memory. */
bool ChainedImage::invariants_hold() const
{
    AV_SCOPE("ChainedImage::invariants_hold");
    if (image_ != VK_NULL_HANDLE && slots_.memory(0u) == 0u) return false;
    return slots_.invariants_hold();
}

}  // namespace av::icd
