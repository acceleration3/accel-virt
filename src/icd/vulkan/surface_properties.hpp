#ifndef AV_SURFACE_PROPERTIES_HPP
#define AV_SURFACE_PROPERTIES_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av::icd {

class WindowSurface;

class SurfaceProperties {
public:
    static constexpr uint32_t min_images = 2u;
    static constexpr uint32_t max_images = 8u;
    static constexpr uint32_t format_count = 4u;
    static constexpr uint32_t mode_count = 2u;
    static constexpr VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                               VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                               VK_IMAGE_USAGE_STORAGE_BIT;

    SurfaceProperties(WindowSurface &windows, void *window);
    SurfaceProperties(const SurfaceProperties &) = delete;
    SurfaceProperties &operator=(const SurfaceProperties &) = delete;

    VkResult capabilities(VkSurfaceCapabilitiesKHR &answer);
    VkResult formats(uint32_t &count, VkSurfaceFormatKHR *formats);
    VkResult formats2(uint32_t &count, VkSurfaceFormat2KHR *formats);
    VkResult present_modes(uint32_t &count, VkPresentModeKHR *modes);
    bool     offers_format(int32_t format, int32_t space) const;
    bool     offers_mode(int32_t mode) const;

private:
    uint32_t fits(uint32_t &count, uint32_t available) const;

    WindowSurface &windows_;
    void          *window_;
    VkFormat       formats_[format_count];
};

}

#endif
