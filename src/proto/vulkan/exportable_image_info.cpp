#include "proto/vulkan/exportable_image_info.hpp"

#include "core/support/alignment.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/d3d/dxgi_format.hpp"

namespace av {

namespace {

/* D3D11_BIND_FLAG values. */
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

}  // namespace

ExportableImageInfo::ExportableImageInfo(uint32_t width, uint32_t height, uint32_t mips, uint32_t layers,
                                         uint32_t samples, uint32_t format, uint32_t bind_flags)
    : external_{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO, nullptr,
                VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT},
      list_{VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO, nullptr, 0u, views_}, views_{},
      plane_{}, layout_{VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT, nullptr,
                        drm_format_mod_linear, 1u, &plane_},
      tilings_{VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT, nullptr, 0u, nullptr},
      info_{}, supported_(false)
{
    AV_SCOPE("ExportableImageInfo::ExportableImageInfo");
    const DxgiFormat shared(format);
    if (shared.vk_format() == 0u || width == 0u || height == 0u || mips != 1u || layers != 1u || samples > 1u ||
        (bind_flags & ~shared_binds) != 0u ||
        !Alignment(row_alignment).align_up(static_cast<uint64_t>(width) * shared.element_bytes(), plane_.rowPitch))
        return;
    info_.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info_.pNext = &external_;
    info_.imageType = VK_IMAGE_TYPE_2D;
    info_.format = static_cast<VkFormat>(shared.vk_format());
    info_.extent = {width, height, 1u};
    info_.mipLevels = 1u;
    info_.arrayLayers = 1u;
    info_.samples = VK_SAMPLE_COUNT_1_BIT;
    /* Laid out by a modifier, which names the layout to every importer: the
     * memory goes to other APIs and to processes outside QEMU (a window
     * mirror's import, docs/external-export.md), and an optimal image's tiling
     * travels with its memory on some kernels' buffers (amdgpu) and on others
     * not at all (i915, Xe), where the importer read it as linear and showed
     * it scrambled. A modifier rather than VK_IMAGE_TILING_LINEAR for rows
     * too: a driver may export a dma-buf only for an image laid out by a
     * modifier - RADV from Mesa 26.2 refuses the other. Rows are placed here
     * rather than left to the driver: every device that makes this image then
     * lays the memory out alike by construction, and no driver picks a pitch
     * its own engines cannot draw at (row_alignment). */
    info_.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    /* Drawn and sampled whatever the binds say: another API opening the
     * memory draws into it, and a superset of what DXVK asks is an image DXVK
     * can wrap. */
    info_.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info_.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info_.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    external_.pNext = &layout_;
    if (shared.vk_sibling() != 0u) {
        views_[0] = static_cast<VkFormat>(shared.vk_format());
        views_[1] = static_cast<VkFormat>(shared.vk_sibling());
        list_.viewFormatCount = max_views;
        list_.pNext = &layout_;
        external_.pNext = &list_;
        info_.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    }
    supported_ = true;
}

/* The rows' description with its last link - the explicit linear layout -
 * replaced by the list the driver picks from. */
ExportableImageInfo::ExportableImageInfo(uint32_t width, uint32_t height, uint32_t mips, uint32_t layers,
                                         uint32_t samples, uint32_t format, uint32_t bind_flags,
                                         const uint64_t *modifiers, uint32_t count)
    : ExportableImageInfo(width, height, mips, layers, samples, format, bind_flags)
{
    AV_SCOPE("ExportableImageInfo::ExportableImageInfo");
    tilings_.drmFormatModifierCount = count;
    tilings_.pDrmFormatModifiers = modifiers;
    if (external_.pNext == &layout_) external_.pNext = &tilings_;
    if (list_.pNext == &layout_) list_.pNext = &tilings_;
    supported_ = supported_ && modifiers != nullptr && count != 0u;
}

ExportableImageInfo::ExportableImageInfo(uint32_t width, uint32_t height, uint32_t mips, uint32_t layers,
                                         uint32_t samples, uint32_t format, uint32_t bind_flags, uint64_t modifier,
                                         uint64_t offset, uint64_t pitch)
    : ExportableImageInfo(width, height, mips, layers, samples, format, bind_flags)
{
    AV_SCOPE("ExportableImageInfo::ExportableImageInfo");
    layout_.drmFormatModifier = modifier;
    plane_.offset = offset;
    plane_.rowPitch = pitch;
    supported_ = supported_ && pitch != 0u;
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
