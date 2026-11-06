#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/icd_rig.hpp"
#include "icd/vulkan/connection.hpp"
#include "icd/vulkan/icd_root.hpp"
#include "core/support/object_pool.hpp"
#include "icd/vulkan/instance_span.hpp"
#include "icd/vulkan/loader_handle.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

namespace av::icd {
struct IcdRootProbe {
    static InstanceSpan *span(IcdRoot &root, VkInstance instance) { return root.span_of(instance); }
    static void          drop_instance(IcdRoot &root, VkInstance instance) { root.destroy_one_instance(instance, nullptr); }
};

}

using av::AdapterScenario;
using av::AvgpuAdapters;
using av::IcdRig;
using av::MockKmt;
using av::PciIdentity;
using av::ScriptedHost;
using av::icd::IcdRootProbe;
using av::icd::InstanceSpan;
using Kind = av::icd::LoaderHandle::Kind;

namespace {

using EnumeratePhysical = VkResult (*)(VkInstance, std::uint32_t *, VkPhysicalDevice *);
using EnumerateAdapter = VkResult (*)(VkInstance, std::uint64_t, std::uint32_t *, VkPhysicalDevice *);

constexpr std::uint64_t integrated_luid = 0x0000000300005678ull;
const PciIdentity on_raphael(0x1002u, 0x13C0u, PciIdentity::virtio_vendor, AvgpuAdapters::av_subsystem);

void second_adapter(IcdRig &rig)
{
    rig.kmt().add_adapter(on_raphael, AdapterScenario::wddm_3_2);
    rig.kmt().set_adapter_luid(1u, integrated_luid);
}

VkInstance two_adapter_instance(IcdRig &rig)
{
    second_adapter(rig);
    rig.host().vk_handle(0, ScriptedHost::vk_instance);
    return rig.create_instance();
}

VkResult create(IcdRig &rig, VkInstance &instance)
{
    const auto make = rig.entry<VkResult (*)(const VkInstanceCreateInfo *, const VkAllocationCallbacks *,
                                             VkInstance *)>(VK_NULL_HANDLE, "vkCreateInstance");
    VkInstanceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance = VK_NULL_HANDLE;
    return make(&info, nullptr, &instance);
}

struct Step {
    VkResult      counted;
    std::uint32_t has;
    VkResult      listed;
};

Step          steps[8] = {};
std::uint32_t at = 0u;

VkResult VKAPI_CALL scripted_groups(VkInstance, std::uint32_t *count, VkPhysicalDeviceGroupProperties *groups)
{
    if (groups == nullptr) {
        const Step &step = steps[at++];
        *count = step.has;
        return step.counted;
    }
    const Step &step = steps[at - 1u];
    if (*count > step.has) *count = step.has;
    return step.listed;
}

}

TEST(icd_root_spans, a_sibling_whose_count_fails_is_left_out_and_the_primary_s_failure_is_passed_on)
{ IcdRig rig;
  const VkInstance instance = two_adapter_instance(rig);
  const EnumeratePhysical enumerate = rig.entry<EnumeratePhysical>(instance, "vkEnumeratePhysicalDevices");
  std::uint32_t count = 7u;
  rig.host().vk_count(VK_ERROR_INITIALIZATION_FAILED, 0u);
  CHECK_EQ(enumerate(instance, &count, nullptr), VK_ERROR_INITIALIZATION_FAILED);
  CHECK_EQ(count, 7u);
  rig.host().vk_count(0, 1u);
  rig.host().vk_count(VK_ERROR_INITIALIZATION_FAILED, 0u);
  CHECK_EQ(enumerate(instance, &count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, 1u);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  rig.destroy_instance(instance);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(icd_root_spans, a_listing_the_host_refuses_is_the_answer_and_a_partial_one_is_taken_as_far_as_it_goes)
{ IcdRig rig;
  const VkInstance instance = two_adapter_instance(rig);
  const EnumeratePhysical enumerate = rig.entry<EnumeratePhysical>(instance, "vkEnumeratePhysicalDevices");
  VkPhysicalDevice devices[2] = {};
  std::uint32_t    count = 2u;
  rig.host().vk_count(0, 1u);
  rig.host().vk_physical_devices(VK_ERROR_DEVICE_LOST, 0u);
  CHECK_EQ(enumerate(instance, &count, devices), VK_ERROR_DEVICE_LOST);
  rig.host().vk_count(0, 1u);
  rig.host().vk_physical_devices(VK_INCOMPLETE, 1u);
  rig.host().vk_count(0, 1u);
  rig.host().vk_physical_devices(0, 1u);
  count = 2u;
  CHECK_EQ(enumerate(instance, &count, devices), VK_SUCCESS);
  CHECK_EQ(count, 2u);
  CHECK(devices[0] != VK_NULL_HANDLE && devices[1] != VK_NULL_HANDLE);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  rig.destroy_instance(instance);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(icd_root_spans, a_physical_device_no_handle_can_be_made_for_ends_the_listing_out_of_host_memory)
{ IcdRig rig;
  const VkInstance instance = two_adapter_instance(rig);
  av::icd::Connection *const connection = rig.root().connection_of(instance, Kind::instance);
  for (std::uint64_t host = 1u; host < av::ObjectPool<av::icd::LoaderHandle>::chunk_slots - 1u; host++)
      CHECK(rig.root().wrap(Kind::physical_device, 0x90000u + host, connection) != nullptr);
  const EnumeratePhysical enumerate = rig.entry<EnumeratePhysical>(instance, "vkEnumeratePhysicalDevices");
  VkPhysicalDevice devices[2] = {};
  std::uint32_t    count = 2u;
  rig.host().vk_count(0, 1u);
  rig.host().vk_physical_devices(0, 1u);
  rig.platform().fail_allocation_at(1u);
  CHECK_EQ(enumerate(instance, &count, devices), VK_ERROR_OUT_OF_HOST_MEMORY);
  rig.platform().fail_allocation_at(0u);
  CHECK(rig.root().invariants_hold());
  rig.destroy_instance(instance);
  CHECK(rig.root().invariants_hold()); }

TEST(icd_root_spans, device_groups_of_every_adapter_are_listed_as_far_as_there_is_room)
{ IcdRig rig;
  const VkInstance instance = two_adapter_instance(rig);
  VkPhysicalDeviceGroupProperties groups[4] = {};
  std::uint32_t count = 0u;
  at = 0u;
  steps[0] = {VK_SUCCESS, 2u, VK_SUCCESS};
  steps[1] = {VK_SUCCESS, 1u, VK_SUCCESS};
  CHECK_EQ(rig.root().enumerate_physical_device_groups(scripted_groups, instance, &count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, 3u);

  at = 0u;
  steps[0] = {VK_SUCCESS, 2u, VK_SUCCESS};
  steps[1] = {VK_SUCCESS, 1u, VK_SUCCESS};
  steps[2] = {VK_SUCCESS, 1u, VK_SUCCESS};
  count = 4u;
  CHECK_EQ(rig.root().enumerate_physical_device_groups(scripted_groups, instance, &count, groups), VK_SUCCESS);
  CHECK_EQ(count, 3u);

  at = 0u;
  steps[0] = {VK_SUCCESS, 2u, VK_SUCCESS};
  steps[1] = {VK_SUCCESS, 1u, VK_SUCCESS};
  count = 2u;
  CHECK_EQ(rig.root().enumerate_physical_device_groups(scripted_groups, instance, &count, groups), VK_INCOMPLETE);
  CHECK_EQ(count, 2u);

  at = 0u;
  steps[0] = {VK_SUCCESS, 2u, VK_INCOMPLETE};
  steps[1] = {VK_SUCCESS, 0u, VK_SUCCESS};
  count = 1u;
  CHECK_EQ(rig.root().enumerate_physical_device_groups(scripted_groups, instance, &count, groups), VK_INCOMPLETE);
  CHECK_EQ(count, 1u);
  rig.destroy_instance(instance); }

TEST(icd_root_spans, a_group_listing_the_host_refuses_or_a_count_that_fails_is_handled_adapter_by_adapter)
{ IcdRig rig;
  const VkInstance instance = two_adapter_instance(rig);
  VkPhysicalDeviceGroupProperties groups[4] = {};
  std::uint32_t count = 9u;
  at = 0u;
  steps[0] = {VK_ERROR_INITIALIZATION_FAILED, 0u, VK_SUCCESS};
  CHECK_EQ(rig.root().enumerate_physical_device_groups(scripted_groups, instance, &count, nullptr),
           VK_ERROR_INITIALIZATION_FAILED);
  CHECK_EQ(count, 9u);
  at = 0u;
  steps[0] = {VK_SUCCESS, 1u, VK_SUCCESS};
  steps[1] = {VK_ERROR_INITIALIZATION_FAILED, 0u, VK_SUCCESS};
  CHECK_EQ(rig.root().enumerate_physical_device_groups(scripted_groups, instance, &count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, 1u);
  at = 0u;
  steps[0] = {VK_SUCCESS, 1u, VK_ERROR_DEVICE_LOST};
  count = 4u;
  CHECK_EQ(rig.root().enumerate_physical_device_groups(scripted_groups, instance, &count, groups),
           VK_ERROR_DEVICE_LOST);
  at = 0u;
  steps[0] = {VK_SUCCESS, 2u, VK_SUCCESS};
  steps[1] = {VK_SUCCESS, 1u, VK_SUCCESS};
  count = 2u;
  CHECK_EQ(rig.root().enumerate_physical_device_groups(scripted_groups, instance, &count, groups), VK_INCOMPLETE);
  rig.destroy_instance(instance); }

TEST(icd_root_spans, an_instance_with_no_span_enumerates_and_groups_through_itself_alone)
{ IcdRig rig;
  const VkInstance instance = two_adapter_instance(rig);
  InstanceSpan *const span = IcdRootProbe::span(rig.root(), instance);
  CHECK(span != nullptr && span->count() == 2u);
  const VkInstance sibling = span->at(1u);
  const EnumeratePhysical enumerate = rig.entry<EnumeratePhysical>(sibling, "vkEnumeratePhysicalDevices");
  std::uint32_t count = 0u;
  rig.host().vk_count(0, 1u);
  CHECK_EQ(enumerate(sibling, &count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, 1u);
  at = 0u;
  steps[0] = {VK_SUCCESS, 3u, VK_SUCCESS};
  CHECK_EQ(rig.root().enumerate_physical_device_groups(scripted_groups, sibling, &count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, 3u);
  const EnumerateAdapter by_luid = rig.entry<EnumerateAdapter>(VK_NULL_HANDLE, "vk_icdEnumerateAdapterPhysicalDevices");
  CHECK_EQ(by_luid(sibling, 0x99u, &count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, 0u);
  rig.destroy_instance(instance); }

TEST(icd_root_spans, an_adapter_asked_for_by_luid_is_not_found_through_a_sibling_that_is_gone_and_refused_when_it_is_lost)
{ { IcdRig rig;
    const VkInstance instance = two_adapter_instance(rig);
    const VkInstance sibling = IcdRootProbe::span(rig.root(), instance)->at(1u);
    IcdRootProbe::drop_instance(rig.root(), sibling);
    const EnumerateAdapter by_luid = rig.entry<EnumerateAdapter>(VK_NULL_HANDLE, "vk_icdEnumerateAdapterPhysicalDevices");
    std::uint32_t count = 5u;
    CHECK_EQ(by_luid(instance, integrated_luid, &count, nullptr), VK_SUCCESS);
    CHECK_EQ(count, 0u);
    rig.destroy_instance(instance); }
  { IcdRig rig;
    const VkInstance instance = two_adapter_instance(rig);
    const VkInstance sibling = IcdRootProbe::span(rig.root(), instance)->at(1u);
    av::icd::Connection *const connection = rig.root().connection_of(sibling, Kind::instance);
    CHECK(connection != nullptr);
    av::WireWriter writer = connection->channel().transport().request_writer();
    av::WireReader reply(nullptr, 0u);
    CHECK(connection->channel().transport().send_sync(0x10001u, writer, reply) != av::TransportResult::ok);
    const EnumerateAdapter by_luid = rig.entry<EnumerateAdapter>(VK_NULL_HANDLE, "vk_icdEnumerateAdapterPhysicalDevices");
    std::uint32_t count = 5u;
    CHECK_EQ(by_luid(instance, integrated_luid, &count, nullptr), VK_ERROR_DEVICE_LOST);
    rig.destroy_instance(instance); } }

TEST(icd_root_spans, an_adapter_list_the_kernel_will_not_give_again_leaves_the_instance_with_its_own_adapter)
{ std::uint32_t alone = 0u;
  for (std::uint32_t failing = 1u; failing <= 8u; failing++) {
      IcdRig rig;
      second_adapter(rig);
      rig.kmt().fail_at(MockKmt::Call::enumerate_adapters, failing, MockKmt::invalid_parameter);
      rig.host().vk_handle(0, ScriptedHost::vk_instance);
      VkInstance instance = VK_NULL_HANDLE;
      const VkResult made = create(rig, instance);
      if (made != VK_SUCCESS) continue;
      InstanceSpan *const span = IcdRootProbe::span(rig.root(), instance);
      if (span != nullptr && span->count() == 1u) alone++;
      CHECK(rig.root().invariants_hold());
      rig.destroy_instance(instance);
      CHECK_EQ(rig.kmt().open_objects(), 0u);
  }
  CHECK(alone > 0u); }

TEST(icd_root_spans, a_connection_to_another_adapter_that_cannot_be_opened_leaves_that_adapter_out)
{ std::uint32_t alone = 0u;
  for (std::uint32_t failing = 1u; failing <= 6u; failing++) {
      IcdRig rig;
      second_adapter(rig);
      rig.kmt().fail_at(MockKmt::Call::create_device, failing, MockKmt::invalid_parameter);
      rig.host().vk_handle(0, ScriptedHost::vk_instance);
      VkInstance instance = VK_NULL_HANDLE;
      const VkResult made = create(rig, instance);
      if (made != VK_SUCCESS) continue;
      InstanceSpan *const span = IcdRootProbe::span(rig.root(), instance);
      if (span != nullptr && span->count() == 1u && rig.log().holding("no Vulkan connection to another avgpu adapter") != 0u)
          alone++;
      CHECK(rig.root().invariants_hold());
      rig.destroy_instance(instance);
      CHECK_EQ(rig.kmt().open_objects(), 0u);
  }
  CHECK(alone > 0u); }

TEST(icd_root_spans, a_sibling_instance_no_handle_could_be_made_for_is_given_back_and_the_primary_goes_on)
{ std::uint32_t alone = 0u;
  for (std::uint32_t failing = 1u; failing <= 60u; failing++) {
      IcdRig rig;
      second_adapter(rig);
      rig.host().vk_handle(0, ScriptedHost::vk_instance);
      rig.host().vk_handle(0, ScriptedHost::vk_instance + 1u);
      VkInstance instance = VK_NULL_HANDLE;
      rig.platform().fail_allocation_at(failing);
      const VkResult made = create(rig, instance);
      rig.platform().fail_allocation_at(0u);
      if (made != VK_SUCCESS) {
          CHECK(instance == VK_NULL_HANDLE);
          CHECK(rig.root().invariants_hold());
          CHECK_EQ(rig.kmt().open_objects(), 0u);
          continue;
      }
      InstanceSpan *const span = IcdRootProbe::span(rig.root(), instance);
      if (span == nullptr || span->count() == 1u) alone++;
      CHECK(rig.root().invariants_hold());
      rig.destroy_instance(instance);
      CHECK_EQ(rig.kmt().open_objects(), 0u);
  }
  CHECK(alone > 0u); }

TEST(icd_root_spans, a_sibling_instance_the_handle_pool_has_no_room_for_is_given_back_and_the_primary_goes_on)
{ std::uint32_t alone = 0u;
  for (std::uint32_t failing = 1u; failing <= 120u; failing++) {
      IcdRig rig;
      second_adapter(rig);
      rig.host().vk_handle(0, ScriptedHost::vk_instance);
      const VkInstance first = rig.create_instance();
      av::icd::Connection *const connection = rig.root().connection_of(first, Kind::instance);
      for (std::uint64_t host = 1u; host < av::ObjectPool<av::icd::LoaderHandle>::chunk_slots - 2u; host++)
          CHECK(rig.root().wrap(Kind::physical_device, 0x90000u + host, connection) != nullptr);
      rig.host().vk_handle(0, ScriptedHost::vk_instance + 1u);
      rig.host().vk_handle(0, ScriptedHost::vk_instance + 2u);
      VkInstance instance = VK_NULL_HANDLE;
      rig.platform().fail_allocation_at(failing);
      const VkResult made = create(rig, instance);
      rig.platform().fail_allocation_at(0u);
      CHECK(rig.root().invariants_hold());
      if (made == VK_SUCCESS) {
          InstanceSpan *const span = IcdRootProbe::span(rig.root(), instance);
          if (span != nullptr && span->count() == 1u) alone++;
          rig.destroy_instance(instance);
      }
      rig.destroy_instance(first);
      CHECK(rig.root().invariants_hold());
      CHECK_EQ(rig.kmt().open_objects(), 0u);
  }
  CHECK(alone > 0u); }
