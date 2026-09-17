/* What a surface offers, from its window (feature 046, research.md §2): the
 * capabilities, formats and present modes the ICD's present path implements,
 * and nothing else (constitution VI). */
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
                                               VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    SurfaceProperties(WindowSurface &windows, void *window);
    SurfaceProperties(const SurfaceProperties &) = delete;
    SurfaceProperties &operator=(const SurfaceProperties &) = delete;

    /* VK_ERROR_SURFACE_LOST_KHR for a window that is gone. The current
     * extent is the client area, 0x0 for a minimized window. */
    VkResult capabilities(VkSurfaceCapabilitiesKHR &answer);
    /* The count/array protocol over the four formats; `formats2` takes the
     * VkSurfaceFormat2KHR spelling instead when `formats` is null. */
    VkResult formats(uint32_t &count, VkSurfaceFormatKHR *formats);
    VkResult formats2(uint32_t &count, VkSurfaceFormat2KHR *formats);
    VkResult present_modes(uint32_t &count, VkPresentModeKHR *modes);
    /* Whether a swapchain may use `format` in the colour space `space`, and
     * whether it may present in `mode`. The three are an application's
     * enumeration values, so each is the integer it is (foreign_enum.hpp). */
    bool     offers_format(int32_t format, int32_t space) const;
    bool     offers_mode(int32_t mode) const;

private:
    uint32_t fits(uint32_t &count, uint32_t available) const;

    WindowSurface &windows_;
    void          *window_;
};

}  // namespace av::icd

#endif  // AV_SURFACE_PROPERTIES_HPP
