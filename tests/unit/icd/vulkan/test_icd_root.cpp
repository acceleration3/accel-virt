/* The ICD as the loader reaches it (contracts/loader_interface.md), over MockKmt
 * with the test playing the host: interface negotiation, what is answered before
 * and after an instance exists, the instance's own connection and its refusals,
 * the adapter's physical device by LUID, device teardown, the generated
 * wrappers' hooks, and the lock order that keeps two threads from deadlocking.
 * Invariants after every call that changes the root. */
#include "core/support/status.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/transport/guest_connection.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/icd_rig.hpp"
#include "harness/icd/scripted_adapter_luids.hpp"
#include "icd/vulkan/connection.hpp"
#include "icd/vulkan/icd_binding.hpp"
#include "icd/vulkan/icd_root.hpp"
#include "icd/vulkan/loader_handle.hpp"
#include "icd/vulkan/local_entries.hpp"
#include "proto/generated/vulkan/vk_icd_entries.h"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/generated/vulkan/vk_supported_extensions.h"
#include "proto/icd_connection.hpp"
#include "proto/icd_handle_registry.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/submission/command_header.hpp"

#include <cstdint>
#include <cstring>

using av::CommandHeader;
using av::IcdRig;
using av::MockKmt;
using av::ScriptedAdapterLuids;
using av::ScriptedHost;
using av::Status;
using av::WireReader;
using av::icd::Connection;
using av::icd::IcdRoot;
using av::icd::LoaderHandle;
using Kind = av::icd::LoaderHandle::Kind;
using namespace av::vkproto;

namespace av::icd {

struct IcdRootProbe {
    static void leave_pending(IcdRoot &root, Connection *connection) { root.pending_ = connection; }
};

struct LoaderHandleProbe {
    static void set_host(LoaderHandle &handle, std::uint64_t host) { handle.host_ = host; }
};

struct ConnectionProbe {
    static GuestConnection &guest(Connection &c) { return c.connection_; }
};

}  // namespace av::icd

namespace av {

struct GuestConnectionProbe {
    static void set_queue(GuestConnection &c, std::uint32_t queue) { c.queue_ = queue; }
};

}  // namespace av

using av::GuestConnectionProbe;
using av::icd::ConnectionProbe;
using av::icd::IcdRootProbe;
using av::icd::LoaderHandleProbe;

namespace {

using CreateInstance = VkResult (*)(const VkInstanceCreateInfo *, const VkAllocationCallbacks *, VkInstance *);
using EnumerateAdapter = VkResult (*)(VkInstance, std::uint64_t, std::uint32_t *, VkPhysicalDevice *);
using EnumeratePhysical = VkResult (*)(VkInstance, std::uint32_t *, VkPhysicalDevice *);
using Negotiate = VkResult (*)(std::uint32_t *);
using InstanceVersion = VkResult (*)(std::uint32_t *);
using InstanceExtensions = VkResult (*)(const char *, std::uint32_t *, VkExtensionProperties *);
using PhysicalProcAddr = PFN_vkVoidFunction (*)(VkInstance, const char *);
using CreateDevice = VkResult (*)(VkPhysicalDevice, const VkDeviceCreateInfo *, const VkAllocationCallbacks *,
                                  VkDevice *);
using DestroyDevice = void (*)(VkDevice, const VkAllocationCallbacks *);
using GetDeviceQueue = void (*)(VkDevice, std::uint32_t, std::uint32_t, VkQueue *);
using QueueWaitIdle = VkResult (*)(VkQueue);
using AllocateCommandBuffers = VkResult (*)(VkDevice, const VkCommandBufferAllocateInfo *, VkCommandBuffer *);
using CmdDraw = void (*)(VkCommandBuffer, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
using ResetCommandBuffer = VkResult (*)(VkCommandBuffer, VkCommandBufferResetFlags);
using Properties = void (*)(VkPhysicalDevice, VkPhysicalDeviceProperties *);
using Properties2 = void (*)(VkPhysicalDevice, VkPhysicalDeviceProperties2 *);

constexpr std::int32_t  vk_error_initialization_failed = -3;
constexpr std::uint32_t vulkan_1_3 = 0x00403000u;
constexpr std::uint64_t other_luid = 0x0000000200001235ull;
constexpr std::uint64_t queue_host = 0x6600u;
constexpr std::uint64_t command_buffer_host = 0x7700u;
/* What one open connection holds in MockKmt (test_connection.cpp). */
constexpr std::uint32_t connection_objects = 9u;

PFN_vkVoidFunction entry_of(auto function) { return reinterpret_cast<PFN_vkVoidFunction>(function); }

/* The body of the journalled command at `index`, past its header. */
WireReader body_of(MockKmt &kmt, std::uint32_t index)
{
    return WireReader(kmt.command_bytes(index) + CommandHeader::encoded_size,
                      MockKmt::journal_bytes - CommandHeader::encoded_size);
}

VkDevice create_device(IcdRig &rig, VkInstance instance, VkPhysicalDevice physical_device)
{
    VkDeviceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    VkDevice device = VK_NULL_HANDLE;
    rig.host().vk_handle(0, ScriptedHost::vk_device);
    CHECK_EQ(rig.entry<CreateDevice>(instance, "vkCreateDevice")(physical_device, &info, nullptr, &device), VK_SUCCESS);
    return device;
}

VkQueue get_queue(IcdRig &rig, VkInstance instance, VkDevice device)
{
    VkQueue queue = VK_NULL_HANDLE;
    rig.host().vk_handle(0, queue_host);
    rig.entry<GetDeviceQueue>(instance, "vkGetDeviceQueue")(device, 0u, 0u, &queue);
    return queue;
}

}  // namespace

TEST(icd_root, binds_itself_for_its_life)
{ CHECK(!av::icd::icd_binding.bound());
  { IcdRig rig;
    CHECK(av::icd::icd_binding.bound());
    CHECK(&av::icd::icd_binding.root() == &rig.root());
    CHECK(rig.root().invariants_hold()); }
  CHECK(!av::icd::icd_binding.bound()); }

TEST(icd_binding, holds_one_root_until_unbound)
{ IcdRig rig;
  av::icd::IcdBinding binding;
  CHECK(!binding.bound());
  binding.bind(rig.root());
  CHECK(binding.bound());
  CHECK(&binding.root() == &rig.root());
  binding.unbind();
  CHECK(!binding.bound()); }

TEST(icd_root, negotiates_interface_version_seven_and_accepts_five_and_six)
{ IcdRig rig;
  const Negotiate negotiate = rig.entry<Negotiate>(VK_NULL_HANDLE, "vk_icdNegotiateLoaderICDInterfaceVersion");
  CHECK(negotiate != nullptr);
  struct Case {
      std::uint32_t asked;
      VkResult      result;
      std::uint32_t answered;
  };
  const Case cases[] = {
      {0xFFFFFFFFu, VK_SUCCESS, 7u}, {8u, VK_SUCCESS, 7u}, {7u, VK_SUCCESS, 7u}, {6u, VK_SUCCESS, 6u},
      {5u, VK_SUCCESS, 5u}, {4u, VK_ERROR_INCOMPATIBLE_DRIVER, 4u}, {0u, VK_ERROR_INCOMPATIBLE_DRIVER, 0u},
  };
  for (const Case &check : cases) {
      std::uint32_t version = check.asked;
      CHECK_EQ(negotiate(&version), check.result);
      CHECK_EQ(version, check.answered);
  }
  CHECK_EQ(rig.log().holding("loader interface version refused: 4"), 1u);
  CHECK_EQ(rig.log().holding("loader interface version refused: 0"), 1u);
  CHECK_EQ(negotiate(nullptr), VK_ERROR_INCOMPATIBLE_DRIVER);
  CHECK_EQ(rig.log().holding("without a version"), 1u); }

TEST(icd_root, before_an_instance_answers_exactly_the_loaders_six_entry_points)
{ IcdRig rig;
  IcdRoot &root = rig.root();
  CHECK(root.instance_proc_addr(VK_NULL_HANDLE, "vkCreateInstance") == entry_of(&av::icd::local_vkCreateInstance));
  CHECK(root.instance_proc_addr(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties") ==
        entry_of(&av::icd::local_vkEnumerateInstanceExtensionProperties));
  CHECK(root.instance_proc_addr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion") ==
        entry_of(&av::icd::local_vkEnumerateInstanceVersion));
  CHECK(root.instance_proc_addr(VK_NULL_HANDLE, "vk_icdNegotiateLoaderICDInterfaceVersion") ==
        entry_of(&av::icd::local_vk_icdNegotiateLoaderICDInterfaceVersion));
  CHECK(root.instance_proc_addr(VK_NULL_HANDLE, "vk_icdGetPhysicalDeviceProcAddr") ==
        entry_of(&av::icd::local_vk_icdGetPhysicalDeviceProcAddr));
  CHECK(root.instance_proc_addr(VK_NULL_HANDLE, "vk_icdEnumerateAdapterPhysicalDevices") ==
        entry_of(&av::icd::local_vk_icdEnumerateAdapterPhysicalDevices));
  const char *const not_before_an_instance[] = {"vkDestroyDevice", "vkCreateDevice", "vkGetPhysicalDeviceProperties",
                                                "vkEnumeratePhysicalDevices", "vk_icdGetInstanceProcAddr",
                                                "vkDestroyInstance", "", nullptr};
  for (const char *name : not_before_an_instance) CHECK(root.instance_proc_addr(VK_NULL_HANDLE, name) == nullptr);
  CHECK(root.invariants_hold()); }

TEST(icd_root, an_instance_it_made_answers_every_entry_point_with_its_own_rows_first)
{ IcdRig rig;
  const VkInstance instance = rig.create_instance();
  CHECK(instance != VK_NULL_HANDLE);
  IcdRoot &root = rig.root();
  std::uint32_t wrappers = 0u;
  for (const av::vkicd::generated::Entry &row : av::vkicd::generated::wrapper_entries()) {
      const PFN_vkVoidFunction found = root.instance_proc_addr(instance, row.name);
      CHECK(found != nullptr);
      if (found == row.fn) wrappers++;
  }
  /* Four wrappers are overridden: vkCreateInstance, vkDestroyDevice and both property queries. */
  CHECK_EQ(wrappers, av::vkicd::generated::wrapper_entries().size() - 4u);
  CHECK(root.instance_proc_addr(instance, "vkDestroyDevice") == entry_of(&av::icd::local_vkDestroyDevice));
  CHECK(root.instance_proc_addr(instance, "vkCreateInstance") == entry_of(&av::icd::local_vkCreateInstance));
  CHECK(root.instance_proc_addr(instance, "vkGetPhysicalDeviceProperties2") ==
        entry_of(&av::icd::local_vkGetPhysicalDeviceProperties2));
  CHECK(root.instance_proc_addr(instance, "vk_icdEnumerateAdapterPhysicalDevices") ==
        entry_of(&av::icd::local_vk_icdEnumerateAdapterPhysicalDevices));
  CHECK(root.instance_proc_addr(instance, "vkNotAnEntryPoint") == nullptr);
  rig.destroy_instance(instance);
  CHECK(rig.root().invariants_hold()); }

TEST(icd_root, a_pointer_it_did_not_make_as_an_instance_answers_nothing)
{ IcdRig rig;
  const VkInstance instance = rig.create_instance();
  const VkPhysicalDevice physical_device = rig.physical_device(instance);
  int stack = 0;
  CHECK(rig.root().instance_proc_addr(reinterpret_cast<VkInstance>(&stack), "vkCreateDevice") == nullptr);
  CHECK(rig.root().instance_proc_addr(reinterpret_cast<VkInstance>(physical_device), "vkCreateDevice") == nullptr);
  rig.destroy_instance(instance);
  /* A destroyed instance too. */
  CHECK(rig.root().instance_proc_addr(instance, "vkCreateDevice") == nullptr);
  CHECK(rig.root().invariants_hold()); }

TEST(icd_root, every_manual_entry_point_it_answers_is_its_own_and_the_rest_are_not_advertised)
{ IcdRig rig;
  const VkInstance instance = rig.create_instance();
  std::uint32_t answered = 0u;
  for (const char *name : kManualEntryPoints) {
      const PFN_vkVoidFunction found = rig.root().instance_proc_addr(instance, name);
      if (found == nullptr) continue;
      answered++;
      CHECK(found == rig.root().instance_proc_addr(VK_NULL_HANDLE, name));
  }
  CHECK_EQ(answered, 2u);
  rig.destroy_instance(instance); }

TEST(icd_root, reports_no_instance_extensions_and_no_layers)
{ IcdRig rig;
  const InstanceExtensions enumerate =
      rig.entry<InstanceExtensions>(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");
  std::uint32_t count = 5u;
  CHECK_EQ(enumerate(nullptr, &count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, 0u);
  VkExtensionProperties properties[2] = {};
  properties[0].specVersion = 9u;
  count = 2u;
  CHECK_EQ(enumerate(nullptr, &count, properties), VK_SUCCESS);
  CHECK_EQ(count, 0u);
  CHECK_EQ(properties[0].specVersion, 9u);
  count = 2u;
  CHECK_EQ(enumerate("VK_LAYER_KHRONOS_validation", &count, properties), VK_ERROR_LAYER_NOT_PRESENT);
  CHECK_EQ(count, 2u);
  CHECK_EQ(rig.log().holding("no layer"), 1u);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(icd_root, reports_vulkan_1_3)
{ IcdRig rig;
  std::uint32_t version = 0u;
  CHECK_EQ(rig.entry<InstanceVersion>(VK_NULL_HANDLE, "vkEnumerateInstanceVersion")(&version), VK_SUCCESS);
  CHECK_EQ(version, vulkan_1_3);
  version = 0u;
  CHECK_EQ(rig.root().enumerate_instance_version(&version), VK_SUCCESS);
  CHECK_EQ(version, vulkan_1_3); }

TEST(icd_root, answers_no_physical_device_level_entry_point_it_cannot_tell_from_a_device_level_one)
{ IcdRig rig;
  const VkInstance instance = rig.create_instance();
  const PhysicalProcAddr proc_addr = rig.entry<PhysicalProcAddr>(VK_NULL_HANDLE, "vk_icdGetPhysicalDeviceProcAddr");
  CHECK(proc_addr(instance, "vkGetPhysicalDeviceFeatures") == nullptr);
  CHECK(proc_addr(instance, "vkCreateBuffer") == nullptr);
  rig.destroy_instance(instance); }

TEST(icd_root, create_instance_opens_its_own_connection_and_wraps_the_hosts_instance)
{ IcdRig rig;
  const VkInstance instance = rig.create_instance();
  CHECK(rig.root().invariants_hold());
  CHECK(instance != VK_NULL_HANDLE);
  /* What the loader reads at the handle's address, before it writes its own. */
  CHECK(*static_cast<void **>(static_cast<void *>(instance)) == reinterpret_cast<void *>(LoaderHandle::loader_magic));
  CHECK_EQ(rig.kmt().open_objects(), connection_objects);
  CHECK_EQ(rig.kmt().commands(), 1u);
  CHECK_EQ(rig.kmt().command_id(0u), static_cast<std::uint32_t>(kCmd_vkCreateInstance));
  /* The connection opened under the root's lock, the command under the send
   * lock, and the handle wrapped under the root's lock again. */
  CHECK(std::strcmp(rig.trace().text(), "HhSsHh") == 0);
  const Connection *const connection = av::icd::connection_of(instance);
  CHECK(connection != nullptr && connection->opened());
  CHECK_EQ(connection->luid(), ScriptedAdapterLuids::default_luid);
  CHECK_EQ(av::vkproto::handle_to_wire(instance), ScriptedHost::vk_instance);
  rig.destroy_instance(instance);
  CHECK(rig.root().invariants_hold());
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.kmt().command_id(1u), static_cast<std::uint32_t>(kCmd_vkDestroyInstance));
  WireReader destroyed = body_of(rig.kmt(), 1u);
  CHECK_EQ(destroyed.read_u64(), ScriptedHost::vk_instance); }

TEST(icd_root, two_instances_have_two_connections)
{ IcdRig rig;
  const VkInstance first = rig.create_instance();
  const VkInstance second = rig.create_instance();
  CHECK(rig.root().invariants_hold());
  CHECK(first != VK_NULL_HANDLE && second != VK_NULL_HANDLE && first != second);
  CHECK(av::icd::connection_of(first) != av::icd::connection_of(second));
  CHECK_EQ(rig.kmt().open_objects(), 2u * connection_objects);
  rig.destroy_instance(first);
  CHECK(rig.root().invariants_hold());
  CHECK_EQ(rig.kmt().open_objects(), connection_objects);
  CHECK(av::icd::connection_of(second) != nullptr);
  rig.destroy_instance(second);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(icd_root, create_instance_with_no_way_to_the_host_answers_incompatible_driver_and_leaves_nothing_open)
{ struct Refusal {
      const char   *why;
      bool          no_adapter;
      bool          no_luid;
      MockKmt::Call call;
  };
  const Refusal refusals[] = {
      {"no rvkgpu adapter", true, false, MockKmt::Call::count},
      {"no LUID", false, true, MockKmt::Call::count},
      {"a capset without Vulkan", false, false, MockKmt::Call::create_context_virtual},
  };
  for (const Refusal &refusal : refusals) {
      av::MockPlatform platform;
      MockKmt kmt;
      ScriptedAdapterLuids luids;
      av::LockTrace trace;
      av::RecordingThreadLock lock(trace, 'H');
      av::RecordingThreadLock send_lock(trace, 'S');
      av::RecordingLog log;
      IcdRoot root(platform, kmt, luids, lock, send_lock, log, av::icd::icd_binding);
      if (!refusal.no_adapter) {
          kmt.add_adapter(av::PciIdentity(av::AdapterScenario::virtio_vendor, av::AdapterScenario::virtio_gpu_device,
                                          av::AdapterScenario::virtio_vendor, av::AdapterScenario::rvk_subsystem),
                          av::AdapterScenario::wddm_3_2);
      }
      if (refusal.no_luid) luids.refuse();
      if (refusal.call != MockKmt::Call::count) kmt.fail_at(refusal.call, 1u, MockKmt::invalid_parameter);
      VkInstanceCreateInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
      VkInstance instance = VK_NULL_HANDLE;
      const VkResult result = reinterpret_cast<CreateInstance>(root.instance_proc_addr(VK_NULL_HANDLE, "vkCreateInstance"))(
          &info, nullptr, &instance);
      if (result != VK_ERROR_INCOMPATIBLE_DRIVER)
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "%s: result %d", refusal.why, result);
      CHECK(root.invariants_hold());
      CHECK(instance == VK_NULL_HANDLE);
      CHECK_EQ(kmt.open_objects(), 0u);
      CHECK_EQ(kmt.commands(), 0u);
      CHECK_EQ(log.holding("no Vulkan connection to an rvkgpu adapter, status 6"), 1u);
      CHECK_EQ(platform.live_allocations(), 0u);
  }
}

TEST(icd_root, create_instance_passes_on_the_hosts_refusal_and_closes_the_connection)
{ IcdRig rig;
  rig.host().vk_handle(vk_error_initialization_failed, 0u);
  VkInstanceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  VkInstance instance = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<CreateInstance>(VK_NULL_HANDLE, "vkCreateInstance")(&info, nullptr, &instance),
           vk_error_initialization_failed);
  CHECK(rig.root().invariants_hold());
  CHECK(instance == VK_NULL_HANDLE);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.log().holding("the host refused, result 4294967293"), 1u);
  CHECK_EQ(rig.platform().live_allocations(), 0u); }

TEST(icd_root, create_instance_with_no_memory_for_its_connection_answers_out_of_host_memory)
{ IcdRig rig;
  rig.platform().fail_allocation_at(1u);
  VkInstanceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  VkInstance instance = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<CreateInstance>(VK_NULL_HANDLE, "vkCreateInstance")(&info, nullptr, &instance),
           VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK(rig.root().invariants_hold());
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.kmt().commands(), 0u);
  CHECK_EQ(rig.log().holding("status 2"), 1u); }

/* The host made an instance, but no handle could be made for it: the context,
 * and with it the host's instance, is closed rather than leaked. */
TEST(icd_root, create_instance_with_no_memory_for_its_handle_closes_the_hosts_instance)
{ IcdRig rig;
  rig.platform().fail_allocation_at(2u);
  VkInstanceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  VkInstance instance = VK_NULL_HANDLE;
  rig.host().vk_handle(0, ScriptedHost::vk_instance);
  CHECK_EQ(rig.entry<CreateInstance>(VK_NULL_HANDLE, "vkCreateInstance")(&info, nullptr, &instance),
           VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK(rig.root().invariants_hold());
  CHECK_EQ(rig.kmt().commands(), 1u);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.log().holding("no handle for host object, status 2"), 1u);
  CHECK_EQ(rig.platform().live_allocations(), 0u); }

TEST(icd_root, the_adapters_luid_answers_the_same_physical_device_enumeration_does)
{ IcdRig rig;
  const VkInstance instance = rig.create_instance();
  const EnumerateAdapter by_luid = rig.entry<EnumerateAdapter>(VK_NULL_HANDLE, "vk_icdEnumerateAdapterPhysicalDevices");
  std::uint32_t count = 0u;
  rig.host().vk_count(0, 1u);
  CHECK_EQ(by_luid(instance, ScriptedAdapterLuids::default_luid, &count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, 1u);
  rig.trace().clear();
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  rig.host().vk_physical_devices(0, 1u);
  CHECK_EQ(by_luid(instance, ScriptedAdapterLuids::default_luid, &count, &physical_device), VK_SUCCESS);
  /* The instance is unwrapped under the root's lock inside the send lock, never
   * the other way round. */
  CHECK(std::strcmp(rig.trace().text(), "HhHhSHhsHh") == 0);
  CHECK(physical_device != VK_NULL_HANDLE);
  CHECK_EQ(av::vkproto::handle_to_wire(physical_device), ScriptedHost::vk_physical_device);
  const EnumeratePhysical enumerate = rig.entry<EnumeratePhysical>(instance, "vkEnumeratePhysicalDevices");
  VkPhysicalDevice again = VK_NULL_HANDLE;
  count = 1u;
  rig.host().vk_physical_devices(0, 1u);
  CHECK_EQ(enumerate(instance, &count, &again), VK_SUCCESS);
  CHECK(rig.root().invariants_hold());
  CHECK(again == physical_device);
  WireReader sent = body_of(rig.kmt(), 2u);
  CHECK_EQ(sent.read_u64(), ScriptedHost::vk_instance);
  rig.destroy_instance(instance);
  CHECK(rig.root().invariants_hold()); }

TEST(icd_root, another_adapters_luid_answers_no_physical_device_without_asking_the_host)
{ IcdRig rig;
  const VkInstance instance = rig.create_instance();
  std::uint32_t count = 4u;
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  const EnumerateAdapter by_luid = rig.entry<EnumerateAdapter>(VK_NULL_HANDLE, "vk_icdEnumerateAdapterPhysicalDevices");
  CHECK_EQ(by_luid(instance, other_luid, &count, &physical_device), VK_SUCCESS);
  CHECK_EQ(count, 0u);
  CHECK(physical_device == VK_NULL_HANDLE);
  CHECK_EQ(rig.kmt().commands(), 1u);
  rig.destroy_instance(instance); }

TEST(icd_root, enumerating_adapters_on_an_instance_it_did_not_make_is_refused)
{ IcdRig rig;
  int stack = 0;
  std::uint32_t count = 1u;
  const EnumerateAdapter by_luid = rig.entry<EnumerateAdapter>(VK_NULL_HANDLE, "vk_icdEnumerateAdapterPhysicalDevices");
  CHECK_EQ(by_luid(reinterpret_cast<VkInstance>(&stack), ScriptedAdapterLuids::default_luid, &count, nullptr),
           VK_ERROR_INITIALIZATION_FAILED);
  CHECK_EQ(count, 1u);
  CHECK_EQ(rig.log().holding("did not make"), 1u); }

TEST(icd_root, enumerating_adapters_passes_on_the_hosts_answer)
{ IcdRig rig;
  const VkInstance instance = rig.create_instance();
  const EnumerateAdapter by_luid = rig.entry<EnumerateAdapter>(VK_NULL_HANDLE, "vk_icdEnumerateAdapterPhysicalDevices");
  VkPhysicalDevice physical_devices[1] = {};
  std::uint32_t count = 1u;
  rig.host().vk_physical_devices(vk_error_initialization_failed, 0u);
  CHECK_EQ(by_luid(instance, ScriptedAdapterLuids::default_luid, &count, physical_devices),
           vk_error_initialization_failed);
  CHECK(physical_devices[0] == VK_NULL_HANDLE);
  /* Room for one of two: the one there is, and VK_INCOMPLETE. */
  count = 1u;
  rig.host().vk_physical_devices(VK_INCOMPLETE, 1u);
  CHECK_EQ(by_luid(instance, ScriptedAdapterLuids::default_luid, &count, physical_devices), VK_INCOMPLETE);
  CHECK(physical_devices[0] != VK_NULL_HANDLE);
  CHECK(rig.root().invariants_hold());
  rig.destroy_instance(instance); }

TEST(icd_root, a_physical_device_no_handle_could_be_made_for_answers_out_of_host_memory)
{ IcdRig rig;
  const VkInstance instance = rig.create_instance();
  Connection *const connection = av::icd::connection_of(instance);
  /* Fill the instance's chunk of handles, so the physical device needs a new one. */
  for (std::uint64_t host = 1u; host < av::ObjectPool<LoaderHandle>::chunk_slots; host++)
      CHECK(rig.root().wrap(Kind::physical_device, 0x90000u + host, connection) != nullptr);
  rig.platform().fail_allocation_at(1u);
  std::uint32_t count = 1u;
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  rig.host().vk_physical_devices(0, 1u);
  const EnumerateAdapter by_luid = rig.entry<EnumerateAdapter>(VK_NULL_HANDLE, "vk_icdEnumerateAdapterPhysicalDevices");
  CHECK_EQ(by_luid(instance, ScriptedAdapterLuids::default_luid, &count, &physical_device), VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK(rig.root().invariants_hold());
  rig.destroy_instance(instance);
  CHECK_EQ(rig.platform().live_allocations(), 0u); }

TEST(icd_root, destroying_a_device_sends_the_destroy_and_ends_its_handle)
{ IcdRig rig;
  const VkInstance instance = rig.create_instance();
  const VkPhysicalDevice physical_device = rig.physical_device(instance);
  const VkDevice device = create_device(rig, instance, physical_device);
  CHECK(rig.root().invariants_hold());
  CHECK(device != VK_NULL_HANDLE);
  CHECK_EQ(av::vkproto::handle_to_wire(device), ScriptedHost::vk_device);
  const std::uint32_t sent = rig.kmt().commands();
  rig.entry<DestroyDevice>(instance, "vkDestroyDevice")(device, nullptr);
  CHECK(rig.root().invariants_hold());
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK_EQ(rig.kmt().command_id(sent), static_cast<std::uint32_t>(kCmd_vkDestroyDevice));
  WireReader destroyed = body_of(rig.kmt(), sent);
  CHECK_EQ(destroyed.read_u64(), ScriptedHost::vk_device);
  CHECK(av::icd::connection_of(device) == nullptr);
  CHECK_EQ(av::vkproto::handle_to_wire(device), 0u);
  CHECK(av::icd::connection_of(physical_device) != nullptr);
  rig.destroy_instance(instance); }

TEST(icd_root, destroying_a_null_or_foreign_device_sends_nothing)
{ IcdRig rig;
  const VkInstance instance = rig.create_instance();
  const DestroyDevice destroy = rig.entry<DestroyDevice>(instance, "vkDestroyDevice");
  const std::uint32_t sent = rig.kmt().commands();
  destroy(VK_NULL_HANDLE, nullptr);
  int stack = 0;
  destroy(reinterpret_cast<VkDevice>(&stack), nullptr);
  /* An instance is not a device. */
  destroy(reinterpret_cast<VkDevice>(instance), nullptr);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(av::icd::connection_of(instance) != nullptr);
  CHECK(rig.root().invariants_hold());
  rig.destroy_instance(instance); }

/* The instance's handles all end with it, whatever was made through it. */
TEST(icd_root, destroying_an_instance_ends_every_handle_made_through_it)
{ IcdRig rig;
  const VkInstance instance = rig.create_instance();
  const VkPhysicalDevice physical_device = rig.physical_device(instance);
  const VkDevice device = create_device(rig, instance, physical_device);
  const VkQueue queue = get_queue(rig, instance, device);
  CHECK(queue != VK_NULL_HANDLE);
  CHECK(rig.root().invariants_hold());
  rig.destroy_instance(instance);
  CHECK(rig.root().invariants_hold());
  CHECK(av::icd::connection_of(queue) == nullptr);
  CHECK(av::icd::connection_of(device) == nullptr);
  CHECK(av::icd::connection_of(physical_device) == nullptr);
  CHECK_EQ(rig.platform().live_allocations(), 0u); }

TEST(icd_root, queues_and_command_buffers_cross_as_the_hosts_handles)
{ IcdRig rig;
  const VkInstance instance = rig.create_instance();
  const VkDevice device = create_device(rig, instance, rig.physical_device(instance));
  const VkQueue queue = get_queue(rig, instance, device);
  CHECK_EQ(av::vkproto::handle_to_wire(queue), queue_host);
  CHECK(av::icd::connection_of(queue) == av::icd::connection_of(device));
  rig.host().vk_result(VK_SUCCESS);
  CHECK_EQ(rig.entry<QueueWaitIdle>(instance, "vkQueueWaitIdle")(queue), VK_SUCCESS);
  WireReader waited = body_of(rig.kmt(), rig.kmt().commands() - 1u);
  CHECK_EQ(waited.read_u64(), queue_host);
  VkCommandBufferAllocateInfo allocate = {};
  allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocate.commandBufferCount = 1u;
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  rig.host().vk_handle(0, command_buffer_host);
  CHECK_EQ(rig.entry<AllocateCommandBuffers>(instance, "vkAllocateCommandBuffers")(device, &allocate, &command_buffer),
           VK_SUCCESS);
  CHECK(rig.root().invariants_hold());
  CHECK_EQ(av::vkproto::handle_to_wire(command_buffer), command_buffer_host);
  rig.entry<CmdDraw>(instance, "vkCmdDraw")(command_buffer, 3u, 1u, 0u, 0u);
  WireReader drawn = body_of(rig.kmt(), rig.kmt().commands() - 1u);
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u), static_cast<std::uint32_t>(kCmd_vkCmdDraw));
  CHECK_EQ(drawn.read_u64(), command_buffer_host);
  CHECK_EQ(drawn.read_u32(), 3u);
  rig.host().vk_result(VK_SUCCESS);
  CHECK_EQ(rig.entry<ResetCommandBuffer>(instance, "vkResetCommandBuffer")(command_buffer, 0u), VK_SUCCESS);
  CHECK(av::icd::connection_of(command_buffer) == av::icd::connection_of(instance));
  rig.destroy_instance(instance);
  CHECK(rig.root().invariants_hold()); }

TEST(icd_root, properties_through_the_root_carry_the_adapters_luid)
{ IcdRig rig;
  rig.luids().answer(0x0000000300000077ull);
  const VkInstance instance = rig.create_instance();
  const EnumerateAdapter by_luid = rig.entry<EnumerateAdapter>(VK_NULL_HANDLE, "vk_icdEnumerateAdapterPhysicalDevices");
  std::uint32_t count = 1u;
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  rig.host().vk_physical_devices(0, 1u);
  CHECK_EQ(by_luid(instance, 0x0000000300000077ull, &count, &physical_device), VK_SUCCESS);
  /* A reply MockKmt can carry holds the result and nothing else: the view's
   * answer is then the zeroed properties with the LUID written. */
  rig.host().vk_result(VK_SUCCESS);
  VkPhysicalDeviceIDProperties identity = {};
  identity.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
  VkPhysicalDeviceProperties2 properties2 = {};
  properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  properties2.pNext = &identity;
  properties2.properties.apiVersion = 99u;
  rig.entry<Properties2>(instance, "vkGetPhysicalDeviceProperties2")(physical_device, &properties2);
  const std::uint64_t luid = 0x0000000300000077ull;
  CHECK(std::memcmp(identity.deviceLUID, &luid, sizeof luid) == 0);
  CHECK_EQ(identity.deviceLUIDValid, VK_TRUE);
  CHECK_EQ(properties2.properties.apiVersion, 0u);
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u),
           static_cast<std::uint32_t>(kCmd_vkGetPhysicalDeviceProperties2));
  rig.host().vk_result(VK_SUCCESS);
  VkPhysicalDeviceProperties properties = {};
  properties.apiVersion = 99u;
  rig.entry<Properties>(instance, "vkGetPhysicalDeviceProperties")(physical_device, &properties);
  CHECK_EQ(properties.apiVersion, 0u);
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u),
           static_cast<std::uint32_t>(kCmd_vkGetPhysicalDeviceProperties));
  rig.destroy_instance(instance); }

TEST(icd_root, properties_of_a_physical_device_it_did_not_make_are_not_asked_for)
{ IcdRig rig;
  const VkInstance instance = rig.create_instance();
  const std::uint32_t sent = rig.kmt().commands();
  int stack = 0;
  VkPhysicalDeviceProperties properties = {};
  properties.apiVersion = 99u;
  rig.entry<Properties>(instance, "vkGetPhysicalDeviceProperties")(reinterpret_cast<VkPhysicalDevice>(&stack),
                                                                    &properties);
  VkPhysicalDeviceProperties2 properties2 = {};
  properties2.properties.apiVersion = 99u;
  rig.entry<Properties2>(instance, "vkGetPhysicalDeviceProperties2")(reinterpret_cast<VkPhysicalDevice>(instance),
                                                                      &properties2);
  CHECK_EQ(properties.apiVersion, 99u);
  CHECK_EQ(properties2.properties.apiVersion, 99u);
  CHECK_EQ(rig.kmt().commands(), sent);
  rig.destroy_instance(instance); }

TEST(icd_root, the_hooks_answer_nothing_for_what_it_did_not_make)
{ IcdRig rig;
  int stack = 0;
  CHECK(av::icd::acquire_default_connection() == nullptr);
  CHECK(av::icd::connection_of(reinterpret_cast<VkInstance>(&stack)) == nullptr);
  CHECK(av::icd::connection_of(reinterpret_cast<VkPhysicalDevice>(&stack)) == nullptr);
  CHECK(av::icd::connection_of(reinterpret_cast<VkDevice>(&stack)) == nullptr);
  CHECK(av::icd::connection_of(reinterpret_cast<VkQueue>(&stack)) == nullptr);
  CHECK(av::icd::connection_of(reinterpret_cast<VkCommandBuffer>(&stack)) == nullptr);
  CHECK_EQ(av::vkproto::handle_to_wire(static_cast<VkInstance>(VK_NULL_HANDLE)), 0u);
  CHECK_EQ(av::vkproto::handle_to_wire(reinterpret_cast<VkPhysicalDevice>(&stack)), 0u);
  CHECK_EQ(av::vkproto::handle_to_wire(reinterpret_cast<VkQueue>(&stack)), 0u);
  CHECK_EQ(av::vkproto::handle_to_wire(reinterpret_cast<VkCommandBuffer>(&stack)), 0u);
  /* The host never names an object 0: wrapping it makes no handle. */
  CHECK(av::icd::wrap_server_handle<VkQueue>(0u, reinterpret_cast<Connection *>(&stack)) == VK_NULL_HANDLE);
  av::icd::release_connection(reinterpret_cast<Connection *>(&stack));
  CHECK_EQ(rig.log().holding("never opened"), 1u);
  av::icd::destroy_wrapper(reinterpret_cast<VkInstance>(&stack));
  CHECK_EQ(rig.log().holding("did not make"), 1u);
  CHECK(rig.root().invariants_hold()); }

TEST(icd_root, invariants_see_a_connection_left_pending_closed_or_unowned_and_a_broken_handle)
{ IcdRig rig;
  const VkInstance instance = rig.create_instance();
  Connection *const connection = av::icd::connection_of(instance);
  CHECK(rig.root().invariants_hold());
  IcdRootProbe::leave_pending(rig.root(), connection);
  CHECK(!rig.root().invariants_hold());
  CHECK(rig.root().acquire_connection() == connection);
  CHECK(rig.root().invariants_hold());
  /* A handle on a connection this root does not hold. */
  int stack = 0;
  void *const unowned = rig.root().wrap(Kind::instance, 0x1u, reinterpret_cast<Connection *>(&stack));
  CHECK(unowned != nullptr);
  CHECK(!rig.root().invariants_hold());
  rig.root().destroy_wrapper(unowned, Kind::instance);
  CHECK(rig.root().invariants_hold());
  /* A handle the host would have named 0. */
  const VkPhysicalDevice physical_device = rig.physical_device(instance);
  LoaderHandleProbe::set_host(*static_cast<LoaderHandle *>(static_cast<void *>(physical_device)), 0u);
  CHECK(!rig.root().invariants_hold());
  LoaderHandleProbe::set_host(*static_cast<LoaderHandle *>(static_cast<void *>(physical_device)),
                              ScriptedHost::vk_physical_device);
  CHECK(rig.root().invariants_hold());
  /* An open connection whose device has lost its paging queue. */
  av::GuestConnection &guest = ConnectionProbe::guest(*connection);
  const std::uint32_t queue = guest.queue();
  GuestConnectionProbe::set_queue(guest, 0u);
  CHECK(!rig.root().invariants_hold());
  GuestConnectionProbe::set_queue(guest, queue);
  CHECK(rig.root().invariants_hold());
  /* Closed under the root: every command it would send fails, and the root
   * says so. */
  connection->close();
  CHECK(!rig.root().invariants_hold());
  rig.destroy_instance(instance);
  CHECK(rig.root().invariants_hold()); }
