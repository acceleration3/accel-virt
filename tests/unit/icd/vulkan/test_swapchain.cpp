#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/open_connection.hpp"
#include "harness/icd/counted_wake_signal.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/icd/scripted_thread.hpp"
#include "harness/icd/scripted_vulkan_device.hpp"
#include "harness/icd/scripted_window_chains.hpp"
#include "harness/icd/scripted_window_surface.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/vulkan/surface_properties.hpp"
#include "core/support/object_storage.hpp"
#include "icd/vulkan/fence_ready.hpp"
#include "icd/common/window_present_queue.hpp"
#include "icd/vulkan/swapchain.hpp"

#include <cstdint>

namespace av::icd {
struct PresentDeviceProbe {
    static void forget_image(PresentDevice &device) { device.made_[0].image_ = VK_NULL_HANDLE; }
};

struct FramePacerProbe {
    static void count_a_blank_without_an_adapter(FramePacer &pacer)
    {
        pacer.adapter_ = 0u;
        pacer.blanks_ = 1u;
    }
};

struct PresentPacingProbe {
    static void count_a_blank_without_an_adapter(PresentPacing &pacing) { FramePacerProbe::count_a_blank_without_an_adapter(pacing.pacer_.object()); }
};

struct ChainSlotsProbe {
    static void forget_window(ChainSlots &slots) { slots.window_ = nullptr; }
};

struct ChainedImageProbe {
    static ChainSlots &slots(ChainedImage &chained) { return chained.slots_; }
};

struct SwapchainProbe {
    static void unsound_pacing(Swapchain &chain) { PresentPacingProbe::count_a_blank_without_an_adapter(chain.pacing_); }
    static void unsound_chained_image(Swapchain &chain) { ChainSlotsProbe::forget_window(ChainedImageProbe::slots(chain.chained_)); }
    static void done_in_slot(Swapchain &chain, std::uint32_t slot, bool drawn)
    {
        chain.done(VkFrame(chain, slot, nullptr, VkFrame::Rows::bgra_top_down, 1u, 1u, nullptr, FenceReady::Mark{nullptr, VK_NULL_HANDLE}), drawn);
    }
    static void hold_past_the_count(Swapchain &chain) { chain.holders_[chain.count_] = Swapchain::Holder::application; }
    static void miscount(Swapchain &chain) { chain.count_ = PresentDevice::max_images + 1u; }
    static void forget_image(Swapchain &chain) { PresentDeviceProbe::forget_image(chain.device_); }
    static void undercount(Swapchain &chain) { chain.count_ = 1u; }
    static VkImage slot(Swapchain &chain) { return chain.chained_.slot(0u); }
    static void lose_slot(Swapchain &chain) { chain.chained_.release(); }
};

}

using av::CountedWakeSignal;
using av::MockKmt;
using av::RecordingThreadLock;
using av::ScriptedHost;
using av::ScriptedThread;
using av::MockPlatform;
using av::RecordingLog;
using av::ScriptedVulkanDevice;
using av::ScriptedWindowChains;
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

class SwapchainRig {
public:
    explicit SwapchainRig(bool queued = false)
        : present_lock_(open_.trace(), 'P'), ready_(host_),
          presents_(memory_, thread_, handed_, shown_, present_lock_, ready_, windows_, chains_, open_.lines())
    { if (!queued) thread_.refuse_start();
      windows_.add_dc(as_window(0x20u), window(), width, height);
      CHECK_STATUS(first_.construct(windows_, chains_, host_, host_.device(), ScriptedHost::vk_device, host_.physical(),
                                    open_.kmt(), open_.lines(), open_.connection(), presents_),
                   av::Status::ok);
      CHECK_STATUS(second_.construct(windows_, chains_, host_, host_.device(), ScriptedHost::vk_device,
                                     host_.physical(), open_.kmt(), open_.lines(), open_.connection(), presents_),
                   av::Status::ok); }
    ~SwapchainRig()
    { if (second_.constructed()) second_.destroy();
      if (first_.constructed()) first_.destroy(); }

    Swapchain &chain() { return first_.object(); }
    Swapchain &other() { return second_.object(); }
    void reset_first() { first_.destroy(); }
    void reset_second() { second_.destroy(); }

    VkResult initialize(Swapchain &chain, const VkSwapchainCreateInfoKHR &info)
    { SurfaceProperties properties(windows_, window());
      return chain.initialize(info, window(), properties); }

    void pump() { thread_.run_until_done(); }
    std::uint32_t shown_waits() const { return shown_.waits(); }
    std::uint32_t shown_raises() const { return shown_.raises(); }
    std::uint32_t shown_last_wait() const { return shown_.last_wait(); }

    MockPlatform          &memory() { return memory_; }
    MockKmt               &kmt() { return open_.kmt(); }
    av::ScriptedHost      &device() { return open_.host(); }
    av::icd::Connection   &connection() { return open_.connection(); }
    RecordingLog          &log() { return open_.log(); }
    ScriptedVulkanDevice  &host() { return host_; }
    ScriptedWindowSurface &windows() { return windows_; }
    ScriptedWindowChains  &chains() { return chains_; }
    ScriptedThread        &thread() { return thread_; }
    av::icd::VkPresentQueue &presents() { return presents_; }
    void                  *window() { return as_window(0x10u); }

private:
    MockPlatform               memory_;
    av::OpenConnection         open_;
    ScriptedVulkanDevice       host_;
    ScriptedWindowSurface      windows_;
    ScriptedWindowChains       chains_;
    ScriptedThread             thread_;
    CountedWakeSignal          handed_;
    CountedWakeSignal          shown_;
    RecordingThreadLock        present_lock_;
    av::icd::FenceReady        ready_;
    av::icd::VkPresentQueue    presents_;
    av::ObjectStorage<Swapchain> first_;
    av::ObjectStorage<Swapchain> second_;
};

}

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
  CHECK_EQ(rig.initialize(chain, create_info()), VK_ERROR_INITIALIZATION_FAILED); }

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
  CHECK_EQ(chain.present(rig.host().queue(), first + images, 0u, nullptr), VK_ERROR_UNKNOWN);
  CHECK_EQ(chain.present(rig.host().queue(), first, 0u, nullptr), VK_SUCCESS);
  CHECK_EQ(chain.present(rig.host().queue(), first, 0u, nullptr), VK_ERROR_UNKNOWN);
  CHECK_EQ(chain.presented(), 2u);
  CHECK(chain.invariants_hold()); }

TEST(swapchain, a_queued_present_returns_at_once_and_the_present_thread_shows_the_frame_and_frees_the_image)
{ SwapchainRig rig(true);
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, create_info()), VK_SUCCESS);
  std::uint32_t first = 9u;
  CHECK_EQ(chain.acquire(0u, first), VK_SUCCESS);
  VkImage made[images] = {};
  std::uint32_t count = images;
  CHECK_EQ(chain.images(count, made), VK_SUCCESS);
  rig.host().paint(made[first], 0x51u, 0x52u, 0x53u, 0x54u);
  CHECK_EQ(chain.present(rig.host().queue(), first, 0u, nullptr), VK_SUCCESS);
  CHECK_EQ(chain.presented(), 1u);
  CHECK_EQ(rig.thread().starts(), 1u);
  CHECK_EQ(rig.windows().blits(), 0u);
  CHECK_EQ(rig.presents().queued(), 1u);
  CHECK(chain.invariants_hold());
  std::uint32_t second = 9u;
  CHECK_EQ(chain.acquire(0u, second), VK_SUCCESS);
  CHECK(second != first);
  std::uint32_t none = 9u;
  CHECK_EQ(chain.acquire(0u, none), VK_NOT_READY);
  CHECK_EQ(chain.present(rig.host().queue(), first, 0u, nullptr), VK_ERROR_UNKNOWN);
  rig.pump();
  CHECK_EQ(rig.windows().blits(), 1u);
  CHECK_EQ(rig.windows().last_blit_bytes()[0], 0x51u);
  CHECK_EQ(rig.windows().last_blit_bytes()[3], 0x54u);
  CHECK_EQ(rig.presents().shown_frames(), 1u);
  CHECK_EQ(rig.presents().queued(), 0u);
  std::uint32_t again = 9u;
  CHECK_EQ(chain.acquire(0u, again), VK_SUCCESS);
  CHECK_EQ(again, first);
  CHECK(chain.invariants_hold());
  CHECK(rig.presents().invariants_hold()); }

TEST(swapchain, a_swapchain_that_goes_while_its_frame_is_queued_takes_the_frame_with_it)
{ SwapchainRig rig(true);
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, create_info()), VK_SUCCESS);
  std::uint32_t index = 0u;
  CHECK_EQ(chain.acquire(0u, index), VK_SUCCESS);
  CHECK_EQ(chain.present(rig.host().queue(), index, 0u, nullptr), VK_SUCCESS);
  CHECK_EQ(rig.presents().queued(), 1u);
  rig.reset_first();
  CHECK_EQ(rig.presents().queued(), 0u);
  rig.pump();
  CHECK_EQ(rig.windows().blits(), 0u);
  CHECK(rig.presents().invariants_hold()); }

TEST(swapchain, a_present_puts_the_frame_on_the_window_top_down_in_the_window_s_channel_order)
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
      rig.host().paint_row(made[index], 0u, width * 4u, 0x11u, 0x22u, 0x33u, 0x44u);
      CHECK_EQ(chain.present(rig.host().queue(), index, 0u, nullptr), VK_SUCCESS);
      CHECK_EQ(rig.windows().blits(), 1u);
      CHECK(rig.windows().last_blit_top_down());
      CHECK_EQ(rig.windows().last_blit_width(), width);
      CHECK_EQ(rig.windows().last_blit_height(), height);
      CHECK(rig.windows().last_blit_dc() == as_window(0x20u));
      const bool rgba = format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_R8G8B8A8_SRGB;
      const std::uint8_t *const bytes = rig.windows().last_blit_bytes();
      for (std::uint32_t at = 0u; at < frame_bytes; at += 4u) {
          const bool top = at < width * 4u;
          CHECK_EQ(bytes[at], rgba ? (top ? 0x33u : 0x30u) : (top ? 0x11u : 0x10u));
          CHECK_EQ(bytes[at + 1u], top ? 0x22u : 0x20u);
          CHECK_EQ(bytes[at + 2u], rgba ? (top ? 0x11u : 0x10u) : (top ? 0x33u : 0x30u));
          CHECK_EQ(bytes[at + 3u], top ? 0x44u : 0x40u);
      }
      CHECK_EQ(rig.memory().live_allocations() != 0u, rgba);
  } }

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
      CHECK_EQ(chain.pacing().blanks(), fifo ? 1u : 0u);
      CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_vertical_blank), fifo ? 1u : 0u);
  } }

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
      case 4u: info.imageUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; break;
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
  SwapchainRig rig;
  Swapchain &chain = rig.chain();
  rig.host().refuse("vkCreateImage", 2u, VK_ERROR_OUT_OF_DEVICE_MEMORY);
  CHECK_EQ(rig.initialize(chain, create_info()), VK_ERROR_OUT_OF_DEVICE_MEMORY);
  CHECK_EQ(rig.host().images(), 0u);
  CHECK(chain.invariants_hold());
  CHECK(chain.invariants_hold()); }

TEST(swapchain, a_window_of_another_height_is_out_of_date_too)
{ SwapchainRig rig;
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, create_info()), VK_SUCCESS);
  rig.windows().resize(rig.window(), width, height + 1u);
  std::uint32_t index = 9u;
  CHECK_EQ(chain.acquire(0u, index), VK_ERROR_OUT_OF_DATE_KHR);
  CHECK_EQ(index, 9u); }

namespace {
av::ObjectStorage<SwapchainRig> invariant_rig;

template <typename Check> void with_rig(Check check)
{
    static_cast<void>(invariant_rig.construct());
    check(invariant_rig.object());
    invariant_rig.destroy();
}

}

TEST(swapchain, its_invariants_refuse_a_hold_past_the_count_a_miscount_and_a_lost_image)
{ with_rig([](SwapchainRig &rig) {
      Swapchain &chain = rig.chain();
      CHECK_EQ(rig.initialize(chain, create_info()), VK_SUCCESS);
      CHECK(chain.invariants_hold());
      SwapchainProbe::hold_past_the_count(chain);
      CHECK(!chain.invariants_hold());
  });
  with_rig([](SwapchainRig &miscounted) {
      CHECK_EQ(miscounted.initialize(miscounted.chain(), create_info()), VK_SUCCESS);
      SwapchainProbe::miscount(miscounted.chain());
      CHECK(!miscounted.chain().invariants_hold());
  });
  with_rig([](SwapchainRig &lost) {
      CHECK_EQ(lost.initialize(lost.chain(), create_info()), VK_SUCCESS);
      SwapchainProbe::undercount(lost.chain());
      CHECK(!lost.chain().invariants_hold());
  });
  with_rig([](SwapchainRig &broken) {
      CHECK_EQ(broken.initialize(broken.chain(), create_info()), VK_SUCCESS);
      SwapchainProbe::forget_image(broken.chain());
      CHECK(!broken.chain().invariants_hold());
  }); }

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
  CHECK_EQ(chain.present(rig.host().queue(), index, 0u, nullptr), VK_ERROR_OUT_OF_DATE_KHR);
  CHECK_EQ(rig.windows().blits(), 0u);
  CHECK_EQ(chain.presented(), 0u);
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

TEST(swapchain, a_window_that_refuses_the_frame_is_not_a_present)
{ SwapchainRig rig;
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, create_info()), VK_SUCCESS);
  std::uint32_t index = 0u;
  CHECK_EQ(chain.acquire(0u, index), VK_SUCCESS);
  rig.windows().refuse_next_blit();
  CHECK_EQ(chain.present(rig.host().queue(), index, 0u, nullptr), VK_ERROR_OUT_OF_DATE_KHR);
  CHECK_EQ(chain.presented(), 0u);
  CHECK_EQ(rig.log().holding("a presented frame was refused by its window"), 1u);
  std::uint32_t again = 0u;
  CHECK_EQ(chain.acquire(0u, again), VK_SUCCESS);
  rig.windows().refuse_next_blit();
  CHECK_EQ(chain.present(rig.host().queue(), again, 0u, nullptr), VK_ERROR_OUT_OF_DATE_KHR);
  CHECK_EQ(chain.presented(), 0u);
  CHECK_EQ(rig.log().holding("a presented frame was refused by its window"), 1u);
  std::uint32_t next = 0u;
  CHECK_EQ(chain.acquire(0u, next), VK_SUCCESS);
  rig.host().refuse("vkQueueSubmit", 1u, VK_ERROR_DEVICE_LOST);
  CHECK_EQ(chain.present(rig.host().queue(), next, 0u, nullptr), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(chain.presented(), 0u);
  CHECK(chain.invariants_hold()); }

TEST(swapchain, a_window_with_a_chain_is_shown_the_frame_copied_into_its_slot_not_blitted)
{ const VkFormat formats[] = {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB};
  for (const VkFormat format : formats) {
      SwapchainRig rig;
      rig.chains().refuse_opens(false);
      rig.device().shared_object(0u, 0x7700u);
      rig.device().shared_object(0u, 0x7710u);
      Swapchain &chain = rig.chain();
      VkSwapchainCreateInfoKHR info = create_info();
      info.imageFormat = format;
      CHECK_EQ(rig.initialize(chain, info), VK_SUCCESS);
      CHECK(chain.invariants_hold());
      CHECK_EQ(rig.chains().opens(), 1u);
      CHECK(rig.chains().open_window() == rig.window());
      CHECK_EQ(rig.chains().last_luid(), rig.connection().luid());
      const VkImage slot = SwapchainProbe::slot(chain);
      CHECK(slot != VK_NULL_HANDLE);
      CHECK_EQ(rig.host().images(), images + 2u);
      CHECK_EQ(rig.host().mappings(), 0u);
      CHECK_EQ(rig.memory().live_allocations(), 0u);
      std::uint32_t index = 0u;
      CHECK_EQ(chain.acquire(0u, index), VK_SUCCESS);
      VkImage made[images] = {};
      std::uint32_t count = images;
      CHECK_EQ(chain.images(count, made), VK_SUCCESS);
      rig.host().paint(made[index], 0x10u, 0x20u, 0x30u, 0x40u);
      CHECK_EQ(chain.present(rig.host().queue(), index, 0u, nullptr), VK_SUCCESS);
      CHECK_EQ(rig.chains().presents(), 1u);
      CHECK_EQ(rig.windows().blits(), 0u);
      CHECK_EQ(chain.presented(), 1u);
      CHECK_EQ(rig.host().image_copies(), 1u);
      const std::uint8_t *const bytes = rig.host().bytes(slot);
      CHECK_EQ(bytes[0], 0x10u);
      CHECK_EQ(bytes[2], 0x30u);
      CHECK_EQ(bytes[frame_bytes - 1u], 0x40u);
      CHECK_EQ(chain.pacing().blanks(), 1u);
      CHECK(chain.invariants_hold());
  } }

TEST(swapchain, a_frame_the_chain_refuses_is_said_and_a_failed_copy_is_the_device_s_error)
{ SwapchainRig rig;
  rig.chains().refuse_opens(false);
  rig.device().shared_object(0u, 0x7700u);
  rig.device().shared_object(0u, 0x7710u);
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, create_info()), VK_SUCCESS);
  std::uint32_t index = 0u;
  CHECK_EQ(chain.acquire(0u, index), VK_SUCCESS);
  rig.chains().refuse_presents(true);
  CHECK_EQ(chain.present(rig.host().queue(), index, 0u, nullptr), VK_ERROR_OUT_OF_DATE_KHR);
  CHECK_EQ(rig.log().holding("a presented frame was refused by its window"), 1u);
  CHECK_EQ(chain.presented(), 0u);
  CHECK_EQ(rig.windows().blits(), 0u);
  CHECK_EQ(chain.acquire(0u, index), VK_SUCCESS);
  CHECK_EQ(chain.present(rig.host().queue(), index, 0u, nullptr), VK_ERROR_OUT_OF_DATE_KHR);
  CHECK_EQ(rig.log().holding("a presented frame was refused by its window"), 1u);
  CHECK_EQ(chain.acquire(0u, index), VK_SUCCESS);
  rig.host().refuse("vkQueueSubmit", 1u, VK_ERROR_DEVICE_LOST);
  CHECK_EQ(chain.present(rig.host().queue(), index, 0u, nullptr), VK_ERROR_DEVICE_LOST);
  CHECK(chain.invariants_hold()); }

TEST(swapchain, a_retired_swapchain_gives_its_slot_back_and_leaves_the_chain_to_its_successor)
{ SwapchainRig rig;
  rig.chains().refuse_opens(false);
  rig.device().shared_object(0u, 0x7700u);
  rig.device().shared_object(0u, 0x7710u);
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, create_info()), VK_SUCCESS);
  std::uint32_t index = 0u;
  CHECK_EQ(chain.acquire(0u, index), VK_SUCCESS);
  const std::uint32_t before = rig.host().images();
  rig.device().shared_object(0u, 0u);
  rig.device().shared_object(0u, 0u);
  chain.retire();
  CHECK_EQ(rig.host().images(), before - 2u);
  CHECK(chain.invariants_hold());
  const VkSemaphore waits[] = {reinterpret_cast<VkSemaphore>(0x900u)};
  const std::uint32_t submits = rig.host().submits();
  CHECK_EQ(chain.present(rig.host().queue(), index, 1u, waits), VK_ERROR_OUT_OF_DATE_KHR);
  CHECK_EQ(rig.host().submits(), submits + 1u);
  CHECK(rig.host().last_wait() == waits[0]);
  CHECK_EQ(rig.host().image_copies(), 0u);
  CHECK_EQ(rig.chains().presents(), 0u);
  rig.device().shared_object(0u, 0x7701u);
  rig.device().shared_object(0u, 0x7711u);
  CHECK_EQ(rig.initialize(rig.other(), create_info()), VK_SUCCESS);
  CHECK_EQ(rig.chains().opens(), 2u);
  CHECK(SwapchainProbe::slot(rig.other()) != VK_NULL_HANDLE);
  CHECK_EQ(chain.acquire(0u, index), VK_ERROR_OUT_OF_DATE_KHR);
  rig.host().refuse("vkQueueSubmit", 1u, VK_ERROR_DEVICE_LOST);
  CHECK_EQ(chain.present(rig.host().queue(), 1u - index, 1u, waits), VK_ERROR_UNKNOWN);
  CHECK_EQ(rig.chains().closes(), 0u);
  rig.reset_first();
  CHECK_EQ(rig.chains().closes(), 0u);
  rig.reset_second();
  CHECK_EQ(rig.chains().closes(), 1u);
  CHECK(rig.chains().last_closed() == rig.window()); }

TEST(swapchain, its_invariants_refuse_a_chained_swapchain_without_its_slot)
{ SwapchainRig rig;
  rig.chains().refuse_opens(false);
  rig.device().shared_object(0u, 0x7700u);
  rig.device().shared_object(0u, 0x7710u);
  CHECK_EQ(rig.initialize(rig.chain(), create_info()), VK_SUCCESS);
  CHECK(rig.chain().invariants_hold());
  SwapchainProbe::lose_slot(rig.chain());
  CHECK(!rig.chain().invariants_hold()); }

TEST(swapchain, a_chain_whose_slot_cannot_be_opened_falls_back_to_the_blit)
{ SwapchainRig rig;
  rig.chains().refuse_opens(false);
  rig.device().shared_object(1u, 0u);
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, create_info()), VK_SUCCESS);
  CHECK_EQ(rig.chains().closes(), 1u);
  CHECK_EQ(rig.host().images(), images);
  CHECK_EQ(rig.host().mappings(), 1u);
  std::uint32_t index = 0u;
  CHECK_EQ(chain.acquire(0u, index), VK_SUCCESS);
  CHECK_EQ(chain.present(rig.host().queue(), index, 0u, nullptr), VK_SUCCESS);
  CHECK_EQ(rig.windows().blits(), 1u);
  CHECK_EQ(rig.chains().presents(), 0u);
  CHECK(chain.invariants_hold()); }

TEST(swapchain, an_acquire_that_waits_for_a_queued_present_gives_up_when_its_time_is_spent)
{ SwapchainRig rig(true);
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, create_info()), VK_SUCCESS);
  std::uint32_t first = 9u;
  std::uint32_t second = 9u;
  CHECK_EQ(chain.acquire(0u, first), VK_SUCCESS);
  CHECK_EQ(chain.acquire(0u, second), VK_SUCCESS);
  CHECK_EQ(chain.present(rig.host().queue(), first, 0u, nullptr), VK_SUCCESS);
  std::uint32_t none = 9u;
  const std::uint32_t waits = rig.shown_waits();
  CHECK_EQ(chain.acquire(1000u, none), VK_TIMEOUT);
  CHECK_EQ(rig.shown_waits(), waits + 1u);
  CHECK_EQ(rig.shown_last_wait(), 1u);
  CHECK_EQ(none, 9u);
  rig.pump();
  CHECK_EQ(chain.acquire(1000u, none), VK_SUCCESS);
  CHECK_EQ(none, first);
  CHECK(chain.invariants_hold()); }

TEST(swapchain, a_present_the_pacer_does_not_admit_is_skipped_and_frees_its_image)
{ SwapchainRig rig;
  Swapchain &chain = rig.chain();
  VkSwapchainCreateInfoKHR info = create_info();
  info.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
  CHECK_EQ(rig.initialize(chain, info), VK_SUCCESS);
  std::uint32_t index = 0u;
  CHECK_EQ(chain.acquire(0u, index), VK_SUCCESS);
  rig.kmt().reads_clock(nullptr, 2u * (MockKmt::ticks_per_second / 60u) + 100000u);
  CHECK_EQ(chain.present(rig.host().queue(), index, 0u, nullptr), VK_SUCCESS);
  CHECK_EQ(chain.presented(), 1u);
  CHECK_EQ(chain.acquire(0u, index), VK_SUCCESS);
  rig.kmt().reads_clock(nullptr, 300u);
  const std::uint32_t submits = rig.host().submits();
  CHECK_EQ(chain.present(rig.host().queue(), index, 0u, nullptr), VK_SUCCESS);
  CHECK_EQ(chain.presented(), 1u);
  CHECK_EQ(rig.host().submits(), submits);
  std::uint32_t again = 9u;
  CHECK_EQ(chain.acquire(0u, again), VK_SUCCESS);
  CHECK(chain.invariants_hold()); }

TEST(swapchain, a_present_for_a_chain_that_lost_its_slot_is_out_of_date_or_the_error_of_the_skip)
{ SwapchainRig rig;
  rig.chains().refuse_opens(false);
  rig.device().shared_object(0u, 0x7700u);
  rig.device().shared_object(0u, 0x7710u);
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, create_info()), VK_SUCCESS);
  std::uint32_t index = 0u;
  CHECK_EQ(chain.acquire(0u, index), VK_SUCCESS);
  SwapchainProbe::lose_slot(chain);
  VkSemaphore many[av::icd::PresentDevice::max_waits + 1u] = {};
  CHECK_EQ(chain.present(rig.host().queue(), index, av::icd::PresentDevice::max_waits + 1u, many),
           VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK_EQ(chain.presented(), 0u);
  CHECK_EQ(chain.acquire(0u, index), VK_SUCCESS);
  CHECK_EQ(chain.present(rig.host().queue(), index, 0u, nullptr), VK_ERROR_OUT_OF_DATE_KHR); }

TEST(swapchain, a_queued_present_takes_the_second_chain_slot_while_the_first_is_queued)
{ SwapchainRig rig(true);
  rig.chains().refuse_opens(false);
  rig.device().shared_object(0u, 0x7700u);
  rig.device().shared_object(0u, 0x7710u);
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, create_info()), VK_SUCCESS);
  std::uint32_t first = 9u;
  std::uint32_t second = 9u;
  std::uint32_t third = 9u;
  CHECK_EQ(chain.acquire(0u, first), VK_SUCCESS);
  CHECK_EQ(chain.acquire(0u, second), VK_SUCCESS);
  CHECK_EQ(chain.present(rig.host().queue(), first, 0u, nullptr), VK_SUCCESS);
  CHECK_EQ(chain.present(rig.host().queue(), second, 0u, nullptr), VK_SUCCESS);
  CHECK_EQ(rig.presents().queued(), 2u);
  CHECK_EQ(chain.acquire(0u, third), VK_NOT_READY);
  CHECK(chain.invariants_hold()); }

TEST(swapchain, a_queued_present_whose_copy_fails_frees_its_image_and_answers_the_error)
{ SwapchainRig rig(true);
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, create_info()), VK_SUCCESS);
  std::uint32_t index = 0u;
  CHECK_EQ(chain.acquire(0u, index), VK_SUCCESS);
  rig.host().refuse("vkQueueSubmit", 1u, VK_ERROR_DEVICE_LOST);
  CHECK_EQ(chain.present(rig.host().queue(), index, 0u, nullptr), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(chain.presented(), 0u);
  CHECK_EQ(rig.presents().queued(), 0u);
  std::uint32_t next = 9u;
  CHECK_EQ(chain.acquire(0u, index), VK_SUCCESS);
  CHECK_EQ(chain.acquire(0u, next), VK_SUCCESS);
  CHECK(index != next);
  CHECK(chain.invariants_hold()); }

TEST(swapchain, a_queued_present_the_queue_has_no_room_for_is_finished_on_the_spot)
{ SwapchainRig rig(true);
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, create_info()), VK_SUCCESS);
  std::uint32_t index = 0u;
  CHECK_EQ(chain.acquire(0u, index), VK_SUCCESS);
  rig.memory().fail_allocation_at(1u);
  CHECK_EQ(chain.present(rig.host().queue(), index, 0u, nullptr), VK_SUCCESS);
  rig.memory().fail_allocation_at(0u);
  CHECK_EQ(rig.presents().queued(), 0u);
  CHECK_EQ(rig.windows().blits(), 0u);
  CHECK_EQ(rig.presents().dropped_frames(), 1u);
  std::uint32_t again = 9u;
  std::uint32_t other = 9u;
  CHECK_EQ(chain.acquire(0u, again), VK_SUCCESS);
  CHECK_EQ(chain.acquire(0u, other), VK_SUCCESS);
  CHECK(again != other);
  CHECK(chain.invariants_hold()); }

TEST(swapchain, a_frame_the_window_refuses_as_it_is_resized_answers_the_resize)
{ SwapchainRig rig;
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, create_info()), VK_SUCCESS);
  std::uint32_t index = 0u;
  CHECK_EQ(chain.acquire(0u, index), VK_SUCCESS);
  rig.windows().refuse_next_blit();
  rig.windows().resize_when_a_blit_is_refused(rig.window(), width + 1u, height);
  CHECK_EQ(chain.present(rig.host().queue(), index, 0u, nullptr), VK_ERROR_OUT_OF_DATE_KHR);
  CHECK_EQ(chain.presented(), 0u);
  CHECK_EQ(chain.acquire(0u, index), VK_ERROR_OUT_OF_DATE_KHR); }

TEST(swapchain, a_frame_finished_in_a_slot_the_swapchain_never_had_is_ignored_and_one_in_a_free_slot_changes_nothing)
{ SwapchainRig rig;
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, create_info()), VK_SUCCESS);
  std::uint32_t index = 0u;
  CHECK_EQ(chain.acquire(0u, index), VK_SUCCESS);
  SwapchainProbe::done_in_slot(chain, av::icd::PresentDevice::slots, true);
  CHECK(chain.invariants_hold());
  SwapchainProbe::done_in_slot(chain, av::icd::PresentDevice::slots + 7u, false);
  CHECK(chain.invariants_hold());
  std::uint32_t second = 9u;
  CHECK_EQ(chain.acquire(0u, second), VK_SUCCESS);
  CHECK(second != index);
  CHECK(chain.invariants_hold()); }

TEST(swapchain, its_invariants_refuse_a_pacing_and_a_chained_image_that_are_unsound)
{ { SwapchainRig rig;
    CHECK_EQ(rig.initialize(rig.chain(), create_info()), VK_SUCCESS);
    CHECK(rig.chain().invariants_hold());
    SwapchainProbe::unsound_pacing(rig.chain());
    CHECK(!rig.chain().invariants_hold()); }
  { SwapchainRig rig;
    rig.chains().refuse_opens(false);
    rig.device().shared_object(0u, 0x7700u);
    rig.device().shared_object(0u, 0x7710u);
    CHECK_EQ(rig.initialize(rig.chain(), create_info()), VK_SUCCESS);
    CHECK(rig.chain().invariants_hold());
    SwapchainProbe::unsound_chained_image(rig.chain());
    CHECK(!rig.chain().invariants_hold()); } }

namespace {

constexpr std::uint64_t one_refresh = MockKmt::ticks_per_second / 60u;

VkSwapchainCreateInfoKHR immediate_info()
{ VkSwapchainCreateInfoKHR info = create_info();
  info.minImageCount = 3u;
  info.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
  return info; }

std::uint32_t present_painted(SwapchainRig &rig, Swapchain &chain, std::uint8_t red, std::uint32_t waits = 0u,
                              const VkSemaphore *semaphores = nullptr)
{ std::uint32_t index = 9u;
  CHECK_EQ(chain.acquire(0u, index), VK_SUCCESS);
  VkImage made[8] = {};
  std::uint32_t count = 8u;
  CHECK_EQ(chain.images(count, made), VK_SUCCESS);
  rig.host().paint(made[index], red, 0u, 0u, 0xFFu);
  CHECK_EQ(chain.present(rig.host().queue(), index, waits, semaphores), VK_SUCCESS);
  return index; }

}

TEST(swapchain, the_newest_present_a_pacing_refused_is_held_and_shown_at_the_next_blank)
{ SwapchainRig rig(true);
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, immediate_info()), VK_SUCCESS);
  rig.kmt().reads_clock(nullptr, 1000u);
  present_painted(rig, chain, 0x11u);
  present_painted(rig, chain, 0x22u);
  CHECK_EQ(chain.presented(), 1u);
  CHECK(chain.invariants_hold());
  rig.thread().step();
  CHECK_EQ(rig.windows().blits(), 1u);
  CHECK_EQ(rig.windows().last_blit_bytes()[0], 0x11u);
  rig.thread().step();
  CHECK_EQ(rig.windows().blits(), 1u);
  CHECK_EQ(chain.presented(), 1u);
  rig.kmt().reads_clock(nullptr, one_refresh);
  rig.thread().step();
  CHECK_EQ(chain.presented(), 2u);
  rig.thread().step();
  CHECK_EQ(rig.windows().blits(), 2u);
  CHECK_EQ(rig.windows().last_blit_bytes()[0], 0x22u);
  CHECK_EQ(rig.presents().queued(), 0u);
  CHECK(chain.invariants_hold());
  CHECK(rig.presents().invariants_hold()); }

TEST(swapchain, a_newer_refused_present_replaces_the_held_one_and_gives_its_image_back)
{ SwapchainRig rig(true);
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, immediate_info()), VK_SUCCESS);
  rig.kmt().reads_clock(nullptr, 1000u);
  present_painted(rig, chain, 0x11u);
  const std::uint32_t older = present_painted(rig, chain, 0x22u);
  const std::uint32_t newer = present_painted(rig, chain, 0x33u);
  CHECK(older != newer);
  std::uint32_t free_image = 9u;
  CHECK_EQ(chain.acquire(0u, free_image), VK_SUCCESS);
  CHECK_EQ(free_image, older);
  rig.thread().step();
  rig.kmt().reads_clock(nullptr, one_refresh);
  rig.thread().step();
  rig.thread().step();
  CHECK_EQ(rig.windows().blits(), 2u);
  CHECK_EQ(rig.windows().last_blit_bytes()[0], 0x33u);
  CHECK(chain.invariants_hold()); }

TEST(swapchain, a_refused_present_still_consumes_its_semaphores_on_the_queue)
{ SwapchainRig rig(true);
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, immediate_info()), VK_SUCCESS);
  rig.kmt().reads_clock(nullptr, 1000u);
  present_painted(rig, chain, 0x11u);
  const std::uint32_t before = rig.host().submits();
  const VkSemaphore waits[2] = {reinterpret_cast<VkSemaphore>(0x71u), reinterpret_cast<VkSemaphore>(0x72u)};
  present_painted(rig, chain, 0x22u, 2u, waits);
  CHECK_EQ(rig.host().submits(), before + 1u);
  CHECK_EQ(rig.host().last_wait_count(), 2u);
  CHECK(chain.invariants_hold()); }

TEST(swapchain, a_present_the_pacing_admits_supersedes_the_held_one)
{ SwapchainRig rig(true);
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, immediate_info()), VK_SUCCESS);
  rig.kmt().reads_clock(nullptr, 1000u);
  present_painted(rig, chain, 0x11u);
  present_painted(rig, chain, 0x22u);
  rig.kmt().reads_clock(nullptr, one_refresh);
  present_painted(rig, chain, 0x33u);
  rig.pump();
  CHECK_EQ(rig.windows().blits(), 2u);
  CHECK_EQ(rig.windows().last_blit_bytes()[0], 0x33u);
  CHECK_EQ(chain.presented(), 2u);
  CHECK(chain.invariants_hold());
  CHECK(rig.presents().invariants_hold()); }

TEST(swapchain, a_held_present_of_a_window_that_went_is_dropped_and_its_image_freed)
{ SwapchainRig rig(true);
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, immediate_info()), VK_SUCCESS);
  rig.kmt().reads_clock(nullptr, 1000u);
  present_painted(rig, chain, 0x11u);
  present_painted(rig, chain, 0x22u);
  rig.windows().resize(rig.window(), width + 1u, height);
  rig.thread().step();
  rig.kmt().reads_clock(nullptr, one_refresh);
  rig.thread().step();
  rig.thread().step();
  CHECK_EQ(rig.windows().blits(), 1u);
  CHECK_EQ(chain.presented(), 1u);
  CHECK(chain.invariants_hold()); }

TEST(swapchain, a_swapchain_that_goes_while_a_present_is_held_takes_the_held_image_with_it)
{ SwapchainRig rig(true);
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, immediate_info()), VK_SUCCESS);
  rig.kmt().reads_clock(nullptr, 1000u);
  present_painted(rig, chain, 0x11u);
  present_painted(rig, chain, 0x22u);
  rig.reset_first();
  rig.kmt().reads_clock(nullptr, one_refresh);
  rig.pump();
  CHECK_EQ(rig.windows().blits(), 0u);
  CHECK(rig.presents().invariants_hold()); }

TEST(swapchain, a_refused_present_without_a_present_thread_is_dropped_as_before)
{ SwapchainRig rig;
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, immediate_info()), VK_SUCCESS);
  rig.kmt().reads_clock(nullptr, 1000u);
  present_painted(rig, chain, 0x11u);
  present_painted(rig, chain, 0x22u);
  for (std::uint32_t each = 0u; each < 3u; each++) {
      std::uint32_t again = 9u;
      CHECK_EQ(chain.acquire(0u, again), VK_SUCCESS);
  }
  CHECK(chain.invariants_hold()); }

TEST(swapchain, an_acquire_waits_once_on_the_shown_event_for_the_whole_time_up_to_the_guard)
{ SwapchainRig rig(true);
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, create_info()), VK_SUCCESS);
  std::uint32_t first = 9u;
  std::uint32_t second = 9u;
  CHECK_EQ(chain.acquire(0u, first), VK_SUCCESS);
  CHECK_EQ(chain.acquire(0u, second), VK_SUCCESS);
  CHECK_EQ(chain.present(rig.host().queue(), first, 0u, nullptr), VK_SUCCESS);
  std::uint32_t none = 9u;
  const std::uint32_t waits = rig.shown_waits();
  CHECK_EQ(chain.acquire(30u * 1000000u, none), VK_TIMEOUT);
  CHECK_EQ(rig.shown_waits(), waits + 1u);
  CHECK_EQ(rig.shown_last_wait(), 30u);
  CHECK_EQ(chain.acquire(1000ull * 1000000ull, none), VK_TIMEOUT);
  CHECK_EQ(rig.shown_last_wait(), av::icd::VkPresentQueue::completion_guard_milliseconds); }

TEST(swapchain, replacing_and_dropping_the_held_present_tell_the_waiters)
{ SwapchainRig rig(true);
  Swapchain &chain = rig.chain();
  CHECK_EQ(rig.initialize(chain, immediate_info()), VK_SUCCESS);
  rig.kmt().reads_clock(nullptr, 1000u);
  present_painted(rig, chain, 0x11u);
  present_painted(rig, chain, 0x22u);
  const std::uint32_t raises = rig.shown_raises();
  present_painted(rig, chain, 0x33u);
  CHECK(rig.shown_raises() > raises);
  const std::uint32_t before_retire = rig.shown_raises();
  chain.retire();
  CHECK(rig.shown_raises() > before_retire); }
