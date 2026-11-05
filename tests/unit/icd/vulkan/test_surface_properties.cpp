#include "harness/framework/test_registry.hpp"
#include "harness/icd/icd_rig.hpp"
#include "harness/icd/scripted_vulkan_device.hpp"
#include "harness/icd/scripted_window_surface.hpp"
#include "icd/vulkan/presentation.hpp"
#include "icd/vulkan/surface_properties.hpp"
#include "icd/vulkan/win32_surface_create_record.hpp"

#include <cstdint>

using av::IcdRig;
using av::ScriptedVulkanDevice;
using av::ScriptedWindowSurface;
using av::icd::Presentation;
using av::icd::SurfaceProperties;
using av::icd::Win32SurfaceCreateRecord;
using av::icd::win32_surface_create_info;

namespace {

constexpr std::uint32_t width = 64u;
constexpr std::uint32_t height = 32u;

void *as_window(std::uintptr_t number) { return reinterpret_cast<void *>(number); }

class WindowRig {
public:
    WindowRig() { windows_.add_dc(as_window(0x20u), as_window(0x10u), width, height); }

    ScriptedWindowSurface &windows() { return windows_; }
    void                  *window() { return as_window(0x10u); }

private:
    ScriptedWindowSurface windows_;
};

}

TEST(surface_properties, the_capabilities_are_the_window_s_client_area_and_what_this_driver_presents)
{ WindowRig rig;
  SurfaceProperties properties(rig.windows(), rig.window());
  VkSurfaceCapabilitiesKHR capabilities = {};
  CHECK_EQ(properties.capabilities(capabilities), VK_SUCCESS);
  CHECK_EQ(capabilities.minImageCount, SurfaceProperties::min_images);
  CHECK_EQ(capabilities.maxImageCount, SurfaceProperties::max_images);
  CHECK_EQ(capabilities.currentExtent.width, width);
  CHECK_EQ(capabilities.currentExtent.height, height);
  CHECK_EQ(capabilities.minImageExtent.width, 1u);
  CHECK_EQ(capabilities.minImageExtent.height, 1u);
  CHECK_EQ(capabilities.maxImageExtent.width, 16384u);
  CHECK_EQ(capabilities.maxImageExtent.height, 16384u);
  CHECK_EQ(capabilities.maxImageArrayLayers, 1u);
  CHECK_EQ(capabilities.currentTransform, VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR);
  CHECK_EQ(capabilities.supportedTransforms, VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR);
  CHECK_EQ(capabilities.supportedCompositeAlpha, VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR);
  CHECK_EQ(capabilities.supportedUsageFlags, SurfaceProperties::usage);
  rig.windows().resize(rig.window(), 8u, 4u);
  CHECK_EQ(properties.capabilities(capabilities), VK_SUCCESS);
  CHECK_EQ(capabilities.currentExtent.width, 8u);
  CHECK_EQ(capabilities.currentExtent.height, 4u); }

TEST(surface_properties, a_minimized_window_offers_no_extent_at_all)
{ WindowRig rig;
  rig.windows().resize(rig.window(), 0u, 0u);
  SurfaceProperties properties(rig.windows(), rig.window());
  VkSurfaceCapabilitiesKHR capabilities = {};
  CHECK_EQ(properties.capabilities(capabilities), VK_SUCCESS);
  CHECK_EQ(capabilities.currentExtent.width, 0u);
  CHECK_EQ(capabilities.currentExtent.height, 0u);
  CHECK_EQ(capabilities.minImageExtent.width, 0u);
  CHECK_EQ(capabilities.minImageExtent.height, 0u);
  CHECK_EQ(capabilities.maxImageExtent.width, 0u);
  CHECK_EQ(capabilities.maxImageExtent.height, 0u);
  rig.windows().resize(rig.window(), width, 0u);
  CHECK_EQ(properties.capabilities(capabilities), VK_SUCCESS);
  CHECK_EQ(capabilities.maxImageExtent.width, 0u); }

TEST(surface_properties, a_window_that_is_gone_loses_the_surface)
{ WindowRig rig;
  SurfaceProperties gone(rig.windows(), rig.window());
  VkSurfaceCapabilitiesKHR capabilities = {};
  rig.windows().close_window(rig.window());
  CHECK_EQ(gone.capabilities(capabilities), VK_ERROR_SURFACE_LOST_KHR);
  SurfaceProperties none(rig.windows(), nullptr);
  CHECK_EQ(none.capabilities(capabilities), VK_ERROR_SURFACE_LOST_KHR); }

TEST(surface_properties, the_formats_are_the_four_it_presents_in_the_srgb_non_linear_space)
{ WindowRig rig;
  SurfaceProperties properties(rig.windows(), rig.window());
  std::uint32_t count = 0u;
  CHECK_EQ(properties.formats(count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, SurfaceProperties::format_count);
  VkSurfaceFormatKHR formats[SurfaceProperties::format_count] = {};
  CHECK_EQ(properties.formats(count, formats), VK_SUCCESS);
  CHECK_EQ(count, SurfaceProperties::format_count);
  const VkFormat expected[SurfaceProperties::format_count] = {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB,
                                                              VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB};
  for (std::uint32_t index = 0u; index < SurfaceProperties::format_count; index++) {
      CHECK_EQ(formats[index].format, expected[index]);
      CHECK_EQ(formats[index].colorSpace, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
      CHECK(properties.offers_format(expected[index], VK_COLOR_SPACE_SRGB_NONLINEAR_KHR));
  }
  count = 2u;
  formats[2] = VkSurfaceFormatKHR{};
  CHECK_EQ(properties.formats(count, formats), VK_INCOMPLETE);
  CHECK_EQ(count, 2u);
  CHECK_EQ(formats[1].format, VK_FORMAT_B8G8R8A8_SRGB);
  CHECK_EQ(formats[2].format, VK_FORMAT_UNDEFINED);
  count = SurfaceProperties::format_count + 3u;
  CHECK_EQ(properties.formats(count, formats), VK_SUCCESS);
  CHECK_EQ(count, SurfaceProperties::format_count);
  CHECK(!properties.offers_format(VK_FORMAT_R8G8B8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR));
  CHECK(!properties.offers_format(VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT)); }

TEST(surface_properties, the_two_spellings_of_the_formats_answer_the_same)
{ WindowRig rig;
  SurfaceProperties properties(rig.windows(), rig.window());
  std::uint32_t count = 0u;
  CHECK_EQ(properties.formats2(count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, SurfaceProperties::format_count);
  VkSurfaceFormat2KHR formats[SurfaceProperties::format_count] = {};
  CHECK_EQ(properties.formats2(count, formats), VK_SUCCESS);
  CHECK_EQ(formats[0].surfaceFormat.format, VK_FORMAT_B8G8R8A8_UNORM);
  CHECK_EQ(formats[3].surfaceFormat.format, VK_FORMAT_R8G8B8A8_SRGB);
  CHECK_EQ(formats[3].surfaceFormat.colorSpace, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
  count = 1u;
  CHECK_EQ(properties.formats2(count, formats), VK_INCOMPLETE);
  CHECK_EQ(count, 1u); }

TEST(surface_properties, the_present_modes_are_fifo_and_immediate)
{ WindowRig rig;
  SurfaceProperties properties(rig.windows(), rig.window());
  std::uint32_t count = 0u;
  CHECK_EQ(properties.present_modes(count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, SurfaceProperties::mode_count);
  VkPresentModeKHR modes[SurfaceProperties::mode_count] = {};
  CHECK_EQ(properties.present_modes(count, modes), VK_SUCCESS);
  CHECK_EQ(modes[0], VK_PRESENT_MODE_FIFO_KHR);
  CHECK_EQ(modes[1], VK_PRESENT_MODE_IMMEDIATE_KHR);
  CHECK(properties.offers_mode(VK_PRESENT_MODE_FIFO_KHR));
  CHECK(properties.offers_mode(VK_PRESENT_MODE_IMMEDIATE_KHR));
  CHECK(!properties.offers_mode(VK_PRESENT_MODE_MAILBOX_KHR));
  CHECK(!properties.offers_mode(VK_PRESENT_MODE_FIFO_RELAXED_KHR));
  count = 1u;
  modes[1] = VK_PRESENT_MODE_MAILBOX_KHR;
  CHECK_EQ(properties.present_modes(count, modes), VK_INCOMPLETE);
  CHECK_EQ(count, 1u);
  CHECK_EQ(modes[1], VK_PRESENT_MODE_MAILBOX_KHR); }

TEST(icd_presentation, the_surface_queries_answer_from_the_named_surface_s_window)
{ IcdRig rig;
  ScriptedVulkanDevice host;
  Presentation wsi(rig.platform(), rig.kmt(), rig.windows(), rig.chains(), rig.lines(), rig.root(), host,
                   rig.present_thread(), rig.handed(), rig.shown(), rig.present_lock());
  rig.windows().add_dc(as_window(0x20u), as_window(0x10u), width, height);
  Win32SurfaceCreateRecord info = {};
  info.type_ = win32_surface_create_info;
  info.window_ = as_window(0x10u);
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  CHECK_EQ(wsi.create_surface(reinterpret_cast<VkInstance>(1u), &info, &surface), VK_SUCCESS);

  VkSurfaceCapabilitiesKHR capabilities = {};
  CHECK_EQ(wsi.capabilities(surface, &capabilities), VK_SUCCESS);
  CHECK_EQ(capabilities.currentExtent.width, width);
  VkPhysicalDeviceSurfaceInfo2KHR named = {};
  named.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
  named.surface = surface;
  VkSurfaceCapabilities2KHR second = {};
  second.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;
  CHECK_EQ(wsi.capabilities2(&named, &second), VK_SUCCESS);
  CHECK_EQ(second.surfaceCapabilities.currentExtent.height, height);

  std::uint32_t count = 0u;
  CHECK_EQ(wsi.formats(surface, &count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, SurfaceProperties::format_count);
  VkSurfaceFormat2KHR formats2[SurfaceProperties::format_count] = {};
  CHECK_EQ(wsi.formats2(&named, &count, formats2), VK_SUCCESS);
  CHECK_EQ(formats2[0].surfaceFormat.format, VK_FORMAT_B8G8R8A8_UNORM);
  VkPresentModeKHR modes[SurfaceProperties::mode_count] = {};
  count = SurfaceProperties::mode_count;
  CHECK_EQ(wsi.present_modes(surface, &count, modes), VK_SUCCESS);
  CHECK_EQ(modes[0], VK_PRESENT_MODE_FIFO_KHR);

  wsi.destroy_surface(reinterpret_cast<VkInstance>(1u), surface);
  CHECK_EQ(wsi.capabilities(surface, &capabilities), VK_ERROR_SURFACE_LOST_KHR);
  CHECK_EQ(wsi.capabilities2(&named, &second), VK_ERROR_SURFACE_LOST_KHR);
  CHECK_EQ(wsi.formats(surface, &count, nullptr), VK_ERROR_SURFACE_LOST_KHR);
  CHECK_EQ(wsi.formats2(&named, &count, formats2), VK_ERROR_SURFACE_LOST_KHR);
  CHECK_EQ(wsi.present_modes(surface, &count, modes), VK_ERROR_SURFACE_LOST_KHR);
  CHECK(wsi.invariants_hold()); }
