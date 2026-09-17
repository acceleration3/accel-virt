/* One swapchain (feature 046, contracts/wsi.md): what its creation refuses,
 * the images it hands out in turn, and what a present puts on the window - the
 * frame's own bytes, bottom-up and in the window's channel order, paced for
 * FIFO and not for IMMEDIATE. The window changes under it: resized, minimized,
 * gone. */
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/scripted_vulkan_device.hpp"
#include "harness/icd/scripted_window_surface.hpp"
#include "harness/platform/mock_platform.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "icd/vulkan/surface_properties.hpp"
#include "core/support/object_storage.hpp"
#include "icd/vulkan/swapchain.hpp"

#include <cstdint>

namespace av::icd {

/* Breaks each invariant a swapchain keeps, and the ones its device and its
 * pacer keep under it. */
struct PresentDeviceProbe {
    static void forget_image(PresentDevice &device) { device.made_[0].image_ = VK_NULL_HANDLE; }
};

struct FramePacerProbe {
    static void count_a_blank_without_an_adapter(FramePacer &pacer)
    { pacer.adapter_ = 0u;
      pacer.blanks_ = 1u; }
};

struct SwapchainProbe {
    static void hold_past_the_count(Swapchain &chain) { chain.held_[chain.count_] = true; }
    static void miscount(Swapchain &chain) { chain.count_ = PresentDevice::max_images + 1u; }
    /* Taken and put back, so the frame buffer is still released. */
    static std::uint8_t *take_frame(Swapchain &chain)
    { std::uint8_t *const frame = chain.frame_;
      chain.frame_ = nullptr;
      return frame; }
    static void put_frame(Swapchain &chain, std::uint8_t *frame) { chain.frame_ = frame; }
    static void forget_image(Swapchain &chain) { PresentDeviceProbe::forget_image(chain.device_); }
    /* Fewer images than the device made: the two must agree. */
    static void undercount(Swapchain &chain) { chain.count_ = 1u; }
    static void pace_without_an_adapter(Swapchain &chain)
    { FramePacerProbe::count_a_blank_without_an_adapter(chain.pacer_); }
};

}  // namespace av::icd

using av::MockKmt;
using av::MockPlatform;
using av::RecordingLog;
using av::ScriptedVulkanDevice;
using av::ScriptedWindowSurface;
using av::icd::SurfaceProperties;
using av::icd::Swapchain;
using av::icd::SwapchainProbe;

namespace {

constexpr std::uint32_t width = 4u;
constexpr std::uint32_t height = 2u;
constexpr std::uint32_t images = 2u;
constexpr std::uint32_t frame_bytes = width * height * 4u;

void *as_window(std::uintptr_t number) { return reinterpret_cast<void *>(number); }

VkSwapchainCreateInfoKHR create_info()
{ VkSwapchainCreateInfoKHR info = {};
  info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  info.minImageCount = images;
  info.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
  info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  info.imageExtent = {width, height};
  info.imageArrayLayers = 1u;
  info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  info.clipped = VK_TRUE;
  return info; }

/* A swapchain on a scripted host device, a window of its own, and the rvkgpu
 * adapter its FIFO pacing waits on. A swapchain is neither copied nor moved,
 * so the rig holds the one it makes, and the second for a recreation. */
class SwapchainRig {
public:
    SwapchainRig()
    { windows_.add_dc(as_window(0x20u), window(), width, height);
      kmt_.add_adapter(av::PciIdentity(av::AdapterScenario::virtio_vendor, av::AdapterScenario::virtio_gpu_device,
                                       av::AdapterScenario::virtio_vendor, av::AdapterScenario::rvk_subsystem),
                       av::AdapterScenario::wddm_3_2);
      std::uint32_t adapters[2] = {};
      std::uint32_t found = 0u;
      CHECK_STATUS(kmt_.enumerate_adapters(adapters, 2u, found), av::Status::ok);
      adapter_ = adapters[0];
      CHECK_STATUS(kmt_.create_device(adapter_, device_), av::Status::ok);
      CHECK_STATUS(first_.construct(memory_, windows_, host_, host_.device(), host_.physical(), kmt_, log_, adapter_,
                                    device_),
                   av::Status::ok);
      CHECK_STATUS(second_.construct(memory_, windows_, host_, host_.device(), host_.physical(), kmt_, log_, adapter_,
                                     device_),
                   av::Status::ok); }
    ~SwapchainRig()
    { second_.destroy();
      first_.destroy(); }

    Swapchain &chain() { return first_.object(); }
    Swapchain &other() { return second_.object(); }

    VkResult initialize(Swapchain &chain, const VkSwapchainCreateInfoKHR &info)
    { SurfaceProperties properties(windows_, window());
      return chain.initialize(info, window(), properties); }

    MockPlatform          &memory() { return memory_; }
    MockKmt               &kmt() { return kmt_; }
    RecordingLog          &log() { return log_; }
    ScriptedVulkanDevice  &host() { return host_; }
    ScriptedWindowSurface &windows() { return windows_; }
    void                  *window() { return as_window(0x10u); }

private:
    MockPlatform               memory_;
    MockKmt                    kmt_;
    RecordingLog               log_;
    ScriptedVulkanDevice       host_;
    ScriptedWindowSurface      windows_;
    std::uint32_t              adapter_ = 0u;
    std::uint32_t              device_ = 0u;
    av::ObjectStorage<Swapchain> first_;
    av::ObjectStorage<Swapchain> second_;
};

}  // namespace

TEST(swapchain, its_images_are_host_images_of_the_window_s_size)
{ SwapchainRig rig;
  Swapchain &chain = rig.chain();
  CHECK(chain.invariants_hold());
  CHECK_EQ(rig.initialize(chain, create_info()), VK_SUCCESS);
  CHECK(chain.invariants_hold());
  CHECK_EQ(rig.host().images(), images);
  CHECK(chain.device() == rig.host().device());
  CHECK_EQ(chain.presented(), 0u);
  std::uint32_t count = 0u;
  CHECK_EQ(chain.images(count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, images);
  VkImage made[images + 1u] = {};
  count = images + 1u;
  CHECK_EQ(chain.images(count, made), VK_SUCCESS);
  CHECK_EQ(count, images);
  CHECK(made[0] != VK_NULL_HANDLE);
  CHECK(made[0] != made[1]);
  CHECK(made[2] == VK_NULL_HANDLE);
  count = 1u;
  CHECK_EQ(chain.images(count, made), VK_INCOMPLETE);
  CHECK_EQ(count, 1u);
  /* A swapchain is made once. */
  CHECK_EQ(rig.initialize(chain, create_info()), VK_ERROR_INITIALIZATION_FAILED); }

/* An application acquires and presents in turn; a synchronous present frees
 * the image it is handed, so an acquire always has one after a present. */
TEST(swapchain, images_are_acquired_in_turn_and_a_present_frees_the_one_it_was_handed)
{ SwapchainRig rig;
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, create_info()), VK_SUCCESS);
  std::uint32_t first = 9u;
  std::uint32_t second = 9u;
  CHECK_EQ(chain.acquire(0u, first), VK_SUCCESS);
  CHECK_EQ(first, 0u);
  CHECK(chain.invariants_hold());
  CHECK_EQ(chain.acquire(0u, second), VK_SUCCESS);
  CHECK_EQ(second, 1u);
  /* Both are held: with no timeout that is not ready, with one it is a
   * timeout, since nothing can free an image while the call waits. */
  std::uint32_t none = 9u;
  CHECK_EQ(chain.acquire(0u, none), VK_NOT_READY);
  CHECK_EQ(chain.acquire(1000u, none), VK_TIMEOUT);
  CHECK_EQ(none, 9u);
  CHECK_EQ(chain.present(rig.host().queue(), first, 0u, nullptr), VK_SUCCESS);
  CHECK_EQ(chain.presented(), 1u);
  CHECK(chain.invariants_hold());
  std::uint32_t again = 9u;
  CHECK_EQ(chain.acquire(0u, again), VK_SUCCESS);
  CHECK_EQ(again, 0u);
  /* An index never acquired, one already presented, and one past the end. */
  CHECK_EQ(chain.present(rig.host().queue(), first + images, 0u, nullptr), VK_ERROR_UNKNOWN);
  CHECK_EQ(chain.present(rig.host().queue(), first, 0u, nullptr), VK_SUCCESS);
  CHECK_EQ(chain.present(rig.host().queue(), first, 0u, nullptr), VK_ERROR_UNKNOWN);
  CHECK_EQ(chain.presented(), 2u);
  CHECK(chain.invariants_hold()); }

/* The frame on the window is the image's own bytes: a Vulkan image's first row
 * is the top one and a DIB's is the bottom one, and an RGBA image's red and
 * blue trade places on the way to a BGRA window. */
TEST(swapchain, a_present_puts_the_frame_on_the_window_bottom_up_in_the_window_s_channel_order)
{ const VkFormat formats[] = {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB,
                              VK_FORMAT_R8G8B8A8_SRGB};
  for (const VkFormat format : formats) {
      SwapchainRig rig;
      Swapchain &chain = rig.chain();
      VkSwapchainCreateInfoKHR info = create_info();
      info.imageFormat = format;
      CHECK_EQ(rig.initialize(chain, info), VK_SUCCESS);
      std::uint32_t index = 0u;
      CHECK_EQ(chain.acquire(0u, index), VK_SUCCESS);
      VkImage made[images] = {};
      std::uint32_t count = images;
      CHECK_EQ(chain.images(count, made), VK_SUCCESS);
      rig.host().paint(made[index], 0x10u, 0x20u, 0x30u, 0x40u);
      CHECK_EQ(chain.present(rig.host().queue(), index, 0u, nullptr), VK_SUCCESS);
      CHECK_EQ(rig.windows().blits(), 1u);
      CHECK_EQ(rig.windows().last_blit_width(), width);
      CHECK_EQ(rig.windows().last_blit_height(), height);
      CHECK(rig.windows().last_blit_dc() == as_window(0x20u));
      const bool rgba = format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_R8G8B8A8_SRGB;
      const std::uint8_t *const bytes = rig.windows().last_blit_bytes();
      for (std::uint32_t at = 0u; at < frame_bytes; at += 4u) {
          CHECK_EQ(bytes[at], rgba ? 0x30u : 0x10u);
          CHECK_EQ(bytes[at + 1u], 0x20u);
          CHECK_EQ(bytes[at + 2u], rgba ? 0x10u : 0x30u);
          CHECK_EQ(bytes[at + 3u], 0x40u);
      }
  } }

/* FIFO holds the frame to the adapter's vertical blank; IMMEDIATE does not. */
TEST(swapchain, fifo_paces_a_present_on_the_vertical_blank_and_immediate_does_not)
{ const VkPresentModeKHR modes[] = {VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR};
  for (const VkPresentModeKHR mode : modes) {
      SwapchainRig rig;
      Swapchain &chain = rig.chain();
      VkSwapchainCreateInfoKHR info = create_info();
      info.presentMode = mode;
      CHECK_EQ(rig.initialize(chain, info), VK_SUCCESS);
      std::uint32_t index = 0u;
      CHECK_EQ(chain.acquire(0u, index), VK_SUCCESS);
      CHECK_EQ(chain.present(rig.host().queue(), index, 0u, nullptr), VK_SUCCESS);
      const bool fifo = mode == VK_PRESENT_MODE_FIFO_KHR;
      CHECK_EQ(chain.pacer().blanks(), fifo ? 1u : 0u);
      CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank), fifo ? 1u : 0u);
  } }

/* Everything contracts/wsi.md refuses, and nothing is made for any of it. */
TEST(swapchain, a_create_structure_this_driver_cannot_present_is_refused)
{ struct Hostile {
      const char *what;
      VkResult    answer;
  };
  const Hostile cases[] = {
      {"another structure type", VK_ERROR_INITIALIZATION_FAILED},
      {"fewer images than the surface offers", VK_ERROR_INITIALIZATION_FAILED},
      {"more images than the surface offers", VK_ERROR_INITIALIZATION_FAILED},
      {"more than one array layer", VK_ERROR_INITIALIZATION_FAILED},
      {"a usage the surface does not offer", VK_ERROR_INITIALIZATION_FAILED},
      {"a rotated transform", VK_ERROR_INITIALIZATION_FAILED},
      {"a composite alpha that is not opaque", VK_ERROR_INITIALIZATION_FAILED},
      {"a format it does not present", VK_ERROR_INITIALIZATION_FAILED},
      {"another colour space", VK_ERROR_INITIALIZATION_FAILED},
      {"a present mode it does not offer", VK_ERROR_INITIALIZATION_FAILED},
      {"an extent whose width is not the window's", VK_ERROR_OUT_OF_DATE_KHR},
      {"an extent whose height is not the window's", VK_ERROR_OUT_OF_DATE_KHR},
  };
  for (std::uint32_t index = 0u; index < sizeof cases / sizeof cases[0]; index++) {
      SwapchainRig rig;
      Swapchain &chain = rig.chain();
      VkSwapchainCreateInfoKHR info = create_info();
      switch (index) {
      case 0u: info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; break;
      case 1u: info.minImageCount = SurfaceProperties::min_images - 1u; break;
      case 2u: info.minImageCount = SurfaceProperties::max_images + 1u; break;
      case 3u: info.imageArrayLayers = 2u; break;
      case 4u: info.imageUsage = VK_IMAGE_USAGE_STORAGE_BIT; break;
      case 5u: info.preTransform = VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR; break;
      case 6u: info.compositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR; break;
      case 7u: info.imageFormat = VK_FORMAT_R5G6B5_UNORM_PACK16; break;
      case 8u: info.imageColorSpace = VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT; break;
      case 9u: info.presentMode = VK_PRESENT_MODE_MAILBOX_KHR; break;
      case 10u: info.imageExtent = {width + 1u, height}; break;
      default: info.imageExtent = {width, height + 1u}; break;
      }
      CHECK_EQ(rig.initialize(chain, info), cases[index].answer);
      CHECK_EQ(rig.host().images(), 0u);
      CHECK_EQ(rig.host().memories(), 0u);
      CHECK_EQ(rig.memory().live_allocations(), 0u);
      CHECK(chain.invariants_hold());
  } }

/* A window with no client area cannot be presented to at all, and one that is
 * gone is a lost surface, whatever the create structure says. */
TEST(swapchain, a_minimized_window_refuses_creation_and_a_gone_one_loses_the_surface)
{ SwapchainRig rig;
  rig.windows().resize(rig.window(), 0u, 0u);
  Swapchain &minimized = rig.chain();
  VkSwapchainCreateInfoKHR info = create_info();
  info.imageExtent = {0u, 0u};
  CHECK_EQ(rig.initialize(minimized, info), VK_ERROR_INITIALIZATION_FAILED);
  CHECK(minimized.invariants_hold());
  SwapchainRig flat;
  flat.windows().resize(flat.window(), width, 0u);
  VkSwapchainCreateInfoKHR none = create_info();
  none.imageExtent = {width, 0u};
  CHECK_EQ(flat.initialize(flat.chain(), none), VK_ERROR_INITIALIZATION_FAILED);
  rig.windows().close_window(rig.window());
  Swapchain &gone = rig.other();
  CHECK_EQ(rig.initialize(gone, create_info()), VK_ERROR_SURFACE_LOST_KHR);
  CHECK_EQ(rig.host().images(), 0u); }

/* Each host object a swapchain needs can fail to be made, and each failure
 * leaves nothing made and says so once. */
TEST(swapchain, a_swapchain_whose_host_objects_cannot_be_made_leaves_nothing_behind)
{ const char *const steps[] = {"vkCreateBuffer", "vkMapMemory", "vkCreateFence", "vkCreateImage"};
  for (const char *const step : steps) {
      SwapchainRig rig;
      Swapchain &chain = rig.chain();
      rig.host().refuse(step, 1u, VK_ERROR_OUT_OF_DEVICE_MEMORY);
      CHECK_EQ(rig.initialize(chain, create_info()), VK_ERROR_OUT_OF_DEVICE_MEMORY);
      CHECK_EQ(rig.host().images(), 0u);
      CHECK_EQ(rig.host().memories(), 0u);
      CHECK_EQ(rig.memory().live_allocations(), 0u);
      CHECK_EQ(rig.log().holding("a swapchain's host objects could not be made"), 1u);
      CHECK(chain.invariants_hold());
  }
  /* The second image of two: the first is already made when it fails. */
  SwapchainRig rig;
  Swapchain &chain = rig.chain();
  rig.host().refuse("vkCreateImage", 2u, VK_ERROR_OUT_OF_DEVICE_MEMORY);
  CHECK_EQ(rig.initialize(chain, create_info()), VK_ERROR_OUT_OF_DEVICE_MEMORY);
  CHECK_EQ(rig.host().images(), 0u);
  CHECK(chain.invariants_hold());
  /* No guest memory for the frame at all. */
  SwapchainRig starved;
  Swapchain &none = starved.chain();
  starved.memory().fail_allocation_at(1u);
  CHECK_EQ(starved.initialize(none, create_info()), VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK_EQ(starved.host().images(), 0u);
  CHECK(none.invariants_hold()); }

/* A window that changed only in height is as out of date as one that changed
 * in width. */
TEST(swapchain, a_window_of_another_height_is_out_of_date_too)
{ SwapchainRig rig;
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, create_info()), VK_SUCCESS);
  rig.windows().resize(rig.window(), width, height + 1u);
  std::uint32_t index = 9u;
  CHECK_EQ(chain.acquire(0u, index), VK_ERROR_OUT_OF_DATE_KHR);
  CHECK_EQ(index, 9u); }

/* Every invariant a swapchain keeps, broken in turn: an image held past the
 * count, a count past the images, no frame buffer, an image the device lost,
 * and a blank counted with no adapter to have counted it. */
TEST(swapchain, its_invariants_refuse_a_hold_past_the_count_a_miscount_and_a_lost_image)
{ SwapchainRig rig;
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, create_info()), VK_SUCCESS);
  CHECK(chain.invariants_hold());
  SwapchainProbe::hold_past_the_count(chain);
  CHECK(!chain.invariants_hold());

  SwapchainRig miscounted;
  CHECK_EQ(miscounted.initialize(miscounted.chain(), create_info()), VK_SUCCESS);
  SwapchainProbe::miscount(miscounted.chain());
  CHECK(!miscounted.chain().invariants_hold());

  SwapchainRig frameless;
  CHECK_EQ(frameless.initialize(frameless.chain(), create_info()), VK_SUCCESS);
  std::uint8_t *const frame = SwapchainProbe::take_frame(frameless.chain());
  CHECK(!frameless.chain().invariants_hold());
  SwapchainProbe::put_frame(frameless.chain(), frame);

  SwapchainRig lost;
  CHECK_EQ(lost.initialize(lost.chain(), create_info()), VK_SUCCESS);
  SwapchainProbe::undercount(lost.chain());
  CHECK(!lost.chain().invariants_hold());

  SwapchainRig broken;
  CHECK_EQ(broken.initialize(broken.chain(), create_info()), VK_SUCCESS);
  SwapchainProbe::forget_image(broken.chain());
  CHECK(!broken.chain().invariants_hold());

  SwapchainRig unpaced;
  CHECK_EQ(unpaced.initialize(unpaced.chain(), create_info()), VK_SUCCESS);
  SwapchainProbe::pace_without_an_adapter(unpaced.chain());
  CHECK(!unpaced.chain().invariants_hold()); }

/* The window changed size under the swapchain: everything it hands out is out
 * of date until the application makes another (US2). */
TEST(swapchain, a_resized_window_puts_a_swapchain_out_of_date)
{ SwapchainRig rig;
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, create_info()), VK_SUCCESS);
  std::uint32_t index = 0u;
  CHECK_EQ(chain.acquire(0u, index), VK_SUCCESS);
  rig.windows().resize(rig.window(), width * 2u, height);
  std::uint32_t after = 9u;
  CHECK_EQ(chain.acquire(0u, after), VK_ERROR_OUT_OF_DATE_KHR);
  CHECK_EQ(after, 9u);
  /* The frame it holds is not drawn: the window is no longer its size. */
  CHECK_EQ(chain.present(rig.host().queue(), index, 0u, nullptr), VK_ERROR_OUT_OF_DATE_KHR);
  CHECK_EQ(rig.windows().blits(), 0u);
  CHECK_EQ(chain.presented(), 0u);
  /* A swapchain of the new size presents. */
  Swapchain &next = rig.other();
  VkSwapchainCreateInfoKHR info = create_info();
  info.imageExtent = {width * 2u, height};
  CHECK_EQ(rig.initialize(next, info), VK_SUCCESS);
  std::uint32_t fresh = 0u;
  CHECK_EQ(next.acquire(0u, fresh), VK_SUCCESS);
  CHECK_EQ(next.present(rig.host().queue(), fresh, 0u, nullptr), VK_SUCCESS);
  CHECK_EQ(rig.windows().last_blit_width(), width * 2u);
  CHECK(next.invariants_hold()); }

TEST(swapchain, a_window_that_goes_loses_the_surface_and_a_retired_swapchain_hands_out_nothing)
{ SwapchainRig rig;
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, create_info()), VK_SUCCESS);
  std::uint32_t index = 0u;
  CHECK_EQ(chain.acquire(0u, index), VK_SUCCESS);
  rig.windows().close_window(rig.window());
  CHECK_EQ(chain.present(rig.host().queue(), index, 0u, nullptr), VK_ERROR_SURFACE_LOST_KHR);
  std::uint32_t after = 9u;
  CHECK_EQ(chain.acquire(0u, after), VK_ERROR_SURFACE_LOST_KHR);
  CHECK_EQ(chain.presented(), 0u);

  SwapchainRig second;
  Swapchain &retired = second.chain();
  CHECK_EQ(second.initialize(retired, create_info()), VK_SUCCESS);
  retired.retire();
  CHECK_EQ(retired.acquire(0u, after), VK_ERROR_OUT_OF_DATE_KHR);
  CHECK(retired.invariants_hold()); }

/* A window that refuses the blit is said once and answers what the window is:
 * a device that lost its window, not a frame that was drawn. */
TEST(swapchain, a_window_that_refuses_the_frame_is_not_a_present)
{ SwapchainRig rig;
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, create_info()), VK_SUCCESS);
  std::uint32_t index = 0u;
  CHECK_EQ(chain.acquire(0u, index), VK_SUCCESS);
  rig.windows().refuse_next_blit();
  CHECK_EQ(chain.present(rig.host().queue(), index, 0u, nullptr), VK_SUCCESS);
  CHECK_EQ(chain.presented(), 0u);
  CHECK_EQ(rig.log().holding("a presented frame was refused by its window"), 1u);
  /* The copy itself failing is the device's own error, and nothing is drawn. */
  std::uint32_t next = 0u;
  CHECK_EQ(chain.acquire(0u, next), VK_SUCCESS);
  rig.host().refuse("vkQueueSubmit", 1u, VK_ERROR_DEVICE_LOST);
  CHECK_EQ(chain.present(rig.host().queue(), next, 0u, nullptr), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(chain.presented(), 0u);
  CHECK(chain.invariants_hold()); }
