#ifndef AV_EXPORTABLE_IMAGE_INFO_HPP
#define AV_EXPORTABLE_IMAGE_INFO_HPP

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class ExportableImageInfo {
public:
    ExportableImageInfo(uint32_t width, uint32_t height, uint32_t mips, uint32_t layers, uint32_t samples,
                        uint32_t format, uint32_t bind_flags);
    ExportableImageInfo(uint32_t width, uint32_t height, uint32_t mips, uint32_t layers, uint32_t samples,
                        uint32_t format, uint32_t bind_flags, const uint64_t *modifiers, uint32_t count);
    ExportableImageInfo(uint32_t width, uint32_t height, uint32_t mips, uint32_t layers, uint32_t samples,
                        uint32_t format, uint32_t bind_flags, uint64_t modifier, uint64_t offset, uint64_t pitch);
    ExportableImageInfo(const ExportableImageInfo &) = delete;
    ExportableImageInfo &operator=(const ExportableImageInfo &) = delete;

    bool                     supported() const;
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

}

#endif
