/* The Vulkan image a Direct3D 11 texture is made as when another API must open
 * its memory (ExportableImages::make): at least what DXVK would make for the
 * texture's description, over memory exported as a dma-buf, laid out linear
 * with its rows a pitch apart this description sets - and the image the Vulkan
 * ICD makes over that memory once it has opened it (ChainedImage). One
 * description for both sides, because a driver lays an image out from its
 * description, and two that differ are two layouts of one memory.
 *
 * Only what interop shares is described - one 2D mip, one layer and one
 * sample of a colour format below, drawn and sampled. Anything else is not
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
    /* What D3D11_TEXTURE2D_DESC1 holds, as DXGI and D3D11 number it. */
    ExportableImageInfo(uint32_t width, uint32_t height, uint32_t mips, uint32_t layers, uint32_t samples,
                        uint32_t format, uint32_t bind_flags);
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
    VkImageCreateInfo                             info_;
    bool                                          supported_;
};

}  // namespace av

#endif  // AV_EXPORTABLE_IMAGE_INFO_HPP
