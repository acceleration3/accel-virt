#include "guest/probe/probe_device.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/scripted_vulkan.hpp"

#include <cstdint>

namespace av {

struct ProbeDeviceProbe {
    static void orphan_device(ProbeDevice &device) { device.physical_device_ = VK_NULL_HANDLE; }
    static void orphan_list(ProbeDevice &device) { device.instance_ = VK_NULL_HANDLE; }
    static void orphan_choice(ProbeDevice &device) { device.listed_count_ = 0u; }
    static void queueless(ProbeDevice &device) { device.queue_ = VK_NULL_HANDLE; }
    static void long_list(ProbeDevice &device) { device.listed_count_ = ProbeDevice::max_physical_devices + 1u; }
    static void far_family(ProbeDevice &device) { device.family_ = ProbeDevice::max_queue_families; }
    static void forget(ProbeDevice &device) { device.device_ = VK_NULL_HANDLE; device.instance_ = VK_NULL_HANDLE; }
};

}

using av::ProbeDevice;
using av::ProbeDeviceProbe;
using av::ScriptedVulkan;
using Call = av::ScriptedVulkan::Call;

namespace {

constexpr std::uint64_t luid = 0x0000000300000042ull;

}

TEST(probe_device, a_device_is_made_on_the_device_with_the_luid)
{ ScriptedVulkan vulkan;
  vulkan.set_physical_devices(2u);
  vulkan.set_luid(1u, luid, true);
  vulkan.set_queue_families(3u, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);
  { ProbeDevice device(vulkan);
    CHECK(device.invariants_hold());
    CHECK_EQ(device.make_instance(), VK_SUCCESS);
    CHECK_EQ(vulkan.instance_api_version(), (1u << 22u) | (3u << 12u));
    CHECK(device.invariants_hold());
    CHECK_EQ(device.list_physical_devices(), VK_SUCCESS);
    CHECK_EQ(device.listed(), 2u);
    CHECK(device.invariants_hold());
    CHECK(device.choose(luid));
    CHECK(device.invariants_hold());
    CHECK(device.choose_family());
    CHECK_EQ(device.family(), 0u);
    CHECK_EQ(device.make_device(), VK_SUCCESS);
    CHECK(device.invariants_hold());
    CHECK_EQ(vulkan.device_parent(), 1u);
    CHECK(device.device() != VK_NULL_HANDLE);
    CHECK(device.queue() != VK_NULL_HANDLE);
    CHECK(&device.vulkan() == &vulkan);
    CHECK_EQ(vulkan.live(), 2u); }
  CHECK_EQ(vulkan.live(), 0u);
  CHECK_EQ(vulkan.misuses(), 0u); }

TEST(probe_device, an_unmade_device_destroys_nothing)
{ ScriptedVulkan vulkan;
  vulkan.fail(Call::create_instance, 1u, VK_ERROR_INITIALIZATION_FAILED);
  { ProbeDevice device(vulkan);
    CHECK_EQ(device.make_instance(), VK_ERROR_INITIALIZATION_FAILED);
    CHECK(device.invariants_hold()); }
  CHECK_EQ(vulkan.misuses(), 0u); }

TEST(probe_device, a_refused_list_is_passed_on_and_an_incomplete_one_is_kept_as_far_as_the_room)
{ ScriptedVulkan vulkan;
  ProbeDevice device(vulkan);
  CHECK_EQ(device.make_instance(), VK_SUCCESS);
  vulkan.fail(Call::enumerate_physical_devices, 1u, VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK_EQ(device.list_physical_devices(), VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK_EQ(device.listed(), 0u);
  CHECK(!device.choose(1u));
  vulkan.set_physical_devices(ScriptedVulkan::max_physical_devices);
  CHECK_EQ(device.list_physical_devices(), VK_SUCCESS);
  CHECK_EQ(device.listed(), ProbeDevice::max_physical_devices);
  CHECK(device.invariants_hold()); }

TEST(probe_device, only_a_valid_equal_luid_is_chosen)
{ ScriptedVulkan vulkan;
  vulkan.set_physical_devices(2u);
  vulkan.set_luid(0u, luid, false);
  vulkan.set_luid(1u, luid ^ (std::uint64_t{1} << 40u), true);
  ProbeDevice device(vulkan);
  CHECK_EQ(device.make_instance(), VK_SUCCESS);
  CHECK_EQ(device.list_physical_devices(), VK_SUCCESS);
  CHECK(!device.choose(luid));
  CHECK(device.physical_device() == VK_NULL_HANDLE);
  CHECK(device.invariants_hold()); }

TEST(probe_device, a_family_that_only_computes_is_passed_over)
{ ScriptedVulkan vulkan;
  vulkan.set_queue_families(2u, VK_QUEUE_GRAPHICS_BIT);
  ProbeDevice device(vulkan);
  CHECK_EQ(device.make_instance(), VK_SUCCESS);
  CHECK_EQ(device.list_physical_devices(), VK_SUCCESS);
  CHECK(device.choose(1u));
  CHECK(!device.choose_family());
  CHECK(device.invariants_hold()); }

TEST(probe_device, a_refused_device_leaves_no_queue)
{ ScriptedVulkan vulkan;
  vulkan.fail(Call::create_device, 1u, VK_ERROR_DEVICE_LOST);
  ProbeDevice device(vulkan);
  CHECK_EQ(device.make_instance(), VK_SUCCESS);
  CHECK_EQ(device.list_physical_devices(), VK_SUCCESS);
  CHECK(device.choose(1u));
  CHECK(device.choose_family());
  CHECK_EQ(device.make_device(), VK_ERROR_DEVICE_LOST);
  CHECK(device.queue() == VK_NULL_HANDLE);
  CHECK(device.invariants_hold()); }

TEST(probe_device, memory_types_are_chosen_lowest_first_within_the_count)
{ ScriptedVulkan vulkan;
  vulkan.set_memory_types(4u);
  vulkan.set_memory_type(2u, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  vulkan.set_memory_type(3u, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  ProbeDevice device(vulkan);
  CHECK_EQ(device.make_instance(), VK_SUCCESS);
  CHECK_EQ(device.list_physical_devices(), VK_SUCCESS);
  CHECK(device.choose(1u));
  CHECK(device.choose_family());
  CHECK_EQ(device.make_device(), VK_SUCCESS);
  std::uint32_t type = 99u;
  CHECK(device.memory_type(0xFu, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, type));
  CHECK_EQ(type, 1u);
  CHECK(device.memory_type(0xDu, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, type));
  CHECK_EQ(type, 2u);
  CHECK(device.memory_type(0xFu, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, type));
  CHECK_EQ(type, 2u);
  type = 99u;
  CHECK(!device.memory_type(0x10u, 0u, type));
  CHECK(!device.memory_type(0x9u, VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, type));
  CHECK_EQ(type, 99u); }

TEST(probe_device, memory_types_past_vulkan_s_limit_are_not_looked_at)
{ ScriptedVulkan vulkan;
  vulkan.set_memory_types(VK_MAX_MEMORY_TYPES + 8u);
  ProbeDevice device(vulkan);
  CHECK_EQ(device.make_instance(), VK_SUCCESS);
  CHECK_EQ(device.list_physical_devices(), VK_SUCCESS);
  CHECK(device.choose(1u));
  CHECK(device.choose_family());
  CHECK_EQ(device.make_device(), VK_SUCCESS);
  std::uint32_t type = 0u;
  CHECK(!device.memory_type(0u, 0u, type)); }

TEST(probe_device, invariants_refuse_what_was_made_out_of_order)
{ ScriptedVulkan vulkan;
  ProbeDevice device(vulkan);
  CHECK_EQ(device.make_instance(), VK_SUCCESS);
  CHECK_EQ(device.list_physical_devices(), VK_SUCCESS);
  CHECK(device.choose(1u));
  CHECK(device.choose_family());
  CHECK_EQ(device.make_device(), VK_SUCCESS);
  CHECK(device.invariants_hold());
  const VkDevice made = device.device();
  const VkInstance instance = device.instance();
  const VkPhysicalDevice physical = device.physical_device();
  ProbeDeviceProbe::queueless(device);
  CHECK(!device.invariants_hold());
  ProbeDeviceProbe::orphan_device(device);
  CHECK(!device.invariants_hold());
  ProbeDeviceProbe::forget(device);
  CHECK(!device.invariants_hold());
  ProbeDeviceProbe::orphan_choice(device);
  CHECK(device.invariants_hold());
  ProbeDeviceProbe::long_list(device);
  CHECK(!device.invariants_hold());
  ProbeDevice other(vulkan);
  ProbeDeviceProbe::far_family(other);
  CHECK(!other.invariants_hold());
  ProbeDevice unlisted(vulkan);
  CHECK_EQ(unlisted.make_instance(), VK_SUCCESS);
  CHECK_EQ(unlisted.list_physical_devices(), VK_SUCCESS);
  CHECK(unlisted.choose(1u));
  ProbeDeviceProbe::orphan_choice(unlisted);
  CHECK(!unlisted.invariants_hold());
  ProbeDevice parentless(vulkan);
  CHECK_EQ(parentless.make_instance(), VK_SUCCESS);
  CHECK_EQ(parentless.list_physical_devices(), VK_SUCCESS);
  ProbeDeviceProbe::orphan_list(parentless);
  CHECK(!parentless.invariants_hold());
  static_cast<void>(physical);
  vulkan.destroy_device(made);
  vulkan.destroy_instance(instance); }

TEST(probe_device, the_window_system_extensions_are_enabled_on_the_instance_and_the_device)
{ ScriptedVulkan vulkan;
  ProbeDevice    device(vulkan);
  CHECK_EQ(device.make_instance(), VK_SUCCESS);
  CHECK(vulkan.instance_enabled("VK_KHR_surface"));
  CHECK(vulkan.instance_enabled("VK_KHR_win32_surface"));
  CHECK(!vulkan.instance_enabled("VK_KHR_swapchain"));
  CHECK_EQ(device.list_physical_devices(), VK_SUCCESS);
  CHECK(device.choose(1u));
  CHECK(device.choose_family());
  CHECK_EQ(device.make_device(), VK_SUCCESS);
  CHECK(vulkan.device_enabled("VK_KHR_swapchain"));
  CHECK(!vulkan.device_enabled("VK_KHR_win32_surface"));
  CHECK_EQ(vulkan.misuses(), 0u); }

TEST(probe_device_make_device, more_extensions_than_there_is_room_for_are_refused)
{ ScriptedVulkan vulkan;
  ProbeDevice    device(vulkan);
  CHECK_EQ(device.make_instance(), VK_SUCCESS);
  CHECK_EQ(device.list_physical_devices(), VK_SUCCESS);
  CHECK(device.choose(1u));
  CHECK(device.choose_family());
  const char *const wanted[ProbeDevice::max_device_extensions] = {};
  const std::uint32_t before = vulkan.calls(ScriptedVulkan::Call::create_device);
  CHECK_EQ(device.make_device(wanted, ProbeDevice::max_device_extensions),
           VK_ERROR_EXTENSION_NOT_PRESENT);
  CHECK_EQ(vulkan.calls(ScriptedVulkan::Call::create_device), before);
  const char *const fits[ProbeDevice::max_device_extensions - ProbeDevice::device_extension_count] = {
      "VK_KHR_external_memory_win32"};
  CHECK_EQ(device.make_device(fits, ProbeDevice::max_device_extensions - ProbeDevice::device_extension_count),
           VK_SUCCESS);
  CHECK(vulkan.device_enabled("VK_KHR_external_memory_win32")); }
