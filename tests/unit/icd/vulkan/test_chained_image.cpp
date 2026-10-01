/* One swapchain's slot of its window's DXGI chain (docs/shared-surfaces.md,
 * step 5): the chain opened for the swapchain's channel order with rows top
 * first, its one slot opened on the device by the id the chain shares it as,
 * an image made over that memory as the host made the slot's texture, the
 * slot shown, and everything given back - by a retired swapchain without
 * closing its successor's chain. The device is a transport answering what a
 * test sets; the application's device is scripted. */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/scripted_vulkan_device.hpp"
#include "harness/icd/scripted_window_chains.hpp"
#include "harness/transport/mock_transport.hpp"
#include "icd/vulkan/chained_image.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/shared_object_answer.hpp"

#include <cstdint>
#include <cstring>

namespace av::icd {

/* Breaks each invariant a chained image keeps. */
struct ChainedImageProbe {
    static void forget_memory(ChainedImage &chained) { chained.memory_ = VK_NULL_HANDLE; }
    static void forget_identity(ChainedImage &chained) { chained.id_ = 0u; }
    static void forget_bytes(ChainedImage &chained) { chained.bytes_ = 0u; }
    static void forget_window(ChainedImage &chained) { chained.window_ = nullptr; }
};

}  // namespace av::icd

using av::CommandHeader;
using av::MockTransport;
using av::ScriptedVulkanDevice;
using av::ScriptedWindowChains;
using av::SharedObjectAnswer;
using av::icd::ChainedImage;
using av::icd::ChainedImageProbe;
using av::icd::SlotLayout;

namespace {

constexpr std::uint64_t luid = 0x0000000200001234ull;
constexpr VkExtent2D    extent = {64u, 32u};
/* The memory the device answers an open with: a handle the guest only passes
 * back. */
constexpr std::uint64_t opened_memory = 0x7700u;
constexpr std::uint64_t slot_bytes = std::uint64_t{64u} * 32u * 4u;

void *window() { return reinterpret_cast<void *>(0x10u); }

std::uint64_t u64_at(const std::uint8_t *bytes, std::size_t offset)
{ std::uint64_t value = 0u;
  std::memcpy(&value, bytes + offset, sizeof value);
  return value; }

/* The device's answer to every shared-object command from now on. */
void answer(MockTransport &wire, std::uint32_t status, std::uint64_t handle)
{ std::uint8_t reply[SharedObjectAnswer::encoded_size] = {};
  CHECK_STATUS(SharedObjectAnswer(status, handle).encode(reply, sizeof reply), av::Status::ok);
  wire.set_reply(reply, sizeof reply); }

/* A chained image over chains that open, a device that opens the slot, and a
 * scripted application device. */
class ChainedRig {
public:
    ChainedRig() : host_(), chains_(), wire_(), chained_(host_, host_.device(), chains_, wire_)
    { chains_.refuse_opens(false);
      answer(wire_, SharedObjectAnswer::done, opened_memory); }

    ScriptedVulkanDevice &host() { return host_; }
    ScriptedWindowChains &chains() { return chains_; }
    MockTransport        &wire() { return wire_; }
    ChainedImage         &chained() { return chained_; }

private:
    ScriptedVulkanDevice host_;
    ScriptedWindowChains chains_;
    MockTransport        wire_;
    ChainedImage         chained_;
};

}  // namespace

/* The slot is opened by the id the chain shares it as, at its bytes, and the
 * image over it is the host's own description of the slot's texture: linear
 * in the frame's channel order, mutable to its sRGB sibling, optimally tiled,
 * over dma-buf memory - bound to the memory the device named. */
TEST(chained_image, a_blue_first_frame_opens_a_blue_first_top_down_slot_made_as_the_host_made_it)
{ ChainedRig rig;
  ChainedImage &chained = rig.chained();
  CHECK(!chained.opened());
  CHECK(chained.slot() == VK_NULL_HANDLE);
  CHECK(chained.invariants_hold());
  CHECK(chained.open(window(), luid, VK_FORMAT_B8G8R8A8_SRGB, extent));
  CHECK(chained.opened());
  CHECK(chained.invariants_hold());
  CHECK_EQ(rig.chains().opens(), 1u);
  CHECK(rig.chains().open_window() == window());
  CHECK_EQ(rig.chains().last_luid(), luid);
  CHECK_EQ(rig.chains().last_width(), extent.width);
  CHECK_EQ(rig.chains().last_height(), extent.height);
  CHECK(rig.chains().last_layout() == SlotLayout::bgra_top_down);
  /* One slot: a present returns only once the chain has read it. */
  CHECK_EQ(rig.chains().last_slots(), 1u);
  CHECK_EQ(rig.wire().sends(), 1u);
  CHECK_EQ(rig.wire().last_command(), CommandHeader::open_shared_object_id);
  CHECK_EQ(u64_at(rig.wire().last_payload(), 0u), ScriptedWindowChains::first_id);
  CHECK_EQ(u64_at(rig.wire().last_payload(), 24u), slot_bytes);
  const ScriptedVulkanDevice::Described made = rig.host().described(chained.slot());
  CHECK_EQ(made.format, VK_FORMAT_B8G8R8A8_UNORM);
  CHECK_EQ(made.flags, VkImageCreateFlags{VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT});
  CHECK_EQ(made.tiling, VK_IMAGE_TILING_OPTIMAL);
  CHECK_EQ(made.external, VkExternalMemoryHandleTypeFlags{VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT});
  CHECK_EQ(made.view_formats, 2u);
  CHECK((made.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0u);
  CHECK(made.bound == reinterpret_cast<VkDeviceMemory>(opened_memory));
  CHECK_EQ(rig.host().images(), 1u);
  /* Opened once: a second open would lose the first slot. */
  CHECK(!chained.open(window(), luid, VK_FORMAT_B8G8R8A8_SRGB, extent));
  CHECK_EQ(rig.chains().opens(), 1u);
  chained.close();
  CHECK_EQ(rig.host().images(), 0u); }

TEST(chained_image, a_red_first_frame_opens_a_red_first_top_down_slot)
{ const VkFormat formats[] = {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB};
  for (const VkFormat format : formats) {
      ChainedRig rig;
      CHECK(rig.chained().open(window(), luid, format, extent));
      CHECK(rig.chains().last_layout() == SlotLayout::rgba_top_down);
      CHECK_EQ(rig.host().described(rig.chained().slot()).format, VK_FORMAT_R8G8B8A8_UNORM);
  }
  ChainedRig blue;
  CHECK(blue.chained().open(window(), luid, VK_FORMAT_B8G8R8A8_UNORM, extent));
  CHECK(blue.chains().last_layout() == SlotLayout::bgra_top_down); }

/* A format no slot takes, no window, and chains that will not open: nothing
 * is asked of the device, and nothing is held. */
TEST(chained_image, a_frame_no_chain_takes_is_refused_before_the_device_is_asked)
{ ChainedRig rig;
  CHECK(!rig.chained().open(window(), luid, VK_FORMAT_R16G16B16A16_SFLOAT, extent));
  CHECK_EQ(rig.chains().opens(), 0u);
  CHECK(!rig.chained().open(nullptr, luid, VK_FORMAT_B8G8R8A8_UNORM, extent));
  CHECK_EQ(rig.chains().opens(), 0u);
  rig.chains().refuse_opens(true);
  CHECK(!rig.chained().open(window(), luid, VK_FORMAT_B8G8R8A8_UNORM, extent));
  CHECK_EQ(rig.chains().opens(), 1u);
  CHECK_EQ(rig.wire().sends(), 0u);
  CHECK(!rig.chained().opened());
  CHECK(rig.chained().invariants_hold());
  /* A chain the chains refused is not theirs to close. */
  rig.chained().close();
  CHECK_EQ(rig.chains().closes(), 0u); }

/* A slot the device will not open, an image the device will not make, and
 * memory it will not bind: each leaves nothing behind - what was opened is
 * released, and the window's chain is closed so its frames go back to the
 * blit. */
TEST(chained_image, a_slot_that_cannot_be_opened_or_made_an_image_leaves_nothing_behind)
{ ChainedRig refused;
  answer(refused.wire(), SharedObjectAnswer::unknown_object, 0u);
  CHECK(!refused.chained().open(window(), luid, VK_FORMAT_B8G8R8A8_UNORM, extent));
  CHECK_EQ(refused.wire().sends(), 1u);
  CHECK_EQ(refused.chains().closes(), 1u);
  CHECK(refused.chains().last_closed() == window());
  CHECK_EQ(refused.host().images(), 0u);
  CHECK(refused.chained().invariants_hold());

  const char *const steps[] = {"vkCreateImage", "vkBindImageMemory"};
  for (const char *const step : steps) {
      ChainedRig rig;
      rig.host().refuse(step, 1u, VK_ERROR_OUT_OF_DEVICE_MEMORY);
      CHECK(!rig.chained().open(window(), luid, VK_FORMAT_B8G8R8A8_UNORM, extent));
      CHECK(!rig.chained().opened());
      CHECK_EQ(rig.host().images(), 0u);
      /* The open, then the release of what it opened. */
      CHECK_EQ(rig.wire().sends(), 2u);
      CHECK_EQ(rig.wire().last_command(), CommandHeader::release_shared_object_id);
      CHECK_EQ(u64_at(rig.wire().last_payload(), 0u), ScriptedWindowChains::first_id);
      CHECK_EQ(rig.chains().closes(), 1u);
      CHECK(rig.chained().invariants_hold());
  } }

TEST(chained_image, the_slot_is_shown_through_the_window_s_chain)
{ ChainedRig rig;
  CHECK(!rig.chained().show());
  CHECK_EQ(rig.chains().presents(), 0u);
  CHECK(rig.chained().open(window(), luid, VK_FORMAT_B8G8R8A8_UNORM, extent));
  CHECK(rig.chained().show());
  CHECK_EQ(rig.chains().presents(), 1u);
  CHECK_EQ(rig.chains().last_slot(), 0u);
  rig.chains().refuse_presents(true);
  CHECK(!rig.chained().show());
  CHECK_EQ(rig.chains().presents(), 1u); }

/* A retired swapchain gives its slot back and leaves the chain to its
 * successor; a destroyed one closes the chain too. */
TEST(chained_image, a_release_gives_the_slot_back_and_a_close_the_chain_as_well)
{ ChainedRig rig;
  CHECK(rig.chained().open(window(), luid, VK_FORMAT_B8G8R8A8_UNORM, extent));
  rig.chained().release();
  CHECK(!rig.chained().opened());
  CHECK(rig.chained().slot() == VK_NULL_HANDLE);
  CHECK_EQ(rig.host().images(), 0u);
  CHECK_EQ(rig.wire().last_command(), CommandHeader::release_shared_object_id);
  CHECK_EQ(u64_at(rig.wire().last_payload(), 24u), slot_bytes);
  CHECK_EQ(rig.chains().closes(), 0u);
  CHECK(!rig.chained().show());
  CHECK(rig.chained().invariants_hold());
  /* Nothing is released twice. */
  const std::uint32_t sent = rig.wire().sends();
  rig.chained().close();
  CHECK_EQ(rig.wire().sends(), sent);
  CHECK_EQ(rig.chains().closes(), 1u);
  CHECK(rig.chains().last_closed() == window());
  rig.chained().close();
  CHECK_EQ(rig.chains().closes(), 1u); }

TEST(chained_image, its_invariants_refuse_an_image_without_memory_and_memory_without_its_identity_or_window)
{ ChainedRig unbacked;
  CHECK(unbacked.chained().open(window(), luid, VK_FORMAT_B8G8R8A8_UNORM, extent));
  ChainedImageProbe::forget_memory(unbacked.chained());
  CHECK(!unbacked.chained().invariants_hold());

  ChainedRig nameless;
  CHECK(nameless.chained().open(window(), luid, VK_FORMAT_B8G8R8A8_UNORM, extent));
  ChainedImageProbe::forget_identity(nameless.chained());
  CHECK(!nameless.chained().invariants_hold());

  ChainedRig sizeless;
  CHECK(sizeless.chained().open(window(), luid, VK_FORMAT_B8G8R8A8_UNORM, extent));
  ChainedImageProbe::forget_bytes(sizeless.chained());
  CHECK(!sizeless.chained().invariants_hold());

  ChainedRig windowless;
  CHECK(windowless.chained().open(window(), luid, VK_FORMAT_B8G8R8A8_UNORM, extent));
  ChainedImageProbe::forget_window(windowless.chained());
  CHECK(!windowless.chained().invariants_hold()); }
