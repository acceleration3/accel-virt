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
#include "harness/icd/scripted_window_surface.hpp"
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
#include "proto/wire/wire_writer.hpp"
#include "proto/submission/command_header.hpp"

#include <cstdint>
#include <cstring>

using av::CommandHeader;
using av::IcdRig;
using av::MockKmt;
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
using AllocateCommandBuffers = VkResult (*)(VkDevice, const VkCommandBufferAllocateInfo *, VkCommandBuffer *);
using CmdDraw = void (*)(VkCommandBuffer, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
using ResetCommandBuffer = VkResult (*)(VkCommandBuffer, VkCommandBufferResetFlags);
using EndCommandBuffer = VkResult (*)(VkCommandBuffer);
using Properties = void (*)(VkPhysicalDevice, VkPhysicalDeviceProperties *);
using Properties2 = void (*)(VkPhysicalDevice, VkPhysicalDeviceProperties2 *);

constexpr std::int32_t  vk_error_initialization_failed = -3;
constexpr std::uint32_t vulkan_1_3 = 0x00403000u;
constexpr std::uint64_t other_luid = 0x0000000200001235ull;
constexpr std::uint64_t queue_host = 0x6600u;
constexpr std::uint64_t command_buffer_host = 0x7700u;
/* What one open connection holds in MockKmt (test_connection.cpp). */
constexpr std::uint32_t connection_objects = 7u;

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
  /* The generated commands the ICD answers itself. */
  const char *const overridden[] = {
      "vkCreateInstance", "vkDestroyDevice", "vkGetPhysicalDeviceProperties", "vkGetPhysicalDeviceProperties2",
      "vkGetDeviceQueue", "vkGetDeviceQueue2", "vkAllocateCommandBuffers", "vkFreeCommandBuffers",
      "vkBeginCommandBuffer", "vkEndCommandBuffer", "vkResetCommandBuffer", "vkWaitForFences", "vkWaitSemaphores",
      "vkWaitSemaphoresKHR", "vkQueueWaitIdle", "vkDeviceWaitIdle", "vkEnumerateDeviceExtensionProperties",
      "vkCreateDevice", "vkDestroyInstance", "vkGetPhysicalDeviceMemoryProperties",
      "vkGetPhysicalDeviceMemoryProperties2", "vkAllocateMemory", "vkFreeMemory", "vkFlushMappedMemoryRanges",
      "vkInvalidateMappedMemoryRanges",
      /* The four that keep the descriptor template table honest (feature 047):
       * each forwards to its generated wrapper and remembers or forgets what
       * it made around it. */
      "vkCreateDescriptorUpdateTemplate", "vkCreateDescriptorUpdateTemplateKHR",
      "vkDestroyDescriptorUpdateTemplate", "vkDestroyDescriptorUpdateTemplateKHR",
      /* And feature 047's US3: a semaphore destroyed takes any handle that
       * named it with it, and the two external-property queries are answered
       * over the host's fd rather than passed on as Win32 (research.md §10). */
      "vkDestroySemaphore", "vkGetPhysicalDeviceExternalBufferProperties",
      "vkGetPhysicalDeviceExternalBufferPropertiesKHR", "vkGetPhysicalDeviceExternalSemaphoreProperties",
      "vkGetPhysicalDeviceExternalSemaphorePropertiesKHR"};
  std::uint32_t wrappers = 0u;
  for (const av::vkicd::generated::Entry &row : av::vkicd::generated::wrapper_entries()) {
      const PFN_vkVoidFunction found = root.instance_proc_addr(instance, row.name);
      CHECK(found != nullptr);
      bool local = false;
      for (const char *name : overridden) local = local || std::strcmp(name, row.name) == 0;
      CHECK((found == row.fn) != local);
      if (found == row.fn) wrappers++;
  }
  CHECK_EQ(wrappers, av::vkicd::generated::wrapper_entries().size() - sizeof overridden / sizeof overridden[0]);
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
  std::uint32_t through_an_instance = 0u;
  for (const char *name : kManualEntryPoints) {
      const PFN_vkVoidFunction found = rig.root().instance_proc_addr(instance, name);
      if (found == nullptr) continue;
      answered++;
      /* Mapping (T035), device dispatch and the WSI (feature 046) are the
       * ICD's own too, and answer only through an instance. */
      if (rig.root().instance_proc_addr(VK_NULL_HANDLE, name) == nullptr) {
          through_an_instance++;
          continue;
      }
      CHECK(found == rig.root().instance_proc_addr(VK_NULL_HANDLE, name));
  }
  CHECK(rig.root().instance_proc_addr(instance, "vkMapMemory") == entry_of(&av::icd::local_vkMapMemory));
  CHECK(rig.root().instance_proc_addr(VK_NULL_HANDLE, "vkMapMemory") == nullptr);
  CHECK(rig.root().instance_proc_addr(instance, "vkCreateSwapchainKHR") ==
        entry_of(&av::icd::local_vkCreateSwapchainKHR));
  /* The window-system entries of another platform are not this driver's. */
  CHECK(rig.root().instance_proc_addr(instance, "vkCreateXlibSurfaceKHR") == nullptr);
  CHECK(rig.root().instance_proc_addr(instance, "vkCreateHeadlessSurfaceEXT") == nullptr);
  /* Three before the WSI, the twelve window-system entries this platform's
   * manual list names (the two Win32-only ones are not on it), feature 047's
   * two mapping entries and its four Win32 external-object ones. */
  CHECK_EQ(answered, 23u);
  CHECK_EQ(through_an_instance, 21u);
  CHECK(rig.root().instance_proc_addr(instance, "vkGetMemoryWin32HandleKHR") ==
        entry_of(&av::icd::local_vkGetMemoryWin32HandleKHR));
  CHECK(rig.root().instance_proc_addr(instance, "vkImportSemaphoreWin32HandleKHR") ==
        entry_of(&av::icd::local_vkImportSemaphoreWin32HandleKHR));
  CHECK(rig.root().instance_proc_addr(instance, "vkMapMemory2KHR") == entry_of(&av::icd::local_vkMapMemory2KHR));
  CHECK(rig.root().instance_proc_addr(instance, "vkUnmapMemory2KHR") == entry_of(&av::icd::local_vkUnmapMemory2KHR));
  rig.destroy_instance(instance); }

/* The Windows loader drops a driver that answers any of these null for its
 * instance (Vulkan-Loader, loader_icd_init_entries: LOOKUP_REQUIRED_GIPA), and
 * every application then sees no device at all. */
TEST(icd_root, an_instance_answers_every_entry_point_the_loader_requires)
{ IcdRig rig;
  const VkInstance instance = rig.create_instance();
  const char *const required[] = {
      "vkDestroyInstance", "vkEnumeratePhysicalDevices", "vkGetPhysicalDeviceFeatures",
      "vkGetPhysicalDeviceFormatProperties", "vkGetPhysicalDeviceImageFormatProperties",
      "vkGetPhysicalDeviceProperties", "vkGetPhysicalDeviceQueueFamilyProperties",
      "vkGetPhysicalDeviceMemoryProperties", "vkGetDeviceProcAddr", "vkCreateDevice",
      "vkEnumerateDeviceExtensionProperties", "vkGetPhysicalDeviceSparseImageFormatProperties"};
  for (const char *name : required) CHECK(rig.root().instance_proc_addr(instance, name) != nullptr);
  CHECK(rig.root().instance_proc_addr(instance, "vkGetDeviceProcAddr") ==
        entry_of(&av::icd::local_vkGetDeviceProcAddr));
  rig.destroy_instance(instance); }

using GetDeviceProcAddr = PFN_vkVoidFunction (*)(VkDevice, const char *);
using CreateDeviceEntry = VkResult (*)(VkPhysicalDevice, const VkDeviceCreateInfo *, const VkAllocationCallbacks *,
                                       VkDevice *);

/* The loader fills a device's dispatch table from this: the device-level
 * entry the instance would answer, local rows first, and null for a command
 * dispatched on anything but a device, queue or command buffer. */
TEST(icd_root, a_device_it_made_answers_device_level_entry_points_and_no_other)
{ IcdRig rig;
  const VkInstance instance = rig.create_instance();
  const VkPhysicalDevice physical_device = rig.physical_device(instance);
  VkDeviceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  rig.host().vk_handle(0, ScriptedHost::vk_device);
  VkDevice device = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<CreateDeviceEntry>(instance, "vkCreateDevice")(physical_device, &info, nullptr, &device),
           VK_SUCCESS);
  const GetDeviceProcAddr device_proc_addr = rig.entry<GetDeviceProcAddr>(instance, "vkGetDeviceProcAddr");
  CHECK(device_proc_addr(device, "vkDestroyDevice") == entry_of(&av::icd::local_vkDestroyDevice));
  CHECK(device_proc_addr(device, "vkAllocateMemory") == entry_of(&av::icd::local_vkAllocateMemory));
  CHECK(device_proc_addr(device, "vkMapMemory") == entry_of(&av::icd::local_vkMapMemory));
  /* A device-level command itself, which the spec has answer through a device. */
  CHECK(device_proc_addr(device, "vkGetDeviceProcAddr") == entry_of(&av::icd::local_vkGetDeviceProcAddr));
  CHECK(device_proc_addr(device, "vkCreateBuffer") ==
        entry_of(&av::vkicd::generated::Wrap_vkCreateBuffer));
  CHECK(device_proc_addr(device, "vkCmdDraw") == entry_of(&av::vkicd::generated::Wrap_vkCmdDraw));
  std::uint32_t device_level = 0u;
  for (const av::vkicd::generated::Entry &row : av::vkicd::generated::wrapper_entries()) {
      const PFN_vkVoidFunction found = device_proc_addr(device, row.name);
      if (row.level != av::vkicd::generated::DispatchLevel::device) {
          CHECK(found == nullptr);
          continue;
      }
      CHECK(found == rig.root().instance_proc_addr(instance, row.name));
      device_level++;
  }
  CHECK(device_level > 100u);
  const char *const not_device_level[] = {"vkCreateDevice", "vkGetPhysicalDeviceProperties2", "vkDestroyInstance",
                                          "vkCreateInstance",
                                          "vk_icdEnumerateAdapterPhysicalDevices", "vkNotAnEntryPoint", "",
                                          nullptr};
  for (const char *name : not_device_level) CHECK(device_proc_addr(device, name) == nullptr);
  /* Anything but a live device of this ICD answers nothing. */
  int stack = 0;
  CHECK(device_proc_addr(reinterpret_cast<VkDevice>(&stack), "vkDestroyDevice") == nullptr);
  CHECK(device_proc_addr(reinterpret_cast<VkDevice>(physical_device), "vkDestroyDevice") == nullptr);
  CHECK(device_proc_addr(VK_NULL_HANDLE, "vkDestroyDevice") == nullptr);
  rig.entry<void (*)(VkDevice, const VkAllocationCallbacks *)>(instance, "vkDestroyDevice")(device, nullptr);
  CHECK(device_proc_addr(device, "vkDestroyDevice") == nullptr);
  rig.destroy_instance(instance);
  CHECK(rig.root().invariants_hold()); }

/* The instance extensions are VK_KHR_get_physical_device_properties2 - its
 * entry points are the ICD's own, and a loader client such as vulkaninfo asks
 * for the physical device's LUID only when the extension is listed - the two
 * surface extensions the WSI answers (feature 046), and the *2 surface queries
 * DXVK resolves whether or not it enabled them (feature 047). The count and
 * array protocol is the loader's: VK_INCOMPLETE with what fits, nothing
 * written past the capacity. */
TEST(icd_root, reports_properties2_and_the_surface_extensions_as_its_instance_extensions)
{ IcdRig rig;
  const InstanceExtensions enumerate =
      rig.entry<InstanceExtensions>(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");
  std::uint32_t count = 6u;
  CHECK_EQ(enumerate(nullptr, &count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, 4u);
  VkExtensionProperties properties[5] = {};
  properties[0].specVersion = 9u;
  count = 0u;
  CHECK_EQ(enumerate(nullptr, &count, properties), VK_INCOMPLETE);
  CHECK_EQ(count, 0u);
  CHECK_EQ(properties[0].specVersion, 9u);
  count = 2u;
  CHECK_EQ(enumerate(nullptr, &count, properties), VK_INCOMPLETE);
  CHECK_EQ(count, 2u);
  count = 5u;
  CHECK_EQ(enumerate(nullptr, &count, properties), VK_SUCCESS);
  CHECK_EQ(count, 4u);
  CHECK(__builtin_strcmp(properties[0].extensionName, "VK_KHR_get_physical_device_properties2") == 0);
  CHECK_EQ(properties[0].specVersion, 2u);
  CHECK(__builtin_strcmp(properties[1].extensionName, "VK_KHR_surface") == 0);
  CHECK_EQ(properties[1].specVersion, 25u);
  CHECK(__builtin_strcmp(properties[2].extensionName, "VK_KHR_win32_surface") == 0);
  CHECK_EQ(properties[2].specVersion, 6u);
  CHECK(__builtin_strcmp(properties[3].extensionName, "VK_KHR_get_surface_capabilities2") == 0);
  CHECK_EQ(properties[3].specVersion, 1u);
  CHECK_EQ(properties[4].specVersion, 0u);
  bool implemented = false;
  for (const char *const name : av::vkproto::kImplementedInstanceExtensions)
    implemented = implemented || __builtin_strcmp(name, properties[0].extensionName) == 0;
  CHECK(implemented);
  count = 4u;
  CHECK_EQ(enumerate("VK_LAYER_KHRONOS_validation", &count, properties), VK_ERROR_LAYER_NOT_PRESENT);
  CHECK_EQ(count, 4u);
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

/* The loader dispatches whatever this answers on a physical device: a device or
 * instance command answered here would be called with the wrong object. */
TEST(icd_root, answers_only_physical_device_level_entry_points_to_get_physical_device_proc_addr)
{ IcdRig rig;
  const VkInstance instance = rig.create_instance();
  const PhysicalProcAddr proc_addr = rig.entry<PhysicalProcAddr>(VK_NULL_HANDLE, "vk_icdGetPhysicalDeviceProcAddr");
  CHECK(proc_addr(instance, "vkGetPhysicalDeviceFeatures") ==
        entry_of(&av::vkicd::generated::Wrap_vkGetPhysicalDeviceFeatures));
  CHECK(proc_addr(instance, "vkGetPhysicalDeviceProperties2") ==
        entry_of(&av::icd::local_vkGetPhysicalDeviceProperties2));
  CHECK(proc_addr(VK_NULL_HANDLE, "vkGetPhysicalDeviceProperties") ==
        entry_of(&av::icd::local_vkGetPhysicalDeviceProperties));
  const char *const other_levels[] = {"vkCreateBuffer", "vkDestroyDevice", "vkQueueWaitIdle", "vkCmdDraw",
                                      "vkEnumeratePhysicalDevices", "vkCreateInstance",
                                      "vk_icdEnumerateAdapterPhysicalDevices", "vkNotAnEntryPoint", "", nullptr};
  for (const char *name : other_levels) CHECK(proc_addr(instance, name) == nullptr);
  CHECK(rig.root().physical_device_proc_addr("vkGetPhysicalDeviceMemoryProperties") ==
        entry_of(&av::icd::local_vkGetPhysicalDeviceMemoryProperties));
  CHECK(rig.root().physical_device_proc_addr("vkGetPhysicalDeviceSparseImageFormatProperties") ==
        entry_of(&av::vkicd::generated::Wrap_vkGetPhysicalDeviceSparseImageFormatProperties));
  CHECK(rig.root().physical_device_proc_addr("vkAllocateMemory") == nullptr);
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
  CHECK_EQ(connection->luid(), IcdRig::adapter_luid);
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
      av::LockTrace trace;
      av::RecordingThreadLock lock(trace, 'H');
      av::RecordingThreadLock send_lock(trace, 'S');
      av::RecordingThreadLock memory_lock(trace, 'M');
      av::RecordingLog log;
      av::ScriptedWindowSurface windows;
      IcdRoot root(platform, kmt, lock, send_lock, memory_lock, log, av::icd::icd_binding, windows);
      if (!refusal.no_adapter) {
          kmt.add_adapter(av::PciIdentity(av::AdapterScenario::virtio_vendor, av::AdapterScenario::virtio_gpu_device,
                                          av::AdapterScenario::virtio_vendor, av::AdapterScenario::rvk_subsystem),
                          av::AdapterScenario::wddm_3_2);
      }
      if (refusal.no_luid) kmt.fail_at(MockKmt::Call::adapter_luid, 1u, MockKmt::invalid_parameter);
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

/* The connection's pool chunk, then its 256 KiB of staging. */
TEST(icd_root, create_instance_with_no_memory_for_its_connection_answers_out_of_host_memory)
{ for (std::uint32_t refused = 1u; refused <= 2u; refused++) {
      IcdRig rig;
      rig.platform().fail_allocation_at(refused);
      VkInstanceCreateInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
      VkInstance instance = VK_NULL_HANDLE;
      CHECK_EQ(rig.entry<CreateInstance>(VK_NULL_HANDLE, "vkCreateInstance")(&info, nullptr, &instance),
               VK_ERROR_OUT_OF_HOST_MEMORY);
      CHECK(rig.root().invariants_hold());
      CHECK(instance == VK_NULL_HANDLE);
      CHECK_EQ(rig.kmt().open_objects(), 0u);
      CHECK_EQ(rig.kmt().commands(), 0u);
      CHECK_EQ(rig.log().holding("status 2"), 1u);
      CHECK_EQ(rig.platform().live_allocations(), 0u);
  }
}

/* The host made an instance, but no handle could be made for it: the context,
 * and with it the host's instance, is closed rather than leaked. */
TEST(icd_root, create_instance_with_no_memory_for_its_handle_closes_the_hosts_instance)
{ IcdRig rig;
  rig.platform().fail_allocation_at(3u);
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
  CHECK_EQ(by_luid(instance, IcdRig::adapter_luid, &count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, 1u);
  rig.trace().clear();
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  rig.host().vk_physical_devices(0, 1u);
  CHECK_EQ(by_luid(instance, IcdRig::adapter_luid, &count, &physical_device), VK_SUCCESS);
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

namespace {

/* A host's answer to vkEnumeratePhysicalDeviceGroups: the result, the count and
 * one group naming `named` devices from vk_physical_device on, claiming
 * `claimed` of them. */
void queue_group(MockKmt &kmt, std::uint32_t claimed, std::uint32_t named)
{
    std::uint8_t    answer[MockKmt::max_answer] = {};
    av::WireWriter  writer(answer, sizeof answer);
    VkPhysicalDeviceGroupProperties group = {};
    group.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES;
    group.physicalDeviceCount = claimed;
    for (std::uint32_t index = 0u; index < named; index++)
        group.physicalDevices[index] = reinterpret_cast<VkPhysicalDevice>(ScriptedHost::vk_physical_device + index);
    writer.write_pod<std::int32_t>(VK_SUCCESS);
    writer.write_pod<std::uint32_t>(1u);
    av::vkproto::serialize_VkPhysicalDeviceGroupProperties(writer, group);
    CHECK_STATUS(writer.status(), Status::ok);
    kmt.queue_answer(answer, static_cast<std::uint32_t>(writer.offset()));
}

}  // namespace

TEST(icd_root, a_device_groups_physical_device_is_the_handle_enumeration_answered)
{ IcdRig rig;
  const VkInstance instance = rig.create_instance();
  const VkPhysicalDevice physical_device = rig.physical_device(instance);
  using EnumerateGroups = VkResult (*)(VkInstance, std::uint32_t *, VkPhysicalDeviceGroupProperties *);
  const char *const spellings[] = {"vkEnumeratePhysicalDeviceGroups", "vkEnumeratePhysicalDeviceGroupsKHR"};
  for (const char *name : spellings) {
      VkPhysicalDeviceGroupProperties groups[1] = {};
      std::uint32_t count = 1u;
      queue_group(rig.kmt(), 1u, 1u);
      CHECK_EQ(rig.entry<EnumerateGroups>(instance, name)(instance, &count, groups), VK_SUCCESS);
      CHECK(rig.root().invariants_hold());
      CHECK_EQ(count, 1u);
      CHECK_EQ(groups[0].physicalDeviceCount, 1u);
      CHECK(groups[0].physicalDevices[0] == physical_device);
      CHECK(groups[0].physicalDevices[1] == VK_NULL_HANDLE);
  }
  rig.destroy_instance(instance); }

/* A full group has every one of its devices wrapped and nothing after it
 * touched; a group claiming more than the array holds is a reply the decoder
 * refuses, with the application's structures left unwrapped. */
TEST(icd_root, a_full_device_group_is_wrapped_and_an_overfull_one_refused)
{ IcdRig rig;
  const VkInstance instance = rig.create_instance();
  using EnumerateGroups = VkResult (*)(VkInstance, std::uint32_t *, VkPhysicalDeviceGroupProperties *);
  const EnumerateGroups enumerate = rig.entry<EnumerateGroups>(instance, "vkEnumeratePhysicalDeviceGroups");
  VkPhysicalDeviceGroupProperties groups[2] = {};
  groups[1].physicalDeviceCount = 0xC0FFEEu;
  std::uint32_t count = 1u;
  queue_group(rig.kmt(), VK_MAX_DEVICE_GROUP_SIZE, VK_MAX_DEVICE_GROUP_SIZE);
  CHECK_EQ(enumerate(instance, &count, groups), VK_SUCCESS);
  CHECK(rig.root().invariants_hold());
  for (std::uint32_t index = 0u; index < VK_MAX_DEVICE_GROUP_SIZE; index++) {
      CHECK(av::icd::connection_of(groups[0].physicalDevices[index]) != nullptr);
      CHECK_EQ(av::vkproto::handle_to_wire(groups[0].physicalDevices[index]), ScriptedHost::vk_physical_device + index);
  }
  CHECK_EQ(groups[1].physicalDeviceCount, 0xC0FFEEu);
  CHECK(groups[1].physicalDevices[0] == VK_NULL_HANDLE);
  VkPhysicalDeviceGroupProperties overfull[1] = {};
  count = 1u;
  queue_group(rig.kmt(), VK_MAX_DEVICE_GROUP_SIZE + 1u, 1u);
  CHECK_EQ(enumerate(instance, &count, overfull), VK_ERROR_DEVICE_LOST);
  CHECK(overfull[0].physicalDevices[0] == VK_NULL_HANDLE);
  /* A device the host names 0 has no handle to be: the call fails rather than
   * hand the application a null. */
  count = 1u;
  queue_group(rig.kmt(), 2u, 1u);
  CHECK_EQ(enumerate(instance, &count, groups), VK_ERROR_OUT_OF_HOST_MEMORY);
  /* Asking only how many wraps nothing. */
  count = 0u;
  rig.host().vk_count(VK_SUCCESS, 1u);
  CHECK_EQ(enumerate(instance, &count, nullptr), VK_SUCCESS);
  CHECK_EQ(count, 1u);
  /* A refusal is passed on with nothing wrapped. */
  count = 1u;
  rig.host().vk_count(vk_error_initialization_failed, 0u);
  CHECK_EQ(enumerate(instance, &count, groups), vk_error_initialization_failed);
  CHECK(rig.root().invariants_hold());
  rig.destroy_instance(instance);
  CHECK_EQ(rig.platform().live_allocations(), 0u); }

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
  CHECK_EQ(by_luid(reinterpret_cast<VkInstance>(&stack), IcdRig::adapter_luid, &count, nullptr),
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
  CHECK_EQ(by_luid(instance, IcdRig::adapter_luid, &count, physical_devices),
           vk_error_initialization_failed);
  CHECK(physical_devices[0] == VK_NULL_HANDLE);
  /* Room for one of two: the one there is, and VK_INCOMPLETE. */
  count = 1u;
  rig.host().vk_physical_devices(VK_INCOMPLETE, 1u);
  CHECK_EQ(by_luid(instance, IcdRig::adapter_luid, &count, physical_devices), VK_INCOMPLETE);
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
  CHECK_EQ(by_luid(instance, IcdRig::adapter_luid, &count, &physical_device), VK_ERROR_OUT_OF_HOST_MEMORY);
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
  VkCommandBufferAllocateInfo allocate = {};
  allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocate.commandBufferCount = 1u;
  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  rig.host().vk_handle(0, command_buffer_host);
  CHECK_EQ(rig.entry<AllocateCommandBuffers>(instance, "vkAllocateCommandBuffers")(device, &allocate, &command_buffer),
           VK_SUCCESS);
  CHECK(rig.root().invariants_hold());
  CHECK_EQ(av::vkproto::handle_to_wire(command_buffer), command_buffer_host);
  /* A recorded command crosses in its batch, at the end. */
  const std::uint32_t sent = rig.kmt().commands();
  rig.entry<CmdDraw>(instance, "vkCmdDraw")(command_buffer, 3u, 1u, 0u, 0u);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK_EQ(rig.entry<EndCommandBuffer>(instance, "vkEndCommandBuffer")(command_buffer), VK_SUCCESS);
  /* The draw, and the end that closes the batch after it. */
  WireReader drawn = body_of(rig.kmt(), rig.kmt().commands() - 2u);
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 2u), static_cast<std::uint32_t>(kCmd_vkCmdDraw));
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u), static_cast<std::uint32_t>(kCmd_vkEndCommandBuffer));
  CHECK_EQ(drawn.read_u64(), command_buffer_host);
  CHECK_EQ(drawn.read_u32(), 3u);
  CHECK_EQ(rig.entry<ResetCommandBuffer>(instance, "vkResetCommandBuffer")(command_buffer, 0u), VK_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u), static_cast<std::uint32_t>(kCmd_vkResetCommandBuffer));
  CHECK(av::icd::connection_of(command_buffer) == av::icd::connection_of(instance));
  rig.destroy_instance(instance);
  CHECK(rig.root().invariants_hold()); }

TEST(icd_root, properties_through_the_root_carry_the_adapters_luid)
{ IcdRig rig;
  rig.kmt().set_adapter_luid(0u, 0x0000000300000077ull);
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
  /* Closed under the root, which never leaves one closed: the root says so. */
  connection->close();
  CHECK(!rig.root().invariants_hold());
  CHECK_STATUS(connection->open(), Status::ok);
  CHECK(rig.root().invariants_hold());
  rig.destroy_instance(instance);
  CHECK(rig.root().invariants_hold()); }
