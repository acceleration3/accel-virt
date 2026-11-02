#include "icd/vulkan/surface_properties.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/common/slot_format.hpp"
#include "icd/common/window_extent.hpp"
#include "icd/common/window_surface.hpp"

namespace av::icd {

namespace {

constexpr VkPresentModeKHR offered_modes[SurfaceProperties::mode_count] = {VK_PRESENT_MODE_FIFO_KHR,
                                                                           VK_PRESENT_MODE_IMMEDIATE_KHR};
constexpr uint32_t max_extent = 16384u;

}

SurfaceProperties::SurfaceProperties(WindowSurface &windows, void *window)
    : windows_(windows), window_(window), formats_{}
{
    AV_SCOPE("SurfaceProperties::SurfaceProperties");
    const SlotFormat slots[] = {SlotFormat(SlotLayout::bgra_top_down), SlotFormat(SlotLayout::rgba_top_down)};
    static_assert(sizeof slots / sizeof slots[0] * 2u == format_count, "a linear and an sRGB format a slot");
    for (uint32_t slot = 0u; slot < format_count / 2u; slot++) {
        formats_[slot * 2u] = static_cast<VkFormat>(slots[slot].vk_format());
        formats_[slot * 2u + 1u] = static_cast<VkFormat>(slots[slot].vk_srgb());
    }
}

VkResult SurfaceProperties::capabilities(VkSurfaceCapabilitiesKHR &answer)
{
    AV_SCOPE("SurfaceProperties::capabilities");
    WindowExtent              window(windows_, window_);
    const WindowExtent::Look  seen = window.look();
    if (seen == WindowExtent::Look::gone) return VK_ERROR_SURFACE_LOST_KHR;
    const uint32_t width = window.width();
    const uint32_t height = window.height();
    const bool     empty = seen == WindowExtent::Look::empty;
    answer = VkSurfaceCapabilitiesKHR{};
    answer.minImageCount = min_images;
    answer.maxImageCount = max_images;
    answer.currentExtent = {width, height};
    answer.minImageExtent = {empty ? 0u : 1u, empty ? 0u : 1u};
    answer.maxImageExtent = {empty ? 0u : max_extent, empty ? 0u : max_extent};
    answer.maxImageArrayLayers = 1u;
    answer.supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    answer.currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    answer.supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    answer.supportedUsageFlags = usage;
    return VK_SUCCESS;
}

VkResult SurfaceProperties::formats(uint32_t &count, VkSurfaceFormatKHR *formats)
{
    AV_SCOPE("SurfaceProperties::formats");
    if (formats == nullptr) {
        count = format_count;
        return VK_SUCCESS;
    }
    const uint32_t written = fits(count, format_count);
    for (uint32_t index = 0u; index < written; index++)
        formats[index] = {formats_[index], VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    return written < format_count ? VK_INCOMPLETE : VK_SUCCESS;
}

VkResult SurfaceProperties::formats2(uint32_t &count, VkSurfaceFormat2KHR *formats)
{
    AV_SCOPE("SurfaceProperties::formats2");
    if (formats == nullptr) {
        count = format_count;
        return VK_SUCCESS;
    }
    const uint32_t written = fits(count, format_count);
    for (uint32_t index = 0u; index < written; index++)
        formats[index].surfaceFormat = {formats_[index], VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    return written < format_count ? VK_INCOMPLETE : VK_SUCCESS;
}

VkResult SurfaceProperties::present_modes(uint32_t &count, VkPresentModeKHR *modes)
{
    AV_SCOPE("SurfaceProperties::present_modes");
    if (modes == nullptr) {
        count = mode_count;
        return VK_SUCCESS;
    }
    const uint32_t written = fits(count, mode_count);
    for (uint32_t index = 0u; index < written; index++) modes[index] = offered_modes[index];
    return written < mode_count ? VK_INCOMPLETE : VK_SUCCESS;
}

bool SurfaceProperties::offers_format(int32_t format, int32_t space) const
{
    AV_SCOPE("SurfaceProperties::offers_format");
    if (space != static_cast<int32_t>(VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)) return false;
    for (const VkFormat offered : formats_)
        if (static_cast<int32_t>(offered) == format) return true;
    return false;
}

bool SurfaceProperties::offers_mode(int32_t mode) const
{
    AV_SCOPE("SurfaceProperties::offers_mode");
    for (const VkPresentModeKHR offered : offered_modes)
        if (static_cast<int32_t>(offered) == mode) return true;
    return false;
}

uint32_t SurfaceProperties::fits(uint32_t &count, uint32_t available) const
{
    AV_SCOPE("SurfaceProperties::fits");
    count = count < available ? count : available;
    return count;
}

}
