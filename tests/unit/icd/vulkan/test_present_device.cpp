#include "harness/framework/test_registry.hpp"
#include "harness/icd/scripted_vulkan_device.hpp"
#include "icd/vulkan/present_device.hpp"

#include <cstdint>

namespace av::icd {
struct PresentDeviceProbe {
    static void forget_image(PresentDevice &device) { device.made_[0].image_ = VK_NULL_HANDLE; }
    static void forget_memory(PresentDevice &device) { device.made_[0].memory_ = VK_NULL_HANDLE; }
    static void miscount(PresentDevice &device) { device.image_count_ = PresentDevice::max_images + 1u; }
    static void count_none(PresentDevice &device) { device.image_count_ = 0u; }
    static void forget_buffer_memory(PresentDevice &device) { device.buffer_memory_ = VK_NULL_HANDLE; }
    static void doubt_without_a_fence(PresentDevice &device, std::uint32_t slot) { device.doubted_[slot] = true; }
};

}

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

}

TEST(present_device, an_image_is_device_local_and_a_copy_source)
{ ScriptedVulkanDevice host;
  EntryLookup &entries = host;
  CHECK(entries.entry("vkCreateImage") != nullptr);
  CHECK(entries.entry("vkNotAnEntryPoint") == nullptr);
  PresentDevice device(entries, host.device(), host.physical());
  CHECK(device.invariants_hold());
  CHECK_EQ(device.images(), 0u);
  CHECK(device.readback(0u) == nullptr);
  const VkImage first = image_of(device, host);
  CHECK(device.invariants_hold());
  CHECK_EQ(device.images(), 1u);
  const VkImage second = image_of(device, host);
  CHECK(first != second);
  CHECK_EQ(device.images(), 2u);
  CHECK_EQ(host.memories(), 2u);
  const PresentImage made = device.made(0u);
  CHECK(made.image_ == first);
  CHECK(made.memory_ != VK_NULL_HANDLE);
  CHECK(device.made(1u).image_ == second);
  CHECK(device.made(2u).image_ == VK_NULL_HANDLE);
  CHECK(device.made(PresentDevice::max_images).memory_ == VK_NULL_HANDLE);
  device.release();
  CHECK(device.invariants_hold());
  CHECK_EQ(device.images(), 0u);
  CHECK_EQ(host.images(), 0u);
  CHECK_EQ(host.memories(), 0u); }

TEST(present_device, a_host_claiming_more_memory_types_than_vulkan_has_is_held_to_what_fits)
{ ScriptedVulkanDevice host;
  host.overcount_memory_types();
  PresentDevice device(host, host.device(), host.physical());
  const VkImage image = image_of(device, host);
  CHECK(image != VK_NULL_HANDLE);
  CHECK(device.invariants_hold()); }

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
  CHECK(device.readback(0u) != nullptr);
  CHECK_EQ(host.mappings(), 1u);
  CHECK_EQ(host.pools(), 1u);
  CHECK_EQ(host.pool_family(), 0u);
  CHECK_EQ(host.fences(), PresentDevice::slots);
  CHECK(device.invariants_hold());
  CHECK_EQ(device.make_readback(frame_bytes), VK_ERROR_INITIALIZATION_FAILED);
  device.release();
  CHECK(device.readback(0u) == nullptr);
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
      device.release();
      CHECK_EQ(host.memories(), 0u);
      CHECK_EQ(host.pools(), 0u);
      CHECK_EQ(host.fences(), 0u);
      CHECK_EQ(host.mappings(), 0u);
  }
  ScriptedVulkanDevice host;
  host.without_host_visible();
  PresentDevice device(host, host.device(), host.physical());
  CHECK_EQ(device.make_readback(frame_bytes), VK_ERROR_OUT_OF_DEVICE_MEMORY);
  CHECK(device.invariants_hold()); }

TEST(present_device, a_copy_moves_the_image_s_bytes_into_the_read_back_buffer)
{ ScriptedVulkanDevice host;
  PresentDevice device(host, host.device(), host.physical());
  CHECK_EQ(device.make_readback(frame_bytes), VK_SUCCESS);
  const VkImage image = image_of(device, host);
  host.paint(image, 0x11u, 0x22u, 0x33u, 0xFFu);
  CHECK_EQ(device.copy(host.queue(), image, extent, 0u, nullptr, 0u), VK_SUCCESS);
  CHECK_EQ(host.submits(), 1u);
  CHECK_EQ(host.copies(), 1u);
  CHECK_EQ(host.last_wait_count(), 0u);
  CHECK_EQ(device.readback(0u)[0], 0x11u);
  CHECK_EQ(device.readback(0u)[1], 0x22u);
  CHECK_EQ(device.readback(0u)[2], 0x33u);
  CHECK_EQ(device.readback(0u)[frame_bytes - 1u], 0xFFu);
  CHECK_EQ(host.barriers(), 2u);
  CHECK(host.barrier(0u).image == image);
  CHECK_EQ(host.barrier(0u).from, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  CHECK_EQ(host.barrier(0u).to, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  CHECK_EQ(host.barrier(1u).from, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  CHECK_EQ(host.barrier(1u).to, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  CHECK(device.invariants_hold()); }

TEST(present_device, a_copy_waits_on_the_present_s_own_semaphores)
{ ScriptedVulkanDevice host;
  PresentDevice device(host, host.device(), host.physical());
  CHECK_EQ(device.make_readback(frame_bytes), VK_SUCCESS);
  const VkImage image = image_of(device, host);
  const VkSemaphore waits[] = {reinterpret_cast<VkSemaphore>(0x900u), reinterpret_cast<VkSemaphore>(0x901u)};
  CHECK_EQ(device.copy(host.queue(), image, extent, 2u, waits, 0u), VK_SUCCESS);
  CHECK_EQ(host.last_wait_count(), 2u);
  CHECK(host.last_wait() == waits[0]);
  VkSemaphore many[PresentDevice::max_waits + 1u] = {};
  CHECK_EQ(device.copy(host.queue(), image, extent, PresentDevice::max_waits + 1u, many, 0u),
           VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK_EQ(host.submits(), 1u); }

TEST(present_device, a_copy_that_cannot_run_answers_the_device_s_error)
{ ScriptedVulkanDevice host;
  PresentDevice device(host, host.device(), host.physical());
  const VkImage before = reinterpret_cast<VkImage>(0x1u);
  CHECK_EQ(device.copy(host.queue(), before, extent, 0u, nullptr, 0u), VK_ERROR_INITIALIZATION_FAILED);
  CHECK_EQ(device.make_readback(frame_bytes), VK_SUCCESS);
  const VkImage image = image_of(device, host);
  CHECK_EQ(device.copy(host.queue(), image, extent, 0u, nullptr, PresentDevice::slots), VK_ERROR_INITIALIZATION_FAILED);
  const char *const steps[] = {"vkBeginCommandBuffer", "vkEndCommandBuffer", "vkQueueSubmit"};
  for (const char *const step : steps) {
      host.refuse(step, 1u, VK_ERROR_DEVICE_LOST);
      CHECK_EQ(device.copy(host.queue(), image, extent, 0u, nullptr, 0u), VK_ERROR_DEVICE_LOST);
      CHECK(device.invariants_hold());
  }
  CHECK(device.invariants_hold()); }

TEST(present_device, a_slot_s_copy_is_waited_for_by_finish_and_a_stalled_one_is_a_lost_device)
{ ScriptedVulkanDevice host;
  PresentDevice device(host, host.device(), host.physical());
  CHECK_EQ(device.finish(0u), VK_ERROR_INITIALIZATION_FAILED);
  CHECK_EQ(device.make_readback(frame_bytes), VK_SUCCESS);
  CHECK_EQ(device.finish(PresentDevice::slots), VK_ERROR_INITIALIZATION_FAILED);
  CHECK(device.fence(0u) != VK_NULL_HANDLE);
  CHECK(device.fence(1u) != VK_NULL_HANDLE);
  CHECK(device.fence(PresentDevice::slots) == VK_NULL_HANDLE);
  const VkImage image = image_of(device, host);
  CHECK_EQ(device.copy(host.queue(), image, extent, 0u, nullptr, 1u), VK_SUCCESS);
  CHECK_EQ(device.finish(1u), VK_SUCCESS);
  host.refuse("vkWaitForFences", 1u, VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK_EQ(device.finish(1u), VK_ERROR_OUT_OF_HOST_MEMORY);
  host.refuse(nullptr, 0u, VK_SUCCESS);
  host.stall_copy();
  CHECK_EQ(device.finish(1u), VK_ERROR_DEVICE_LOST);
  CHECK(device.invariants_hold()); }

TEST(present_device, a_doubted_slot_is_finished_before_it_is_copied_into_again)
{ ScriptedVulkanDevice host;
  PresentDevice device(host, host.device(), host.physical());
  CHECK_EQ(device.make_readback(frame_bytes), VK_SUCCESS);
  const VkImage image = image_of(device, host);
  CHECK_EQ(device.copy(host.queue(), image, extent, 0u, nullptr, 0u), VK_SUCCESS);
  device.doubt(0u);
  device.doubt(PresentDevice::slots);
  CHECK(device.invariants_hold());
  host.refuse("vkWaitForFences", 1u, VK_ERROR_OUT_OF_HOST_MEMORY);
  const std::uint32_t submits = host.submits();
  CHECK_EQ(device.copy(host.queue(), image, extent, 0u, nullptr, 0u), VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK_EQ(host.submits(), submits);
  host.refuse(nullptr, 0u, VK_SUCCESS);
  CHECK_EQ(device.copy(host.queue(), image, extent, 0u, nullptr, 0u), VK_SUCCESS);
  CHECK_EQ(host.submits(), submits + 1u);
  CHECK(device.invariants_hold()); }

TEST(present_device, the_two_slots_read_back_into_their_own_halves_of_the_buffer)
{ ScriptedVulkanDevice host;
  PresentDevice device(host, host.device(), host.physical());
  CHECK_EQ(device.make_readback(frame_bytes), VK_SUCCESS);
  CHECK(device.readback(PresentDevice::slots) == nullptr);
  CHECK(device.readback(1u) == device.readback(0u) + frame_bytes);
  const VkImage first = image_of(device, host);
  const VkImage second = image_of(device, host);
  host.paint(first, 0x11u, 0x12u, 0x13u, 0x14u);
  host.paint(second, 0x21u, 0x22u, 0x23u, 0x24u);
  CHECK_EQ(device.copy(host.queue(), first, extent, 0u, nullptr, 0u), VK_SUCCESS);
  CHECK_EQ(device.copy(host.queue(), second, extent, 0u, nullptr, 1u), VK_SUCCESS);
  CHECK_EQ(device.finish(0u), VK_SUCCESS);
  CHECK_EQ(device.finish(1u), VK_SUCCESS);
  CHECK_EQ(device.readback(0u)[0], 0x11u);
  CHECK_EQ(device.readback(0u)[frame_bytes - 1u], 0x14u);
  CHECK_EQ(device.readback(1u)[0], 0x21u);
  CHECK_EQ(device.readback(1u)[frame_bytes - 1u], 0x24u);
  CHECK(device.invariants_hold()); }

TEST(present_device, a_skip_only_waits_the_presents_semaphores_and_copies_nothing)
{ ScriptedVulkanDevice host;
  PresentDevice device(host, host.device(), host.physical());
  CHECK_EQ(device.skip(host.queue(), 0u, nullptr), VK_SUCCESS);
  CHECK_EQ(host.submits(), 0u);
  const VkSemaphore waits[] = {reinterpret_cast<VkSemaphore>(0x900u), reinterpret_cast<VkSemaphore>(0x901u)};
  CHECK_EQ(device.skip(host.queue(), 2u, waits), VK_SUCCESS);
  CHECK_EQ(host.submits(), 1u);
  CHECK_EQ(host.last_wait_count(), 2u);
  CHECK(host.last_wait() == waits[0]);
  CHECK_EQ(host.copies(), 0u);
  VkSemaphore many[PresentDevice::max_waits + 1u] = {};
  CHECK_EQ(device.skip(host.queue(), PresentDevice::max_waits + 1u, many), VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK_EQ(host.submits(), 1u); }

TEST(present_device, the_commands_a_copy_runs_on_are_made_once_without_a_read_back_buffer)
{ ScriptedVulkanDevice host;
  PresentDevice device(host, host.device(), host.physical());
  CHECK_EQ(device.make_commands(), VK_SUCCESS);
  CHECK(device.readback(0u) == nullptr);
  CHECK_EQ(host.mappings(), 0u);
  CHECK_EQ(host.pools(), 1u);
  CHECK_EQ(host.pool_family(), 0u);
  CHECK_EQ(host.fences(), PresentDevice::slots);
  CHECK(device.invariants_hold());
  CHECK_EQ(device.make_commands(), VK_ERROR_INITIALIZATION_FAILED);
  CHECK_EQ(host.pools(), 1u);
  CHECK_EQ(device.make_readback(frame_bytes), VK_ERROR_INITIALIZATION_FAILED);
  device.release();
  CHECK_EQ(host.pools(), 0u);
  CHECK_EQ(host.fences(), 0u);
  CHECK_EQ(host.memories(), 0u); }

TEST(present_device, a_copy_into_a_slot_moves_the_image_s_bytes_and_releases_the_slot_to_its_reader)
{ ScriptedVulkanDevice host;
  PresentDevice device(host, host.device(), host.physical());
  CHECK_EQ(device.make_commands(), VK_SUCCESS);
  const VkImage image = image_of(device, host);
  const VkImage slot = image_of(device, host);
  host.paint(image, 0x11u, 0x22u, 0x33u, 0x44u);
  host.paint(slot, 0u, 0u, 0u, 0u);
  const VkSemaphore waits[] = {reinterpret_cast<VkSemaphore>(0x900u)};
  CHECK_EQ(device.copy_into(host.queue(), image, slot, extent, 1u, waits, 0u), VK_SUCCESS);
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
  CHECK_EQ(device.copy_into(host.queue(), before, before, extent, 0u, nullptr, 0u), VK_ERROR_INITIALIZATION_FAILED);
  CHECK_EQ(device.make_commands(), VK_SUCCESS);
  const VkImage image = image_of(device, host);
  const VkImage slot = image_of(device, host);
  const char *const steps[] = {"vkBeginCommandBuffer", "vkEndCommandBuffer", "vkQueueSubmit"};
  for (const char *const step : steps) {
      host.refuse(step, 1u, VK_ERROR_DEVICE_LOST);
      CHECK_EQ(device.copy_into(host.queue(), image, slot, extent, 0u, nullptr, 0u), VK_ERROR_DEVICE_LOST);
  }
  host.refuse(nullptr, 0u, VK_SUCCESS);
  VkSemaphore many[PresentDevice::max_waits + 1u] = {};
  CHECK_EQ(device.copy_into(host.queue(), image, slot, extent, PresentDevice::max_waits + 1u, many, 0u),
           VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK(device.invariants_hold()); }

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
  PresentDeviceProbe::count_none(miscounted);
  CHECK(miscounted.invariants_hold());

  ScriptedVulkanDevice fourth;
  PresentDevice mapped(fourth, fourth.device(), fourth.physical());
  CHECK_EQ(mapped.make_readback(frame_bytes), VK_SUCCESS);
  PresentDeviceProbe::forget_buffer_memory(mapped);
  CHECK(!mapped.invariants_hold()); }

TEST(present_device, a_copy_to_the_readback_needs_the_readback_buffer_and_a_doubted_slot_needs_its_fence)
{ ScriptedVulkanDevice host;
  PresentDevice device(host, host.device(), host.physical());
  CHECK_EQ(device.make_commands(), VK_SUCCESS);
  const VkImage image = image_of(device, host);
  CHECK_EQ(device.copy(host.queue(), image, extent, 0u, nullptr, 0u), VK_ERROR_INITIALIZATION_FAILED);
  CHECK_EQ(host.submits(), 0u);
  CHECK(device.invariants_hold());

  ScriptedVulkanDevice other;
  PresentDevice unfenced(other, other.device(), other.physical());
  PresentDeviceProbe::doubt_without_a_fence(unfenced, 1u);
  CHECK(!unfenced.invariants_hold());
  unfenced.release(); }
