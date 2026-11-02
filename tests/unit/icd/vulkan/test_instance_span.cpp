#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/icd_rig.hpp"
#include "icd/vulkan/connection.hpp"
#include "proto/client/icd_handle_registry.hpp"
#include "icd/vulkan/instance_span.hpp"

#include <cstdint>

using av::AdapterScenario;
using av::AvgpuAdapters;
using av::IcdRig;
using av::PciIdentity;
using av::ScriptedHost;
using av::Status;
using av::icd::Connection;
using av::icd::InstanceSpan;

namespace {

using EnumeratePhysical = VkResult (*)(VkInstance, std::uint32_t *, VkPhysicalDevice *);
using EnumerateGroups = VkResult (*)(VkInstance, std::uint32_t *, VkPhysicalDeviceGroupProperties *);
using EnumerateAdapter = VkResult (*)(VkInstance, std::uint64_t, std::uint32_t *, VkPhysicalDevice *);

constexpr std::uint64_t integrated_luid = 0x0000000300005678ull;
const PciIdentity on_raphael(0x1002u, 0x13C0u, PciIdentity::virtio_vendor, AvgpuAdapters::av_subsystem);

VkInstance fake(std::uintptr_t value)
{
    return reinterpret_cast<VkInstance>(value);
}

VkInstance two_adapter_instance(IcdRig &rig)
{
    rig.kmt().add_adapter(on_raphael, AdapterScenario::wddm_3_2);
    rig.kmt().set_adapter_luid(1u, integrated_luid);
    rig.host().vk_handle(0, ScriptedHost::vk_instance);
    return rig.create_instance();
}

std::uint64_t luid_of(VkPhysicalDevice physical_device)
{
    const Connection *const connection = av::icd::connection_of(physical_device);
    return connection != nullptr ? connection->luid() : 0u;
}

}

TEST(instance_span, the_own_instance_first_and_each_sibling_once)
{ InstanceSpan span(fake(0x10u));
  CHECK(span.invariants_hold());
  CHECK_EQ(span.count(), 1u);
  CHECK(span.at(0u) == fake(0x10u));
  CHECK(span.at(1u) == VK_NULL_HANDLE);
  CHECK(span.spans_from(fake(0x10u)));
  CHECK(!span.spans_from(fake(0x20u)));
  CHECK(!span.spans_from(VK_NULL_HANDLE));
  CHECK_STATUS(span.add(fake(0x20u)), Status::ok);
  CHECK(!span.spans_from(fake(0x20u)));
  CHECK_EQ(span.count(), 2u);
  CHECK(span.invariants_hold());
  for (std::uint32_t index = 2u; index < InstanceSpan::max_instances; ++index)
      CHECK_STATUS(span.add(fake(0x100u + index)), Status::ok);
  CHECK_STATUS(span.add(fake(0x999u)), Status::no_space);
  CHECK_EQ(span.count(), InstanceSpan::max_instances); }

TEST(instance_span, an_instance_spanned_twice_or_none_breaks_the_invariant)
{ { InstanceSpan span(fake(0x10u));
    CHECK_STATUS(span.add(fake(0x10u)), Status::ok);
    CHECK(!span.invariants_hold()); }
  { InstanceSpan span(VK_NULL_HANDLE);
    CHECK(!span.invariants_hold()); } }

TEST(instance_span, an_instance_on_two_adapters_enumerates_a_device_of_each)
{ IcdRig rig;
  const VkInstance instance = two_adapter_instance(rig);
  CHECK(instance != VK_NULL_HANDLE);
  CHECK_EQ(rig.kmt().commands(), 2u);
  CHECK(rig.root().invariants_hold());
  const EnumeratePhysical enumerate = rig.entry<EnumeratePhysical>(instance, "vkEnumeratePhysicalDevices");
  std::uint32_t count = 0u;
  rig.host().vk_count(0, 1u);
  rig.host().vk_count(0, 1u);
  CHECK_EQ(enumerate(instance, &count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, 2u);
  VkPhysicalDevice devices[2] = {};
  rig.host().vk_count(0, 1u);
  rig.host().vk_physical_devices(0, 1u);
  rig.host().vk_count(0, 1u);
  rig.host().vk_physical_devices(0, 1u);
  CHECK_EQ(enumerate(instance, &count, devices), VK_SUCCESS);
  CHECK_EQ(count, 2u);
  CHECK(devices[0] != VK_NULL_HANDLE && devices[1] != VK_NULL_HANDLE && devices[0] != devices[1]);
  CHECK_EQ(luid_of(devices[0]), IcdRig::adapter_luid);
  CHECK_EQ(luid_of(devices[1]), integrated_luid);
  VkPhysicalDevice first = VK_NULL_HANDLE;
  count = 1u;
  rig.host().vk_count(0, 1u);
  rig.host().vk_physical_devices(0, 1u);
  rig.host().vk_count(0, 1u);
  CHECK_EQ(enumerate(instance, &count, &first), VK_INCOMPLETE);
  CHECK_EQ(count, 1u);
  CHECK(first == devices[0]);
  const EnumerateAdapter by_luid = rig.entry<EnumerateAdapter>(VK_NULL_HANDLE, "vk_icdEnumerateAdapterPhysicalDevices");
  VkPhysicalDevice on_integrated = VK_NULL_HANDLE;
  count = 1u;
  rig.host().vk_physical_devices(0, 1u);
  CHECK_EQ(by_luid(instance, integrated_luid, &count, &on_integrated), VK_SUCCESS);
  CHECK_EQ(count, 1u);
  CHECK(on_integrated == devices[1]);
  count = 1u;
  CHECK_EQ(by_luid(instance, 0x99u, &count, &on_integrated), VK_SUCCESS);
  CHECK_EQ(count, 0u);
  CHECK(rig.root().invariants_hold());
  rig.destroy_instance(instance);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(instance_span, device_groups_are_each_adapters_own)
{ IcdRig rig;
  const VkInstance instance = two_adapter_instance(rig);
  const EnumerateGroups enumerate = rig.entry<EnumerateGroups>(instance, "vkEnumeratePhysicalDeviceGroups");
  std::uint32_t count = 0u;
  rig.host().vk_count(0, 1u);
  rig.host().vk_count(0, 1u);
  CHECK_EQ(enumerate(instance, &count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, 2u);
  rig.destroy_instance(instance);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(instance_span, an_adapter_whose_host_refuses_is_left_out)
{ IcdRig rig;
  rig.kmt().add_adapter(on_raphael, AdapterScenario::wddm_3_2);
  rig.kmt().set_adapter_luid(1u, integrated_luid);
  const auto create = rig.entry<VkResult (*)(const VkInstanceCreateInfo *, const VkAllocationCallbacks *,
                                             VkInstance *)>(VK_NULL_HANDLE, "vkCreateInstance");
  VkInstanceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  VkInstance instance = VK_NULL_HANDLE;
  rig.host().vk_handle(0, ScriptedHost::vk_instance);
  rig.host().vk_handle(VK_ERROR_INITIALIZATION_FAILED, 0u);
  CHECK_EQ(create(&info, nullptr, &instance), VK_SUCCESS);
  CHECK(instance != VK_NULL_HANDLE);
  CHECK_EQ(rig.log().holding("another adapter's host refused"), 1u);
  CHECK(rig.root().invariants_hold());
  const EnumeratePhysical enumerate = rig.entry<EnumeratePhysical>(instance, "vkEnumeratePhysicalDevices");
  std::uint32_t count = 0u;
  rig.host().vk_count(0, 1u);
  CHECK_EQ(enumerate(instance, &count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, 1u);
  rig.destroy_instance(instance);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }
