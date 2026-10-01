/* The host objects behind a swapchain (feature 046, research.md §1): images in
 * device-local memory, a read-back buffer the process maps, and the copy of a
 * presented image into it - with every refusal the device can answer on the
 * way, each leaving nothing made. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/scripted_vulkan_device.hpp"
#include "icd/vulkan/present_device.hpp"

#include <cstdint>

namespace av::icd {

/* Reaches into a device to break each invariant it keeps. */
struct PresentDeviceProbe {
    static void forget_image(PresentDevice &device) { device.made_[0].image_ = VK_NULL_HANDLE; }
    static void forget_memory(PresentDevice &device) { device.made_[0].memory_ = VK_NULL_HANDLE; }
    static void miscount(PresentDevice &device) { device.image_count_ = PresentDevice::max_images + 1u; }
    static void forget_buffer_memory(PresentDevice &device) { device.buffer_memory_ = VK_NULL_HANDLE; }
};

}  // namespace av::icd

using av::ScriptedVulkanDevice;
using av::icd::EntryLookup;
using av::icd::PresentDevice;
using av::icd::PresentDeviceProbe;
using av::icd::PresentImage;

namespace {

constexpr VkExtent2D extent = {4u, 2u};
constexpr VkDeviceSize frame_bytes = 4u * 2u * 4u;

VkImage image_of(PresentDevice &device, ScriptedVulkanDevice &host)
{ VkImage image = VK_NULL_HANDLE;
  CHECK_EQ(device.make_image(VK_FORMAT_B8G8R8A8_UNORM, extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, image), VK_SUCCESS);
  CHECK(image != VK_NULL_HANDLE);
  CHECK_EQ(host.images(), device.images());
  return image; }

}  // namespace

TEST(present_device, an_image_is_device_local_and_a_copy_source)
{ ScriptedVulkanDevice host;
  /* The functions it calls are found through the lookup, whoever answers it. */
  EntryLookup &entries = host;
  CHECK(entries.entry("vkCreateImage") != nullptr);
  CHECK(entries.entry("vkNotAnEntryPoint") == nullptr);
  PresentDevice device(entries, host.device(), host.physical());
  CHECK(device.invariants_hold());
  CHECK_EQ(device.images(), 0u);
  CHECK(device.readback() == nullptr);
  const VkImage first = image_of(device, host);
  CHECK(device.invariants_hold());
  CHECK_EQ(device.images(), 1u);
  const VkImage second = image_of(device, host);
  CHECK(first != second);
  CHECK_EQ(device.images(), 2u);
  /* Its memory is bound: two images, two allocations. */
  CHECK_EQ(host.memories(), 2u);
  const PresentImage made = device.made(0u);
  CHECK(made.image_ == first);
  CHECK(made.memory_ != VK_NULL_HANDLE);
  CHECK(device.made(1u).image_ == second);
  /* Nothing past the count. */
  CHECK(device.made(2u).image_ == VK_NULL_HANDLE);
  CHECK(device.made(PresentDevice::max_images).memory_ == VK_NULL_HANDLE);
  device.release();
  CHECK(device.invariants_hold());
  CHECK_EQ(device.images(), 0u);
  CHECK_EQ(host.images(), 0u);
  CHECK_EQ(host.memories(), 0u); }

/* A host that claims more memory types than Vulkan has: only the types there
 * is room for are read, and the image is still made. */
TEST(present_device, a_host_claiming_more_memory_types_than_vulkan_has_is_held_to_what_fits)
{ ScriptedVulkanDevice host;
  host.overcount_memory_types();
  PresentDevice device(host, host.device(), host.physical());
  const VkImage image = image_of(device, host);
  CHECK(image != VK_NULL_HANDLE);
  CHECK(device.invariants_hold()); }

/* A device with no device-local type still presents: the image goes in
 * whatever type its requirements allow. */
TEST(present_device, an_image_falls_back_to_any_memory_type_the_device_allows)
{ ScriptedVulkanDevice host;
  host.without_device_local();
  PresentDevice device(host, host.device(), host.physical());
  const VkImage image = image_of(device, host);
  CHECK(image != VK_NULL_HANDLE);
  CHECK_EQ(host.memories(), 1u);
  CHECK(device.invariants_hold()); }

TEST(present_device, more_images_than_a_swapchain_may_have_are_refused)
{ ScriptedVulkanDevice host;
  PresentDevice device(host, host.device(), host.physical());
  for (std::uint32_t index = 0u; index < PresentDevice::max_images; index++) static_cast<void>(image_of(device, host));
  VkImage past = reinterpret_cast<VkImage>(0x1u);
  CHECK_EQ(device.make_image(VK_FORMAT_B8G8R8A8_UNORM, extent, 0u, past), VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK(past == VK_NULL_HANDLE);
  CHECK_EQ(device.images(), PresentDevice::max_images);
  CHECK(device.invariants_hold()); }

/* Each step of making an image can fail, and each leaves nothing behind: no
 * image counted, no memory allocated, and the handle null. */
TEST(present_device, an_image_that_cannot_be_made_leaves_nothing_made)
{ struct Refusal {
      const char *name;
      VkResult    code;
      VkResult    answer;
      std::uint32_t memories;
  };
  const Refusal refusals[] = {
      {"vkCreateImage", VK_ERROR_OUT_OF_DEVICE_MEMORY, VK_ERROR_OUT_OF_DEVICE_MEMORY, 0u},
      {"vkAllocateMemory", VK_ERROR_OUT_OF_DEVICE_MEMORY, VK_ERROR_OUT_OF_DEVICE_MEMORY, 0u},
      {"vkBindImageMemory", VK_ERROR_OUT_OF_HOST_MEMORY, VK_ERROR_OUT_OF_HOST_MEMORY, 0u},
  };
  for (const Refusal &refusal : refusals) {
      ScriptedVulkanDevice host;
      host.refuse(refusal.name, 1u, refusal.code);
      PresentDevice device(host, host.device(), host.physical());
      VkImage image = reinterpret_cast<VkImage>(0x1u);
      CHECK_EQ(device.make_image(VK_FORMAT_B8G8R8A8_UNORM, extent, 0u, image), refusal.answer);
      CHECK(image == VK_NULL_HANDLE);
      CHECK_EQ(device.images(), 0u);
      CHECK_EQ(host.images(), 0u);
      CHECK_EQ(host.memories(), refusal.memories);
      CHECK(device.invariants_hold());
  }
  /* No memory type the image may live in at all. */
  ScriptedVulkanDevice host;
  host.allow_no_memory_type();
  PresentDevice device(host, host.device(), host.physical());
  VkImage image = reinterpret_cast<VkImage>(0x1u);
  CHECK_EQ(device.make_image(VK_FORMAT_B8G8R8A8_UNORM, extent, 0u, image), VK_ERROR_OUT_OF_DEVICE_MEMORY);
  CHECK(image == VK_NULL_HANDLE);
  CHECK_EQ(host.images(), 0u);
  CHECK(device.invariants_hold()); }

TEST(present_device, the_read_back_buffer_is_host_visible_mapped_and_made_once)
{ ScriptedVulkanDevice host;
  PresentDevice device(host, host.device(), host.physical());
  CHECK_EQ(device.make_readback(frame_bytes), VK_SUCCESS);
  CHECK(device.readback() != nullptr);
  CHECK_EQ(host.mappings(), 1u);
  CHECK_EQ(host.pools(), 1u);
  /* The queue family a present's copy runs on (a recorded limitation). */
  CHECK_EQ(host.pool_family(), 0u);
  CHECK_EQ(host.fences(), 1u);
  CHECK(device.invariants_hold());
  /* A second read-back buffer would leak the first. */
  CHECK_EQ(device.make_readback(frame_bytes), VK_ERROR_INITIALIZATION_FAILED);
  device.release();
  CHECK(device.readback() == nullptr);
  CHECK_EQ(host.mappings(), 0u);
  CHECK_EQ(host.pools(), 0u);
  CHECK_EQ(host.fences(), 0u);
  CHECK(device.invariants_hold()); }

TEST(present_device, a_read_back_buffer_that_cannot_be_made_answers_the_device_s_own_error)
{ const char *const steps[] = {"vkCreateBuffer",       "vkAllocateMemory",         "vkBindBufferMemory",
                               "vkMapMemory",          "vkCreateCommandPool",      "vkAllocateCommandBuffers",
                               "vkCreateFence"};
  for (const char *const step : steps) {
      ScriptedVulkanDevice host;
      host.refuse(step, 1u, VK_ERROR_OUT_OF_DEVICE_MEMORY);
      PresentDevice device(host, host.device(), host.physical());
      CHECK_EQ(device.make_readback(frame_bytes), VK_ERROR_OUT_OF_DEVICE_MEMORY);
      CHECK(device.invariants_hold());
      /* Whatever was made before the refusal goes with the release. */
      device.release();
      CHECK_EQ(host.memories(), 0u);
      CHECK_EQ(host.pools(), 0u);
      CHECK_EQ(host.fences(), 0u);
      CHECK_EQ(host.mappings(), 0u);
  }
  /* No host-visible memory type: the frame could never be read back. */
  ScriptedVulkanDevice host;
  host.without_host_visible();
  PresentDevice device(host, host.device(), host.physical());
  CHECK_EQ(device.make_readback(frame_bytes), VK_ERROR_OUT_OF_DEVICE_MEMORY);
  CHECK(device.invariants_hold()); }

/* The copy is what puts the frame in guest memory: the image's bytes, through
 * the barriers that make it a copy source and a present source again. */
TEST(present_device, a_copy_moves_the_image_s_bytes_into_the_read_back_buffer)
{ ScriptedVulkanDevice host;
  PresentDevice device(host, host.device(), host.physical());
  CHECK_EQ(device.make_readback(frame_bytes), VK_SUCCESS);
  const VkImage image = image_of(device, host);
  host.paint(image, 0x11u, 0x22u, 0x33u, 0xFFu);
  CHECK_EQ(device.copy(host.queue(), image, extent, 0u, nullptr), VK_SUCCESS);
  CHECK_EQ(host.submits(), 1u);
  CHECK_EQ(host.copies(), 1u);
  CHECK_EQ(host.last_wait_count(), 0u);
  CHECK_EQ(device.readback()[0], 0x11u);
  CHECK_EQ(device.readback()[1], 0x22u);
  CHECK_EQ(device.readback()[2], 0x33u);
  CHECK_EQ(device.readback()[frame_bytes - 1u], 0xFFu);
  /* PRESENT_SRC to TRANSFER_SRC and back, on that image. */
  CHECK_EQ(host.barriers(), 2u);
  CHECK(host.barrier(0u).image == image);
  CHECK_EQ(host.barrier(0u).from, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  CHECK_EQ(host.barrier(0u).to, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  CHECK_EQ(host.barrier(1u).from, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  CHECK_EQ(host.barrier(1u).to, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  CHECK(device.invariants_hold()); }

/* A present's semaphores are the host's: the copy waits on them where the host
 * driver would have presented, so it never reads a half-drawn frame. */
TEST(present_device, a_copy_waits_on_the_present_s_own_semaphores)
{ ScriptedVulkanDevice host;
  PresentDevice device(host, host.device(), host.physical());
  CHECK_EQ(device.make_readback(frame_bytes), VK_SUCCESS);
  const VkImage image = image_of(device, host);
  const VkSemaphore waits[] = {reinterpret_cast<VkSemaphore>(0x900u), reinterpret_cast<VkSemaphore>(0x901u)};
  CHECK_EQ(device.copy(host.queue(), image, extent, 2u, waits), VK_SUCCESS);
  CHECK_EQ(host.last_wait_count(), 2u);
  CHECK(host.last_wait() == waits[0]);
  /* More semaphores than a present ever waits on: refused rather than
   * overrunning the stage array. */
  VkSemaphore many[PresentDevice::max_waits + 1u] = {};
  CHECK_EQ(device.copy(host.queue(), image, extent, PresentDevice::max_waits + 1u, many),
           VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK_EQ(host.submits(), 1u); }

TEST(present_device, a_copy_that_cannot_run_answers_the_device_s_error_and_a_stalled_one_is_a_lost_device)
{ ScriptedVulkanDevice host;
  PresentDevice device(host, host.device(), host.physical());
  const VkImage before = reinterpret_cast<VkImage>(0x1u);
  /* Nothing to copy into: no read-back buffer was made. */
  CHECK_EQ(device.copy(host.queue(), before, extent, 0u, nullptr), VK_ERROR_INITIALIZATION_FAILED);
  CHECK_EQ(device.make_readback(frame_bytes), VK_SUCCESS);
  const VkImage image = image_of(device, host);
  const char *const steps[] = {"vkBeginCommandBuffer", "vkEndCommandBuffer", "vkQueueSubmit", "vkResetFences"};
  for (const char *const step : steps) {
      host.refuse(step, 1u, VK_ERROR_DEVICE_LOST);
      CHECK_EQ(device.copy(host.queue(), image, extent, 0u, nullptr), VK_ERROR_DEVICE_LOST);
      CHECK(device.invariants_hold());
  }
  host.refuse("vkWaitForFences", 1u, VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK_EQ(device.copy(host.queue(), image, extent, 0u, nullptr), VK_ERROR_OUT_OF_HOST_MEMORY);
  /* A copy the host never finishes: the device is gone, not the frame late. */
  host.refuse(nullptr, 0u, VK_SUCCESS);
  host.stall_copy();
  CHECK_EQ(device.copy(host.queue(), image, extent, 0u, nullptr), VK_ERROR_DEVICE_LOST);
  CHECK(device.invariants_hold()); }

/* A swapchain shown through its window's chain reads nothing back: it needs
 * only what a copy runs on, made once. */
TEST(present_device, the_commands_a_copy_runs_on_are_made_once_without_a_read_back_buffer)
{ ScriptedVulkanDevice host;
  PresentDevice device(host, host.device(), host.physical());
  CHECK_EQ(device.make_commands(), VK_SUCCESS);
  CHECK(device.readback() == nullptr);
  CHECK_EQ(host.mappings(), 0u);
  CHECK_EQ(host.pools(), 1u);
  CHECK_EQ(host.pool_family(), 0u);
  CHECK_EQ(host.fences(), 1u);
  CHECK(device.invariants_hold());
  CHECK_EQ(device.make_commands(), VK_ERROR_INITIALIZATION_FAILED);
  CHECK_EQ(host.pools(), 1u);
  /* A read-back buffer made after them makes no second set. */
  CHECK_EQ(device.make_readback(frame_bytes), VK_ERROR_INITIALIZATION_FAILED);
  device.release();
  CHECK_EQ(host.pools(), 0u);
  CHECK_EQ(host.fences(), 0u);
  CHECK_EQ(host.memories(), 0u); }

/* Into a slot of the window's chain: the image's bytes as they are, the image
 * a present source again after, and the slot - whatever it held discarded -
 * handed to whichever device reads it next. */
TEST(present_device, a_copy_into_a_slot_moves_the_image_s_bytes_and_releases_the_slot_to_its_reader)
{ ScriptedVulkanDevice host;
  PresentDevice device(host, host.device(), host.physical());
  CHECK_EQ(device.make_commands(), VK_SUCCESS);
  const VkImage image = image_of(device, host);
  const VkImage slot = image_of(device, host);
  host.paint(image, 0x11u, 0x22u, 0x33u, 0x44u);
  host.paint(slot, 0u, 0u, 0u, 0u);
  const VkSemaphore waits[] = {reinterpret_cast<VkSemaphore>(0x900u)};
  CHECK_EQ(device.copy_into(host.queue(), image, slot, extent, 1u, waits), VK_SUCCESS);
  CHECK_EQ(host.submits(), 1u);
  CHECK_EQ(host.last_wait_count(), 1u);
  CHECK(host.last_wait() == waits[0]);
  CHECK_EQ(host.image_copies(), 1u);
  CHECK_EQ(host.copies(), 0u);
  const std::uint8_t *const copied = host.bytes(slot);
  CHECK_EQ(copied[0], 0x11u);
  CHECK_EQ(copied[2], 0x33u);
  CHECK_EQ(copied[frame_bytes - 1u], 0x44u);
  CHECK_EQ(host.barriers(), 4u);
  CHECK(host.barrier(0u).image == image);
  CHECK_EQ(host.barrier(0u).from, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  CHECK_EQ(host.barrier(0u).to, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  CHECK(host.barrier(1u).image == slot);
  CHECK_EQ(host.barrier(1u).from, VK_IMAGE_LAYOUT_UNDEFINED);
  CHECK_EQ(host.barrier(1u).to, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  CHECK_EQ(host.barrier(1u).to_family, VK_QUEUE_FAMILY_IGNORED);
  CHECK(host.barrier(2u).image == image);
  CHECK_EQ(host.barrier(2u).to, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  CHECK(host.barrier(3u).image == slot);
  CHECK_EQ(host.barrier(3u).from, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  CHECK_EQ(host.barrier(3u).to, VK_IMAGE_LAYOUT_GENERAL);
  CHECK_EQ(host.barrier(3u).to_family, VK_QUEUE_FAMILY_EXTERNAL);
  CHECK(device.invariants_hold()); }

TEST(present_device, a_copy_into_a_slot_that_cannot_run_answers_the_device_s_error)
{ ScriptedVulkanDevice host;
  PresentDevice device(host, host.device(), host.physical());
  const VkImage before = reinterpret_cast<VkImage>(0x1u);
  /* Nothing to run it on: no commands were made. */
  CHECK_EQ(device.copy_into(host.queue(), before, before, extent, 0u, nullptr), VK_ERROR_INITIALIZATION_FAILED);
  CHECK_EQ(device.make_commands(), VK_SUCCESS);
  const VkImage image = image_of(device, host);
  const VkImage slot = image_of(device, host);
  const char *const steps[] = {"vkBeginCommandBuffer", "vkEndCommandBuffer", "vkQueueSubmit"};
  for (const char *const step : steps) {
      host.refuse(step, 1u, VK_ERROR_DEVICE_LOST);
      CHECK_EQ(device.copy_into(host.queue(), image, slot, extent, 0u, nullptr), VK_ERROR_DEVICE_LOST);
  }
  host.refuse(nullptr, 0u, VK_SUCCESS);
  VkSemaphore many[PresentDevice::max_waits + 1u] = {};
  CHECK_EQ(device.copy_into(host.queue(), image, slot, extent, PresentDevice::max_waits + 1u, many),
           VK_ERROR_OUT_OF_HOST_MEMORY);
  host.stall_copy();
  CHECK_EQ(device.copy_into(host.queue(), image, slot, extent, 0u, nullptr), VK_ERROR_DEVICE_LOST);
  CHECK(device.invariants_hold()); }

/* Each invariant, broken: an image without its memory, memory without its
 * image, a count past the images, and a mapping over memory that is gone. */
TEST(present_device, its_invariants_refuse_an_image_without_memory_a_miscount_and_a_stray_mapping)
{ ScriptedVulkanDevice host;
  PresentDevice device(host, host.device(), host.physical());
  CHECK_EQ(device.make_readback(frame_bytes), VK_SUCCESS);
  static_cast<void>(image_of(device, host));
  CHECK(device.invariants_hold());
  PresentDeviceProbe::forget_image(device);
  CHECK(!device.invariants_hold());

  ScriptedVulkanDevice second;
  PresentDevice without_memory(second, second.device(), second.physical());
  static_cast<void>(image_of(without_memory, second));
  PresentDeviceProbe::forget_memory(without_memory);
  CHECK(!without_memory.invariants_hold());

  ScriptedVulkanDevice third;
  PresentDevice miscounted(third, third.device(), third.physical());
  PresentDeviceProbe::miscount(miscounted);
  CHECK(!miscounted.invariants_hold());

  ScriptedVulkanDevice fourth;
  PresentDevice mapped(fourth, fourth.device(), fourth.physical());
  CHECK_EQ(mapped.make_readback(frame_bytes), VK_SUCCESS);
  PresentDeviceProbe::forget_buffer_memory(mapped);
  CHECK(!mapped.invariants_hold()); }
