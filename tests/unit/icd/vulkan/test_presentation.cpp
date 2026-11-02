#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/icd_rig.hpp"
#include "harness/icd/scripted_vulkan_device.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/vulkan/icd_entry_lookup.hpp"
#include "icd/vulkan/presentation.hpp"
#include "icd/vulkan/surface_properties.hpp"
#include "icd/vulkan/win32_surface_create_record.hpp"

#include <cstdint>
#include <cstring>

namespace av::icd {
struct SurfaceTableProbe {
    static void strip_the_instance(SurfaceTable &table) { table.slots_[0].instance_ = VK_NULL_HANDLE; }
};

struct SwapchainProbe {
    static void hold_past_the_count(Swapchain &chain) { chain.held_[chain.count_] = true; }
};

struct PresentationProbe {
    static void orphan_a_surface(Presentation &wsi) { SurfaceTableProbe::strip_the_instance(wsi.surfaces_); }
    static void break_a_swapchain(Presentation &wsi)
    { for (SwapchainSlot &slot : wsi.slots_)
          if (slot.chain_.constructed()) SwapchainProbe::hold_past_the_count(slot.chain_.object()); }
};

}

using av::IcdRig;
using av::ScriptedHost;
using av::ScriptedVulkanDevice;
using av::icd::Presentation;
using av::icd::SurfaceProperties;
using av::icd::PresentationProbe;
using av::icd::SwapchainSlot;
using av::icd::Win32SurfaceCreateRecord;
using av::icd::win32_surface_create_info;

namespace {

using CreateDevice = VkResult (*)(VkPhysicalDevice, const VkDeviceCreateInfo *, const VkAllocationCallbacks *,
                                  VkDevice *);

constexpr std::uint32_t width = 4u;
constexpr std::uint32_t height = 2u;
constexpr std::uint32_t images = 2u;

void *as_window(std::uintptr_t number) { return reinterpret_cast<void *>(number); }

VkSwapchainCreateInfoKHR create_info(VkSurfaceKHR surface)
{ VkSwapchainCreateInfoKHR info = {};
  info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  info.surface = surface;
  info.minImageCount = images;
  info.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
  info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  info.imageExtent = {width, height};
  info.imageArrayLayers = 1u;
  info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  info.clipped = VK_TRUE;
  return info; }

class PresentationRig {
public:
    PresentationRig() : wsi_(rig_.platform(), rig_.kmt(), rig_.windows(), rig_.chains(), rig_.log(), rig_.root(), host_)
    { instance_ = rig_.create_instance();
      physical_ = rig_.physical_device(instance_);
      VkDeviceCreateInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
      rig_.host().vk_handle(0, ScriptedHost::vk_device);
      CHECK_EQ(rig_.entry<CreateDevice>(instance_, "vkCreateDevice")(physical_, &info, nullptr, &device_), VK_SUCCESS);
      rig_.windows().add_dc(as_window(0x20u), window(), width, height);
      Win32SurfaceCreateRecord surface = {};
      surface.type_ = win32_surface_create_info;
      surface.window_ = window();
      CHECK_EQ(wsi_.create_surface(instance_, &surface, &surface_), VK_SUCCESS); }
    ~PresentationRig() { rig_.destroy_instance(instance_); }

    IcdRig               &rig() { return rig_; }
    Presentation         &wsi() { return wsi_; }
    ScriptedVulkanDevice &host() { return host_; }
    VkInstance            instance() const { return instance_; }
    VkDevice              device() const { return device_; }
    VkSurfaceKHR          surface() const { return surface_; }
    void                 *window() { return as_window(0x10u); }

    VkSwapchainKHR create(const VkSwapchainCreateInfoKHR &info)
    { VkSwapchainKHR swapchain = VK_NULL_HANDLE;
      CHECK_EQ(wsi_.create_swapchain(device_, &info, &swapchain), VK_SUCCESS);
      return swapchain; }

private:
    IcdRig               rig_;
    ScriptedVulkanDevice host_;
    Presentation         wsi_;
    VkInstance           instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice     physical_ = VK_NULL_HANDLE;
    VkDevice             device_ = VK_NULL_HANDLE;
    VkSurfaceKHR         surface_ = VK_NULL_HANDLE;
};

}

TEST(icd_presentation, a_swapchain_presents_its_frame_on_the_window)
{ PresentationRig rig;
  const VkSwapchainKHR swapchain = rig.create(create_info(rig.surface()));
  CHECK(swapchain != VK_NULL_HANDLE);
  CHECK(reinterpret_cast<const SwapchainSlot *>(swapchain)->chain_.constructed());
  CHECK_EQ(rig.wsi().swapchains(), 1u);
  CHECK(rig.wsi().invariants_hold());
  std::uint32_t count = 0u;
  CHECK_EQ(rig.wsi().swapchain_images(rig.device(), swapchain, &count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, images);
  VkImage made[images] = {};
  CHECK_EQ(rig.wsi().swapchain_images(rig.device(), swapchain, &count, made), VK_SUCCESS);
  std::uint32_t index = 9u;
  CHECK_EQ(rig.wsi().acquire(rig.device(), swapchain, 0u, VK_NULL_HANDLE, VK_NULL_HANDLE, &index), VK_SUCCESS);
  CHECK_EQ(index, 0u);
  rig.host().paint(made[index], 0x81u, 0x82u, 0x83u, 0xFFu);
  VkPresentInfoKHR present = {};
  present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present.swapchainCount = 1u;
  present.pSwapchains = &swapchain;
  present.pImageIndices = &index;
  CHECK_EQ(rig.wsi().present(rig.host().queue(), &present), VK_SUCCESS);
  CHECK_EQ(rig.rig().windows().blits(), 1u);
  CHECK_EQ(rig.rig().windows().last_blit_bytes()[0], 0x81u);
  CHECK_EQ(rig.rig().windows().last_blit_bytes()[3], 0xFFu);
  rig.wsi().destroy_swapchain(rig.device(), swapchain);
  CHECK_EQ(rig.wsi().swapchains(), 0u);
  CHECK(rig.wsi().invariants_hold()); }

TEST(icd_presentation, the_entries_a_swapchain_calls_are_the_icd_s_own)
{ PresentationRig rig;
  av::icd::IcdEntryLookup lookup(rig.rig().root());
  CHECK(lookup.entry("vkCreateImage") ==
        rig.rig().root().instance_proc_addr(rig.instance(), "vkCreateImage"));
  CHECK(lookup.entry("vkQueueSubmit") != nullptr);
  CHECK(lookup.entry("vkCmdCopyImageToBuffer") != nullptr);
  CHECK(lookup.entry("vkNotAnEntryPoint") == nullptr); }

TEST(icd_presentation, a_swapchain_needs_this_driver_s_own_device_and_surface)
{ PresentationRig rig;
  VkSwapchainKHR swapchain = reinterpret_cast<VkSwapchainKHR>(0x1u);
  const VkSwapchainCreateInfoKHR info = create_info(rig.surface());
  int stack = 0;
  CHECK_EQ(rig.wsi().create_swapchain(reinterpret_cast<VkDevice>(&stack), &info, &swapchain),
           VK_ERROR_INITIALIZATION_FAILED);
  CHECK(swapchain == VK_NULL_HANDLE);
  CHECK_EQ(rig.wsi().create_swapchain(rig.device(), nullptr, &swapchain), VK_ERROR_INITIALIZATION_FAILED);
  const VkSwapchainCreateInfoKHR nowhere = create_info(reinterpret_cast<VkSurfaceKHR>(0x40u));
  CHECK_EQ(rig.wsi().create_swapchain(rig.device(), &nowhere, &swapchain), VK_ERROR_SURFACE_LOST_KHR);
  VkSwapchainCreateInfoKHR mailbox = info;
  mailbox.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
  CHECK_EQ(rig.wsi().create_swapchain(rig.device(), &mailbox, &swapchain), VK_ERROR_INITIALIZATION_FAILED);
  CHECK_EQ(rig.wsi().swapchains(), 0u);
  CHECK(rig.wsi().invariants_hold()); }

TEST(icd_presentation, more_swapchains_than_the_process_holds_are_out_of_memory)
{ PresentationRig rig;
  const VkSwapchainCreateInfoKHR info = create_info(rig.surface());
  VkSwapchainKHR made[Presentation::max_swapchains] = {};
  for (std::uint32_t index = 0u; index < Presentation::max_swapchains; index++) made[index] = rig.create(info);
  CHECK_EQ(rig.wsi().swapchains(), Presentation::max_swapchains);
  VkSwapchainKHR past = reinterpret_cast<VkSwapchainKHR>(0x1u);
  CHECK_EQ(rig.wsi().create_swapchain(rig.device(), &info, &past), VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK(past == VK_NULL_HANDLE);
  rig.wsi().destroy_swapchain(rig.device(), made[0]);
  CHECK_EQ(rig.wsi().swapchains(), Presentation::max_swapchains - 1u);
  CHECK(rig.wsi().invariants_hold()); }

TEST(icd_presentation, the_old_swapchain_a_recreation_names_is_retired)
{ PresentationRig rig;
  const VkSwapchainKHR first = rig.create(create_info(rig.surface()));
  VkSwapchainCreateInfoKHR again = create_info(rig.surface());
  again.oldSwapchain = first;
  const VkSwapchainKHR second = rig.create(again);
  CHECK(second != first);
  CHECK_EQ(rig.wsi().swapchains(), 2u);
  std::uint32_t index = 9u;
  CHECK_EQ(rig.wsi().acquire(rig.device(), first, 0u, VK_NULL_HANDLE, VK_NULL_HANDLE, &index), VK_ERROR_OUT_OF_DATE_KHR);
  CHECK_EQ(rig.wsi().acquire(rig.device(), second, 0u, VK_NULL_HANDLE, VK_NULL_HANDLE, &index), VK_SUCCESS);
  rig.wsi().destroy_swapchain(rig.device(), first);
  CHECK_EQ(rig.wsi().swapchains(), 1u);
  CHECK(rig.wsi().invariants_hold()); }

TEST(icd_presentation, the_old_swapchain_is_retired_even_when_its_recreation_fails)
{ PresentationRig rig;
  const VkSwapchainKHR first = rig.create(create_info(rig.surface()));
  VkSwapchainCreateInfoKHR again = create_info(rig.surface());
  again.oldSwapchain = first;
  again.imageExtent = {width + 1u, height};
  VkSwapchainKHR second = VK_NULL_HANDLE;
  CHECK_EQ(rig.wsi().create_swapchain(rig.device(), &again, &second), VK_ERROR_OUT_OF_DATE_KHR);
  CHECK(second == VK_NULL_HANDLE);
  std::uint32_t index = 9u;
  CHECK_EQ(rig.wsi().acquire(rig.device(), first, 0u, VK_NULL_HANDLE, VK_NULL_HANDLE, &index), VK_ERROR_OUT_OF_DATE_KHR);
  CHECK_EQ(rig.wsi().swapchains(), 1u); }

TEST(icd_presentation, a_swapchain_this_driver_did_not_make_is_out_of_date_or_refused)
{ PresentationRig rig;
  const VkSwapchainKHR swapchain = rig.create(create_info(rig.surface()));
  const VkSwapchainKHR nowhere = reinterpret_cast<VkSwapchainKHR>(0x50u);
  std::uint32_t count = 1u;
  VkImage image = VK_NULL_HANDLE;
  CHECK_EQ(rig.wsi().swapchain_images(rig.device(), nowhere, &count, &image), VK_ERROR_INITIALIZATION_FAILED);
  std::uint32_t index = 9u;
  CHECK_EQ(rig.wsi().acquire(rig.device(), nowhere, 0u, VK_NULL_HANDLE, VK_NULL_HANDLE, &index), VK_ERROR_OUT_OF_DATE_KHR);
  int stack = 0;
  VkDevice other = reinterpret_cast<VkDevice>(&stack);
  CHECK_EQ(rig.wsi().acquire(other, swapchain, 0u, VK_NULL_HANDLE, VK_NULL_HANDLE, &index), VK_ERROR_OUT_OF_DATE_KHR);
  rig.wsi().destroy_swapchain(other, swapchain);
  rig.wsi().destroy_swapchain(rig.device(), nowhere);
  CHECK_EQ(rig.wsi().swapchains(), 1u);
  CHECK(rig.wsi().invariants_hold()); }

TEST(icd_presentation, a_present_of_several_swapchains_answers_the_worst_result)
{ PresentationRig rig;
  const VkSwapchainKHR first = rig.create(create_info(rig.surface()));
  const VkSwapchainKHR second = rig.create(create_info(rig.surface()));
  const VkSwapchainKHR chains[] = {first, second, reinterpret_cast<VkSwapchainKHR>(0x60u)};
  std::uint32_t indices[3] = {0u, 0u, 0u};
  for (std::uint32_t index = 0u; index < 2u; index++)
      CHECK_EQ(rig.wsi().acquire(rig.device(), chains[index], 0u, VK_NULL_HANDLE, VK_NULL_HANDLE, &indices[index]), VK_SUCCESS);
  VkResult results[3] = {VK_SUCCESS, VK_SUCCESS, VK_SUCCESS};
  VkPresentInfoKHR present = {};
  present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present.swapchainCount = 3u;
  present.pSwapchains = chains;
  present.pImageIndices = indices;
  present.pResults = results;
  CHECK_EQ(rig.wsi().present(rig.host().queue(), &present), VK_ERROR_OUT_OF_DATE_KHR);
  CHECK_EQ(results[0], VK_SUCCESS);
  CHECK_EQ(results[1], VK_SUCCESS);
  CHECK_EQ(results[2], VK_ERROR_OUT_OF_DATE_KHR);
  CHECK_EQ(rig.rig().windows().blits(), 2u);
  CHECK(rig.wsi().invariants_hold()); }

TEST(icd_presentation, the_present_s_semaphores_are_waited_on_once)
{ PresentationRig rig;
  const VkSwapchainKHR first = rig.create(create_info(rig.surface()));
  const VkSwapchainKHR second = rig.create(create_info(rig.surface()));
  const VkSwapchainKHR chains[] = {first, second};
  std::uint32_t indices[2] = {0u, 0u};
  for (std::uint32_t index = 0u; index < 2u; index++)
      CHECK_EQ(rig.wsi().acquire(rig.device(), chains[index], 0u, VK_NULL_HANDLE, VK_NULL_HANDLE, &indices[index]), VK_SUCCESS);
  const VkSemaphore waits[] = {reinterpret_cast<VkSemaphore>(0x700u)};
  VkPresentInfoKHR present = {};
  present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present.waitSemaphoreCount = 1u;
  present.pWaitSemaphores = waits;
  present.swapchainCount = 2u;
  present.pSwapchains = chains;
  present.pImageIndices = indices;
  CHECK_EQ(rig.wsi().present(rig.host().queue(), &present), VK_SUCCESS);
  CHECK_EQ(rig.host().submits(), 2u);
  CHECK_EQ(rig.host().last_wait_count(), 0u); }

TEST(icd_presentation, a_device_that_goes_takes_its_swapchains)
{ PresentationRig rig;
  static_cast<void>(rig.create(create_info(rig.surface())));
  CHECK_EQ(rig.wsi().swapchains(), 1u);
  CHECK_EQ(rig.host().images(), images);
  int stack = 0;
  rig.wsi().forget_device(reinterpret_cast<VkDevice>(&stack));
  CHECK_EQ(rig.wsi().swapchains(), 1u);
  rig.wsi().forget_device(rig.device());
  CHECK_EQ(rig.wsi().swapchains(), 0u);
  CHECK_EQ(rig.host().images(), 0u);
  CHECK(rig.wsi().invariants_hold()); }

TEST(icd_presentation, its_report_names_the_surfaces_and_each_swapchain_s_presents)
{ PresentationRig rig;
  const VkSwapchainKHR swapchain = rig.create(create_info(rig.surface()));
  std::uint32_t index = 0u;
  CHECK_EQ(rig.wsi().acquire(rig.device(), swapchain, 0u, VK_NULL_HANDLE, VK_NULL_HANDLE, &index), VK_SUCCESS);
  VkPresentInfoKHR present = {};
  present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present.swapchainCount = 1u;
  present.pSwapchains = &swapchain;
  present.pImageIndices = &index;
  CHECK_EQ(rig.wsi().present(rig.host().queue(), &present), VK_SUCCESS);
  char text[512] = {};
  av::StateWriter writer(text, sizeof text);
  rig.wsi().report(writer);
  CHECK(std::strstr(text, "presentation/0") != nullptr);
  CHECK(std::strstr(text, "surfaces=1") != nullptr);
  CHECK(std::strstr(text, "swapchains=1") != nullptr);
  CHECK(std::strstr(text, "swapchain/0") != nullptr);
  CHECK(std::strstr(text, "presented=1") != nullptr); }

TEST(icd_presentation, its_invariants_refuse_a_swapchain_that_lost_one)
{ PresentationRig rig;
  static_cast<void>(rig.create(create_info(rig.surface())));
  CHECK(rig.wsi().invariants_hold());
  PresentationProbe::break_a_swapchain(rig.wsi());
  CHECK(!rig.wsi().invariants_hold()); }

TEST(icd_presentation, its_invariants_refuse_a_surface_no_instance_owns)
{ PresentationRig rig;
  CHECK(rig.wsi().invariants_hold());
  PresentationProbe::orphan_a_surface(rig.wsi());
  CHECK(!rig.wsi().invariants_hold()); }

TEST(icd_presentation, a_destroyed_swapchain_s_handle_answers_nothing)
{ PresentationRig rig;
  const VkSwapchainKHR live = rig.create(create_info(rig.surface()));
  const VkSwapchainKHR swapchain = rig.create(create_info(rig.surface()));
  rig.wsi().destroy_swapchain(rig.device(), swapchain);
  std::uint32_t index = 9u;
  CHECK_EQ(rig.wsi().acquire(rig.device(), swapchain, 0u, VK_NULL_HANDLE, VK_NULL_HANDLE, &index), VK_ERROR_OUT_OF_DATE_KHR);
  std::uint32_t count = 0u;
  CHECK_EQ(rig.wsi().swapchain_images(rig.device(), swapchain, &count, nullptr), VK_ERROR_INITIALIZATION_FAILED);
  CHECK_EQ(rig.wsi().acquire(rig.device(), live, 0u, VK_NULL_HANDLE, VK_NULL_HANDLE, &index), VK_SUCCESS);
  const VkSwapchainKHR chains[] = {swapchain, live};
  std::uint32_t indices[2] = {0u, index};
  VkPresentInfoKHR present = {};
  present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present.swapchainCount = 2u;
  present.pSwapchains = chains;
  present.pImageIndices = indices;
  CHECK_EQ(rig.wsi().present(rig.host().queue(), &present), VK_ERROR_OUT_OF_DATE_KHR);
  CHECK_EQ(rig.rig().windows().blits(), 1u); }
