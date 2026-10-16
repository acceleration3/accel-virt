/* The Vulkan image a shared Direct3D 11 texture is made as (ExportableImages):
 * at least what DXVK would make for the texture's description, over memory
 * exported as a dma-buf, laid out by a DRM format modifier so that whoever
 * imports the memory can be told the layout.
 *
 * - Rows: linear, with its rows a pitch apart this description sets - for a
 *   texture another API reads as rows (ExportableImages::make_rows), and the
 *   image the Vulkan ICD makes over that memory once it has opened it
 *   (ChainedImage). One description for both sides, because a driver lays an
 *   image out from its description, and two that differ are two layouts of
 *   one memory.
 * - Tiled: by whichever of the modifiers it is given the driver picks - the
 *   GPU's own tilings, which it draws and samples several times faster than
 *   rows (ExportableImages::make_tiled). Only its maker knows the layout, and
 *   tells it with the memory (AvExportedImage).
 *
 * Only what interop shares is described - one 2D mip, one layer and one
 * sample of a colour format DxgiFormat gives a Vulkan format, drawn and
 * sampled. Anything else is not
 * supported(), and DXVK makes it as it would any texture, with memory nothing
 * else can open. The image is the memory's alone (ExportableImages binds it
 * dedicated), which is what lets another driver read its layout. A format
 * with an sRGB sibling is made mutable between the two, which is all the views
 * such a texture is ever given. */
#ifndef AV_EXPORTABLE_IMAGE_INFO_HPP
#define AV_EXPORTABLE_IMAGE_INFO_HPP

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class ExportableImageInfo {
public:
    /* What D3D11_TEXTURE2D_DESC1 holds, as DXGI and D3D11 number it, laid
     * out as rows. */
    ExportableImageInfo(uint32_t width, uint32_t height, uint32_t mips, uint32_t layers, uint32_t samples,
                        uint32_t format, uint32_t bind_flags);
    /* The same texture laid out by one of the `count` `modifiers`, which the
     * driver picks; none is not supported(). The modifiers are read where
     * they are, so they outlive this. */
    ExportableImageInfo(uint32_t width, uint32_t height, uint32_t mips, uint32_t layers, uint32_t samples,
                        uint32_t format, uint32_t bind_flags, const uint64_t *modifiers, uint32_t count);
    /* The same texture as an image made by one of the others laid its memory
     * out: by `modifier`, its one plane at `offset` with rows `pitch` apart -
     * what an importer of that memory is told. */
    ExportableImageInfo(uint32_t width, uint32_t height, uint32_t mips, uint32_t layers, uint32_t samples,
                        uint32_t format, uint32_t bind_flags, uint64_t modifier, uint64_t offset, uint64_t pitch);
    ExportableImageInfo(const ExportableImageInfo &) = delete;
    ExportableImageInfo &operator=(const ExportableImageInfo &) = delete;

    bool                     supported() const;
    /* The image to create; its chain points into this object. */
    const VkImageCreateInfo &info() const;

private:
    static constexpr uint32_t max_views = 2u;

    VkExternalMemoryImageCreateInfo               external_;
    VkImageFormatListCreateInfo                   list_;
    VkFormat                                      views_[max_views];
    VkSubresourceLayout                           plane_;
    VkImageDrmFormatModifierExplicitCreateInfoEXT layout_;
    VkImageDrmFormatModifierListCreateInfoEXT     tilings_;
    VkImageCreateInfo                             info_;
    bool                                          supported_;
};

}  // namespace av

#endif  // AV_EXPORTABLE_IMAGE_INFO_HPP
