#include "proto/vulkan/exportable_image_info.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

/* DXGI_FORMAT and D3D11_BIND_FLAG values. */
constexpr uint32_t dxgi_r16g16b16a16_float = 10u;
constexpr uint32_t dxgi_r10g10b10a2_unorm = 24u;
constexpr uint32_t dxgi_r8g8b8a8_unorm = 28u;
constexpr uint32_t dxgi_r8g8b8a8_unorm_srgb = 29u;
constexpr uint32_t dxgi_b8g8r8a8_unorm = 87u;
constexpr uint32_t dxgi_b8g8r8a8_unorm_srgb = 91u;
constexpr uint32_t bind_shader_resource = 0x8u;
constexpr uint32_t bind_render_target = 0x20u;
constexpr uint32_t shared_binds = bind_shader_resource | bind_render_target;

/* A format, and its sRGB or linear sibling; VK_FORMAT_UNDEFINED for none. */
struct ViewPair {
    uint32_t dxgi;
    VkFormat format;
    VkFormat sibling;
};

constexpr ViewPair pairs[] = {
    {dxgi_r8g8b8a8_unorm, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB},
    {dxgi_r8g8b8a8_unorm_srgb, VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM},
    {dxgi_b8g8r8a8_unorm, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB},
    {dxgi_b8g8r8a8_unorm_srgb, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM},
    {dxgi_r10g10b10a2_unorm, VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_UNDEFINED},
    {dxgi_r16g16b16a16_float, VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_UNDEFINED},
};

}  // namespace

ExportableImageInfo::ExportableImageInfo(uint32_t width, uint32_t height, uint32_t mips, uint32_t layers,
                                         uint32_t samples, uint32_t format, uint32_t bind_flags)
    : external_{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO, nullptr,
                VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT},
      list_{VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO, nullptr, 0u, views_}, views_{}, info_{},
      supported_(false)
{
    AV_SCOPE("ExportableImageInfo::ExportableImageInfo");
    const ViewPair *pair = nullptr;
    for (const ViewPair &candidate : pairs)
        if (candidate.dxgi == format) pair = &candidate;
    if (pair == nullptr || width == 0u || height == 0u || mips != 1u || layers != 1u || samples > 1u ||
        (bind_flags & ~shared_binds) != 0u)
        return;
    info_.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info_.pNext = &external_;
    info_.imageType = VK_IMAGE_TYPE_2D;
    info_.format = pair->format;
    info_.extent = {width, height, 1u};
    info_.mipLevels = 1u;
    info_.arrayLayers = 1u;
    info_.samples = VK_SAMPLE_COUNT_1_BIT;
    /* Linear: the memory goes to processes outside QEMU (a window mirror's
     * import, docs/external-export.md), and only a linear layout is one every
     * driver reads the same with no modifier to agree on. An optimal image's
     * tiling travels with its memory on some kernels' buffers (amdgpu) and on
     * others not at all (i915, Xe), where the importer read it as linear and
     * showed it scrambled. */
    info_.tiling = VK_IMAGE_TILING_LINEAR;
    /* Drawn and sampled whatever the binds say: another API opening the
     * memory draws into it, and a superset of what DXVK asks is an image DXVK
     * can wrap. */
    info_.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info_.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info_.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (pair->sibling != VK_FORMAT_UNDEFINED) {
        views_[0] = pair->format;
        views_[1] = pair->sibling;
        list_.viewFormatCount = max_views;
        external_.pNext = &list_;
        info_.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    }
    supported_ = true;
}

bool ExportableImageInfo::supported() const
{
    AV_SCOPE("ExportableImageInfo::supported");
    return supported_;
}

const VkImageCreateInfo &ExportableImageInfo::info() const
{
    AV_SCOPE("ExportableImageInfo::info");
    return info_;
}

}  // namespace av
