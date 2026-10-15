#include "proto/vulkan/exportable_image_info.hpp"

#include "core/support/alignment.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

namespace {

/* DXGI_FORMAT and D3D11_BIND_FLAG values. */
constexpr uint32_t dxgi_r32g32b32a32_float = 2u;
constexpr uint32_t dxgi_r16g16b16a16_float = 10u;
constexpr uint32_t dxgi_r16g16b16a16_unorm = 11u;
constexpr uint32_t dxgi_r32g32_float = 16u;
constexpr uint32_t dxgi_r16g16_float = 34u;
constexpr uint32_t dxgi_r32_float = 41u;
constexpr uint32_t dxgi_r8g8_unorm = 49u;
constexpr uint32_t dxgi_r16_float = 54u;
constexpr uint32_t dxgi_r8_unorm = 61u;
constexpr uint32_t dxgi_r10g10b10a2_unorm = 24u;
constexpr uint32_t dxgi_r8g8b8a8_unorm = 28u;
constexpr uint32_t dxgi_r8g8b8a8_unorm_srgb = 29u;
constexpr uint32_t dxgi_b8g8r8a8_unorm = 87u;
constexpr uint32_t dxgi_b8g8r8a8_unorm_srgb = 91u;
constexpr uint32_t bind_shader_resource = 0x8u;
constexpr uint32_t bind_render_target = 0x20u;
constexpr uint32_t shared_binds = bind_shader_resource | bind_render_target;
/* DRM_FORMAT_MOD_LINEAR, from drm_fourcc.h. */
constexpr uint64_t drm_format_mod_linear = 0u;
/* The bytes between the starts of two rows are a multiple of this. NVIDIA
 * (580) lays a linear image's rows only 32 bytes apart - 2464 for 614 BGRA
 * texels - and its 3D engine faults on clearing a target whose rows are not
 * 128-byte aligned (Xid 69, CLEAR_SURFACE, ErrorCode 0xf): the device is lost,
 * and with it every Direct3D 11 session on the GPU. 256 is what RADV lays
 * linear rows at itself, and RADV, ANV and NVIDIA all take it. */
constexpr uint32_t row_alignment = 256u;

/* A format, its sRGB or linear sibling (VK_FORMAT_UNDEFINED for none), and
 * the bytes of one texel. */
struct ViewPair {
    uint32_t dxgi;
    VkFormat format;
    VkFormat sibling;
    uint32_t texel_bytes;
};

constexpr ViewPair pairs[] = {
    {dxgi_r8g8b8a8_unorm, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB, 4u},
    {dxgi_r8g8b8a8_unorm_srgb, VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM, 4u},
    {dxgi_b8g8r8a8_unorm, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB, 4u},
    {dxgi_b8g8r8a8_unorm_srgb, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM, 4u},
    {dxgi_r10g10b10a2_unorm, VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_UNDEFINED, 4u},
    {dxgi_r16g16b16a16_float, VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_UNDEFINED, 8u},
    /* What an OpenCL image over a shared texture reads as itself
     * (DxgiClFormat): video processing's float and single-channel planes. */
    {dxgi_r32g32b32a32_float, VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_UNDEFINED, 16u},
    {dxgi_r16g16b16a16_unorm, VK_FORMAT_R16G16B16A16_UNORM, VK_FORMAT_UNDEFINED, 8u},
    {dxgi_r32g32_float, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_UNDEFINED, 8u},
    {dxgi_r16g16_float, VK_FORMAT_R16G16_SFLOAT, VK_FORMAT_UNDEFINED, 4u},
    {dxgi_r32_float, VK_FORMAT_R32_SFLOAT, VK_FORMAT_UNDEFINED, 4u},
    {dxgi_r8g8_unorm, VK_FORMAT_R8G8_UNORM, VK_FORMAT_UNDEFINED, 2u},
    {dxgi_r16_float, VK_FORMAT_R16_SFLOAT, VK_FORMAT_UNDEFINED, 2u},
    {dxgi_r8_unorm, VK_FORMAT_R8_UNORM, VK_FORMAT_UNDEFINED, 1u},
};

}  // namespace

ExportableImageInfo::ExportableImageInfo(uint32_t width, uint32_t height, uint32_t mips, uint32_t layers,
                                         uint32_t samples, uint32_t format, uint32_t bind_flags)
    : external_{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO, nullptr,
                VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT},
      list_{VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO, nullptr, 0u, views_}, views_{},
      plane_{}, layout_{VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT, nullptr,
                        drm_format_mod_linear, 1u, &plane_},
      info_{}, supported_(false)
{
    AV_SCOPE("ExportableImageInfo::ExportableImageInfo");
    const ViewPair *pair = nullptr;
    for (const ViewPair &candidate : pairs)
        if (candidate.dxgi == format) pair = &candidate;
    if (pair == nullptr || width == 0u || height == 0u || mips != 1u || layers != 1u || samples > 1u ||
        (bind_flags & ~shared_binds) != 0u ||
        !Alignment(row_alignment).align_up(static_cast<uint64_t>(width) * pair->texel_bytes, plane_.rowPitch))
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
     * driver reads the same. An optimal image's tiling travels with its memory
     * on some kernels' buffers (amdgpu) and on others not at all (i915, Xe),
     * where the importer read it as linear and showed it scrambled. Linear
     * under the modifier that names it rather than VK_IMAGE_TILING_LINEAR: a
     * driver may export a dma-buf only for an image laid out by a modifier -
     * RADV from Mesa 26.2 refuses the other. The rows are placed here rather
     * than left to the driver: every device that makes this image then lays
     * the memory out alike by construction, and no driver picks a pitch its
     * own engines cannot draw at (row_alignment). */
    info_.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    /* Drawn and sampled whatever the binds say: another API opening the
     * memory draws into it, and a superset of what DXVK asks is an image DXVK
     * can wrap. */
    info_.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info_.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info_.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    external_.pNext = &layout_;
    if (pair->sibling != VK_FORMAT_UNDEFINED) {
        views_[0] = pair->format;
        views_[1] = pair->sibling;
        list_.viewFormatCount = max_views;
        list_.pNext = &layout_;
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
