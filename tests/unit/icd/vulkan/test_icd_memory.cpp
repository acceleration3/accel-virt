#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "guest/kmt/memory_segment_group.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/icd_rig.hpp"
#include "guest/transport/host_allocation.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "icd/vulkan/connection.hpp"
#include "icd/vulkan/device_memory.hpp"
#include "icd/vulkan/icd_root.hpp"
#include "icd/vulkan/loader_handle.hpp"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/submission/blob_import_answer.hpp"
#include "proto/submission/command_header.hpp"

#include <cstdint>
#include <cstring>

using av::CommandHeader;
using av::IcdRig;
using av::MemorySegmentGroup;
using av::MockKmt;
using av::ScriptedHost;
using av::Status;
using av::icd::DeviceMemory;
using Kind = av::icd::LoaderHandle::Kind;

namespace av::icd {

struct ConnectionProbe {
    static GuestConnection &guest(Connection &connection) { return connection.connection_; }
    static KmtTransport    &transport(Connection &connection) { return connection.channel_.transport(); }
};

struct IcdRootProbe {
    static ObjectPool<DeviceMemory> &memories(IcdRoot &root) { return root.memories_; }
};

}

namespace av {

struct ObjectPoolProbe {
    template <typename T>
    static void miscount(ObjectPool<T> &pool, std::int32_t by) { pool.live_ += static_cast<std::uint32_t>(by); }
};

}

using av::ObjectPoolProbe;
using av::icd::IcdRootProbe;

namespace {

using CreateDevice = VkResult (*)(VkPhysicalDevice, const VkDeviceCreateInfo *, const VkAllocationCallbacks *,
                                  VkDevice *);
using DestroyDevice = void (*)(VkDevice, const VkAllocationCallbacks *);
using MemoryProperties = void (*)(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties *);
using MemoryProperties2 = void (*)(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties2 *);
using AllocateMemory = VkResult (*)(VkDevice, const VkMemoryAllocateInfo *, const VkAllocationCallbacks *,
                                    VkDeviceMemory *);
using FreeMemory = void (*)(VkDevice, VkDeviceMemory, const VkAllocationCallbacks *);
using MapMemory = VkResult (*)(VkDevice, VkDeviceMemory, VkDeviceSize, VkDeviceSize, VkMemoryMapFlags, void **);
using UnmapMemory = void (*)(VkDevice, VkDeviceMemory);
using MappedRanges = VkResult (*)(VkDevice, std::uint32_t, const VkMappedMemoryRange *);

constexpr std::uint64_t page = 4096u;
constexpr std::uint64_t mib = 1024u * 1024u;
constexpr std::uint32_t types = 4u;
constexpr std::uint32_t visible = 0x6u;
constexpr std::uint64_t imported = 0x0000700000000040ull;
constexpr std::uint64_t allocated = 0x0000600000000010ull;

VkMemoryAllocateInfo allocation(std::uint64_t bytes, std::uint32_t type)
{ VkMemoryAllocateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  info.allocationSize = bytes;
  info.memoryTypeIndex = type;
  return info; }

VkDeviceMemory as_memory(std::uint64_t host) { return reinterpret_cast<VkDeviceMemory>(host); }

class MemoryRig {
public:
    MemoryRig()
    { instance_ = rig_.create_instance();
      physical_ = rig_.physical_device(instance_);
      VkDeviceCreateInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
      rig_.host().vk_handle(0, ScriptedHost::vk_device);
      CHECK_EQ(entry<CreateDevice>("vkCreateDevice")(physical_, &info, nullptr, &device_), VK_SUCCESS); }
    ~MemoryRig()
    { if (instance_ != VK_NULL_HANDLE) rig_.destroy_instance(instance_);
      CHECK_EQ(rig_.kmt().window_chunks(), 0u);
      CHECK_EQ(rig_.kmt().misuses(), 0u); }

    IcdRig          &rig() { return rig_; }
    MockKmt         &kmt() { return rig_.kmt(); }
    ScriptedHost    &host() { return rig_.host(); }
    VkInstance       instance() const { return instance_; }
    VkPhysicalDevice physical() const { return physical_; }
    VkDevice         device() const { return device_; }
    av::icd::Connection &connection() { return *rig_.root().connection_of(device_, Kind::device); }

    void forget_instance() { instance_ = VK_NULL_HANDLE; }

    template <typename Function>
    Function entry(const char *name) { return rig_.entry<Function>(instance_, name); }

    VkResult allocate(std::uint64_t bytes, std::uint32_t type, VkDeviceMemory &memory)
    { const VkMemoryAllocateInfo info = allocation(bytes, type);
      const VkResult result = entry<AllocateMemory>("vkAllocateMemory")(device_, &info, nullptr, &memory);
      CHECK(rig_.root().invariants_hold());
      return result; }

    VkDeviceMemory host_visible(std::uint64_t bytes, std::uint64_t host, bool first)
    { if (first) rig_.host().vk_memory_properties(types, visible);
      rig_.host().blob_import(0u, host);
      VkDeviceMemory memory = VK_NULL_HANDLE;
      CHECK_EQ(allocate(bytes, 1u, memory), VK_SUCCESS);
      CHECK(memory == as_memory(host));
      return memory; }

    VkResult map(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, void *&data)
    { data = nullptr;
      return entry<MapMemory>("vkMapMemory")(device_, memory, offset, size, 0u, &data); }

    void free(VkDeviceMemory memory)
    { entry<FreeMemory>("vkFreeMemory")(device_, memory, nullptr);
      CHECK(rig_.root().invariants_hold()); }

    std::uint32_t last_command() { return kmt().command_id(kmt().commands() - 1u); }

private:
    IcdRig           rig_;
    VkInstance       instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_ = VK_NULL_HANDLE;
    VkDevice         device_ = VK_NULL_HANDLE;
};

}

TEST(icd_memory, the_physical_device_s_types_sit_on_the_adapter_s_budgets)
{ MemoryRig rig;
  rig.kmt().set_video_memory(MemorySegmentGroup::non_local, 512u * mib, 12u * mib);
  rig.host().vk_memory_properties(types, visible);
  VkPhysicalDeviceMemoryProperties properties = {};
  properties.memoryHeaps[1].size = 99u;
  rig.entry<MemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(rig.physical(), &properties);
  CHECK_EQ(properties.memoryTypeCount, types);
  CHECK_EQ(properties.memoryTypes[1].propertyFlags,
           static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
  CHECK_EQ(properties.memoryTypes[0].propertyFlags,
           static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
  CHECK_EQ(properties.memoryHeapCount, 1u);
  CHECK_EQ(properties.memoryHeaps[0].size, 512u * mib);
  CHECK_EQ(properties.memoryHeaps[1].size, 0u);
  CHECK_EQ(rig.last_command(), static_cast<std::uint32_t>(av::vkproto::kCmd_vkGetPhysicalDeviceMemoryProperties)); }

TEST(icd_memory, the_budget_nodes_in_the_chain_are_the_adapter_s)
{ MemoryRig rig;
  rig.kmt().set_video_memory(MemorySegmentGroup::non_local, 512u * mib, 12u * mib);
  rig.host().vk_memory_properties(types, visible);
  VkPhysicalDeviceMemoryBudgetPropertiesEXT second = {};
  second.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
  VkPhysicalDeviceIDProperties other = {};
  other.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
  other.pNext = &second;
  other.deviceNodeMask = 7u;
  VkPhysicalDeviceMemoryBudgetPropertiesEXT first = {};
  first.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
  first.pNext = &other;
  first.heapBudget[3] = 5u;
  VkPhysicalDeviceMemoryProperties2 properties = {};
  properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
  properties.pNext = &first;
  rig.entry<MemoryProperties2>("vkGetPhysicalDeviceMemoryProperties2")(rig.physical(), &properties);
  CHECK_EQ(properties.memoryProperties.memoryTypeCount, types);
  CHECK_EQ(properties.memoryProperties.memoryHeaps[0].size, 512u * mib);
  const VkPhysicalDeviceMemoryBudgetPropertiesEXT *const budgets[] = {&first, &second};
  for (const VkPhysicalDeviceMemoryBudgetPropertiesEXT *budget : budgets) {
      CHECK_EQ(budget->heapBudget[0], 512u * mib);
      CHECK_EQ(budget->heapUsage[0], 12u * mib);
      CHECK_EQ(budget->heapBudget[3], 0u);
  }
  CHECK(first.pNext == &other);
  CHECK(other.pNext == &second);
  CHECK_EQ(other.deviceNodeMask, 7u);
  CHECK_EQ(other.sType, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES);
  const char *const budget_extension = "VK_EXT_memory_budget";
  VkDeviceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  info.enabledExtensionCount = 1u;
  info.ppEnabledExtensionNames = &budget_extension;
  VkDevice enabled = VK_NULL_HANDLE;
  rig.host().vk_handle(0, ScriptedHost::vk_device + 1u);
  CHECK_EQ(rig.entry<CreateDevice>("vkCreateDevice")(rig.physical(), &info, nullptr, &enabled), VK_SUCCESS);
  CHECK(enabled != VK_NULL_HANDLE); }

TEST(icd_memory, a_cyclic_chain_is_walked_a_bounded_distance)
{ MemoryRig rig;
  rig.host().vk_memory_properties(types, visible);
  VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {};
  budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
  budget.pNext = &budget;
  VkPhysicalDeviceMemoryProperties2 properties = {};
  properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
  properties.pNext = &budget;
  rig.entry<MemoryProperties2>("vkGetPhysicalDeviceMemoryProperties2")(rig.physical(), &properties);
  CHECK_EQ(budget.heapBudget[0], MockKmt::default_non_local_budget);
  CHECK(budget.pNext == &budget); }

TEST(icd_memory, types_that_cannot_be_taken_answer_nothing)
{ MemoryRig rig;
  rig.host().vk_memory_properties(VK_MAX_MEMORY_TYPES + 1u, visible);
  VkPhysicalDeviceMemoryProperties properties = {};
  properties.memoryTypeCount = 3u;
  rig.entry<MemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(rig.physical(), &properties);
  CHECK_EQ(properties.memoryTypeCount, 0u);
  CHECK_EQ(properties.memoryHeapCount, 0u);
  CHECK_EQ(rig.rig().log().holding("memory types were not taken"), 1u);

  rig.host().vk_memory_properties(types, visible);
  rig.kmt().fail_at(MockKmt::Call::query_video_memory, 1u, MockKmt::invalid_parameter);
  VkPhysicalDeviceMemoryProperties2 properties2 = {};
  properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
  properties2.memoryProperties.memoryTypeCount = 3u;
  rig.entry<MemoryProperties2>("vkGetPhysicalDeviceMemoryProperties2")(rig.physical(), &properties2);
  CHECK_EQ(properties2.memoryProperties.memoryTypeCount, 0u);
  CHECK_EQ(rig.rig().log().holding("memory types were not taken"), 2u); }

TEST(icd_memory, a_physical_device_it_did_not_make_is_not_asked_about)
{ MemoryRig rig;
  const std::uint32_t commands = rig.kmt().commands();
  int stack = 0;
  const auto foreign = reinterpret_cast<VkPhysicalDevice>(&stack);
  VkPhysicalDeviceMemoryProperties properties = {};
  properties.memoryTypeCount = 3u;
  rig.rig().root().physical_device_memory_properties(foreign, &properties);
  CHECK_EQ(properties.memoryTypeCount, 3u);
  VkPhysicalDeviceMemoryProperties2 properties2 = {};
  properties2.memoryProperties.memoryTypeCount = 3u;
  rig.rig().root().physical_device_memory_properties2(foreign, &properties2);
  CHECK_EQ(properties2.memoryProperties.memoryTypeCount, 3u);
  CHECK_EQ(rig.kmt().commands(), commands); }

TEST(icd_memory, a_host_visible_allocation_maps_its_own_chunk_with_no_round_trip)
{ MemoryRig rig;
  const VkDeviceMemory first = rig.host_visible(page, imported, true);
  const VkDeviceMemory second = rig.host_visible(2u * page + 1u, imported + 1u, false);
  CHECK_EQ(rig.kmt().window_chunks(), 2u);
  CHECK_EQ(rig.last_command(), CommandHeader::import_blob_id);
  const av::HostMemoryRequest asked(av::HostMemoryRequest::Kind::vulkan, 1u);
  CHECK_EQ(rig.kmt().window_request(MockKmt::first_window_resource).encoded(), asked.encoded());
  CHECK_EQ(rig.kmt().window_request(MockKmt::first_window_resource + 1u).encoded(), asked.encoded());
  const std::uint8_t *const first_chunk = rig.kmt().window_chunk_memory(MockKmt::first_window_resource);
  const std::uint8_t *const second_chunk = rig.kmt().window_chunk_memory(MockKmt::first_window_resource + 1u);
  CHECK(first_chunk != nullptr && second_chunk == first_chunk + page);

  const std::uint32_t commands = rig.kmt().commands();
  void *data = nullptr;
  CHECK_EQ(rig.map(second, 5u, 16u, data), VK_SUCCESS);
  CHECK(data == second_chunk + 5u);
  std::memcpy(data, "guest bytes", 12u);
  CHECK(std::memcmp(second_chunk + 5u, "guest bytes", 12u) == 0);
  CHECK_EQ(rig.map(first, 0u, VK_WHOLE_SIZE, data), VK_SUCCESS);
  CHECK(data == first_chunk);
  rig.entry<UnmapMemory>("vkUnmapMemory")(rig.device(), first);
  VkMappedMemoryRange range = {};
  range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  range.memory = first;
  range.size = VK_WHOLE_SIZE;
  CHECK_EQ(rig.entry<MappedRanges>("vkFlushMappedMemoryRanges")(rig.device(), 1u, &range), VK_SUCCESS);
  CHECK_EQ(rig.entry<MappedRanges>("vkInvalidateMappedMemoryRanges")(rig.device(), 1u, &range), VK_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), commands);

  rig.host().nop();
  rig.free(first);
  CHECK_EQ(rig.last_command(), CommandHeader::release_blob_id);
  CHECK_EQ(rig.kmt().window_chunks(), 1u);
  CHECK_EQ(rig.map(first, 0u, 1u, data), VK_ERROR_MEMORY_MAP_FAILED);
  CHECK_EQ(rig.map(second, 0u, VK_WHOLE_SIZE, data), VK_SUCCESS);
  rig.host().nop();
  rig.free(second);
  CHECK_EQ(rig.kmt().window_chunks(), 0u); }

TEST(icd_memory, a_map_outside_the_allocation_fails)
{ MemoryRig rig;
  const VkDeviceMemory memory = rig.host_visible(page + 1u, imported, true);
  struct Case {
      VkDeviceSize     offset;
      VkDeviceSize     size;
      VkMemoryMapFlags flags;
      bool             maps;
  };
  const Case cases[] = {
      {0u, 2u * page, 0u, true},
      {2u * page - 1u, 1u, 0u, true},
      {2u * page - 1u, VK_WHOLE_SIZE, 0u, true},
      {0u, 2u * page + 1u, 0u, false},
      {2u * page - 1u, 2u, 0u, false},
      {2u * page, VK_WHOLE_SIZE, 0u, false},
      {~VkDeviceSize{0}, 1u, 0u, false},
      {1u, ~VkDeviceSize{0} - 1u, 0u, false},
      {0u, 0u, 0u, false},
      {0u, 1u, 1u, false},
  };
  for (const Case &test : cases) {
      void *data = reinterpret_cast<void *>(1);
      const VkResult result =
          rig.entry<MapMemory>("vkMapMemory")(rig.device(), memory, test.offset, test.size, test.flags, &data);
      CHECK_EQ(result, test.maps ? VK_SUCCESS : VK_ERROR_MEMORY_MAP_FAILED);
      if (test.maps) CHECK(data != nullptr);
  }
  rig.host().nop();
  rig.free(memory); }

TEST(icd_memory, map_memory2_maps_what_vk_map_memory_does_and_refuses_placement)
{ MemoryRig rig;
  const VkDeviceMemory memory = rig.host_visible(2u * page, 0x30000u, true);
  using MapMemory2 = VkResult (*)(VkDevice, const VkMemoryMapInfoKHR *, void **);
  using UnmapMemory2 = VkResult (*)(VkDevice, const VkMemoryUnmapInfoKHR *);
  const MapMemory2   map2 = rig.entry<MapMemory2>("vkMapMemory2KHR");
  const UnmapMemory2 unmap2 = rig.entry<UnmapMemory2>("vkUnmapMemory2KHR");
  CHECK(map2 != nullptr);
  CHECK(unmap2 != nullptr);

  VkMemoryMapInfoKHR info = {};
  info.sType = VK_STRUCTURE_TYPE_MEMORY_MAP_INFO;
  info.memory = memory;
  info.offset = 0u;
  info.size = VK_WHOLE_SIZE;
  void *through_two = nullptr;
  CHECK_EQ(map2(rig.device(), &info, &through_two), VK_SUCCESS);
  CHECK(through_two != nullptr);
  void *through_one = nullptr;
  CHECK_EQ(rig.map(memory, 0u, VK_WHOLE_SIZE, through_one), VK_SUCCESS);
  CHECK(through_one == through_two);
  info.offset = page;
  info.size = page;
  void *further = nullptr;
  CHECK_EQ(map2(rig.device(), &info, &further), VK_SUCCESS);
  CHECK_EQ(static_cast<const unsigned char *>(further) - static_cast<const unsigned char *>(through_two), page);

  info.offset = 0u;
  info.size = 2u * page + 1u;
  CHECK_EQ(map2(rig.device(), &info, &further), VK_ERROR_MEMORY_MAP_FAILED);
  info.size = VK_WHOLE_SIZE;
  info.flags = VK_MEMORY_MAP_PLACED_BIT_EXT;
  CHECK_EQ(map2(rig.device(), &info, &further), VK_ERROR_MEMORY_MAP_FAILED);
  info.flags = 0u;

  VkMemoryMapPlacedInfoEXT placed = {};
  placed.sType = VK_STRUCTURE_TYPE_MEMORY_MAP_PLACED_INFO_EXT;
  placed.pPlacedAddress = reinterpret_cast<void *>(0x40000000u);
  info.pNext = &placed;
  CHECK_EQ(map2(rig.device(), &info, &further), VK_ERROR_MEMORY_MAP_FAILED);
  CHECK_EQ(rig.rig().log().holding("cannot place"), 1u);
  info.pNext = nullptr;

  CHECK_EQ(map2(rig.device(), nullptr, &further), VK_ERROR_MEMORY_MAP_FAILED);
  CHECK_EQ(map2(rig.device(), &info, nullptr), VK_ERROR_MEMORY_MAP_FAILED);

  VkMemoryUnmapInfoKHR unmapping = {};
  unmapping.sType = VK_STRUCTURE_TYPE_MEMORY_UNMAP_INFO;
  unmapping.memory = memory;
  CHECK_EQ(unmap2(rig.device(), &unmapping), VK_SUCCESS);
  unmapping.flags = VK_MEMORY_UNMAP_RESERVE_BIT_EXT;
  CHECK_EQ(unmap2(rig.device(), &unmapping), VK_ERROR_MEMORY_MAP_FAILED);
  CHECK_EQ(unmap2(rig.device(), nullptr), VK_ERROR_MEMORY_MAP_FAILED);
  void *again = nullptr;
  CHECK_EQ(rig.map(memory, 0u, VK_WHOLE_SIZE, again), VK_SUCCESS);
  CHECK(again == through_two);
  rig.host().nop();
  rig.free(memory); }

TEST(icd_memory, a_device_local_allocation_is_the_host_s_and_cannot_be_mapped)
{ MemoryRig rig;
  rig.host().vk_memory_properties(types, visible);
  rig.host().vk_handle(0, allocated);
  VkDeviceMemory memory = VK_NULL_HANDLE;
  CHECK_EQ(rig.allocate(page, 0u, memory), VK_SUCCESS);
  CHECK(memory == as_memory(allocated));
  CHECK_EQ(rig.last_command(), static_cast<std::uint32_t>(av::vkproto::kCmd_vkAllocateMemory));
  CHECK_EQ(rig.kmt().window_chunks(), 0u);
  void *data = nullptr;
  CHECK_EQ(rig.map(memory, 0u, 1u, data), VK_ERROR_MEMORY_MAP_FAILED);
  CHECK(data == nullptr);
  rig.free(memory);
  CHECK_EQ(rig.last_command(), static_cast<std::uint32_t>(av::vkproto::kCmd_vkFreeMemory));
  rig.host().vk_handle(VK_ERROR_OUT_OF_DEVICE_MEMORY, 0u);
  CHECK_EQ(rig.allocate(page, types, memory), VK_ERROR_OUT_OF_DEVICE_MEMORY);
  const std::uint32_t commands = rig.kmt().commands();
  rig.free(VK_NULL_HANDLE);
  CHECK_EQ(rig.kmt().commands(), commands); }

TEST(icd_memory, the_types_are_asked_for_once)
{ MemoryRig rig;
  const VkDeviceMemory memory = rig.host_visible(page, imported, true);
  rig.host().vk_handle(0, allocated);
  VkDeviceMemory local = VK_NULL_HANDLE;
  CHECK_EQ(rig.allocate(page, 3u, local), VK_SUCCESS);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  rig.free(local);
  rig.host().nop();
  rig.free(memory); }

TEST(icd_memory, a_host_visible_allocation_refused_anywhere_is_out_of_device_memory)
{ MemoryRig rig;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  rig.host().cut_next(0u);
  rig.host().vk_memory_properties(types, visible);
  CHECK_EQ(rig.allocate(page, 1u, memory), VK_ERROR_OUT_OF_DEVICE_MEMORY);
  rig.host().vk_memory_properties(types, visible);
  CHECK_EQ(rig.rig().log().holding("memory types cannot be learnt"), 1u);
  rig.kmt().fail_at(MockKmt::Call::escape, 1u, MockKmt::invalid_parameter);
  CHECK_EQ(rig.allocate(page, 1u, memory), VK_ERROR_OUT_OF_DEVICE_MEMORY);
  CHECK_EQ(rig.rig().log().holding("no host-visible memory"), 1u);
  rig.host().blob_import(av::BlobImportAnswer::executor_refused, 0u);
  CHECK_EQ(rig.allocate(page, 2u, memory), VK_ERROR_OUT_OF_DEVICE_MEMORY);
  CHECK_EQ(rig.rig().log().holding("no host-visible memory"), 2u);
  rig.host().blob_import(av::BlobImportAnswer::no_room, 0u);
  CHECK_EQ(rig.allocate(page, 2u, memory), VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK_EQ(rig.rig().log().holding("no host-visible memory"), 3u);
  const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
  CHECK_EQ(rig.allocate(0u, 1u, memory), VK_ERROR_OUT_OF_DEVICE_MEMORY);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes);
  CHECK_EQ(rig.kmt().window_chunks(), 0u);
  CHECK_EQ(rig.kmt().queued_answers(), 0u); }

TEST(icd_memory, a_refused_release_is_logged_and_the_allocation_gone)
{ MemoryRig rig;
  const VkDeviceMemory memory = rig.host_visible(page, imported, true);
  rig.host().blob_import(0u, 1u);
  rig.free(memory);
  CHECK_EQ(rig.rig().log().holding("the host refused to release the memory"), 1u);
  CHECK_EQ(rig.kmt().window_chunks(), 0u); }

TEST(icd_memory, destroying_the_device_gives_its_memory_back_first)
{ MemoryRig rig;
  static_cast<void>(rig.host_visible(page, imported, true));
  static_cast<void>(rig.host_visible(page, imported + 1u, false));
  const std::uint32_t commands = rig.kmt().commands();
  rig.host().nop();
  rig.host().nop();
  rig.entry<DestroyDevice>("vkDestroyDevice")(rig.device(), nullptr);
  CHECK(rig.rig().root().invariants_hold());
  CHECK_EQ(rig.kmt().commands(), commands + 3u);
  CHECK_EQ(rig.kmt().command_id(commands), CommandHeader::release_blob_id);
  CHECK_EQ(rig.kmt().command_id(commands + 1u), CommandHeader::release_blob_id);
  CHECK_EQ(rig.kmt().command_id(commands + 2u), static_cast<std::uint32_t>(av::vkproto::kCmd_vkDestroyDevice));
  CHECK_EQ(rig.kmt().window_chunks(), 0u);
  const char *const trace = rig.rig().trace().text();
  if (std::strstr(trace, "MSsmHhHhS") == nullptr)
      av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "trace %s", trace); }

TEST(icd_memory, a_device_whose_memory_is_not_released_whole_still_goes)
{ MemoryRig rig;
  static_cast<void>(rig.host_visible(page, imported, true));
  rig.host().blob_import(0u, 1u);
  rig.entry<DestroyDevice>("vkDestroyDevice")(rig.device(), nullptr);
  CHECK_EQ(rig.rig().log().holding("not released whole"), 1u);
  CHECK_EQ(rig.kmt().window_chunks(), 0u);
  CHECK(rig.rig().root().invariants_hold()); }

TEST(icd_memory, destroying_the_instance_gives_every_device_s_memory_back_first)
{ MemoryRig rig;
  static_cast<void>(rig.host_visible(page, imported, true));
  VkDeviceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  VkDevice idle = VK_NULL_HANDLE;
  rig.host().vk_handle(0, ScriptedHost::vk_device + 1u);
  CHECK_EQ(rig.entry<CreateDevice>("vkCreateDevice")(rig.physical(), &info, nullptr, &idle), VK_SUCCESS);
  const std::uint32_t commands = rig.kmt().commands();
  rig.host().nop();
  rig.rig().destroy_instance(rig.instance());
  rig.forget_instance();
  CHECK(rig.rig().root().invariants_hold());
  CHECK_EQ(rig.kmt().command_id(commands), CommandHeader::release_blob_id);
  CHECK_EQ(rig.kmt().command_id(commands + 1u), static_cast<std::uint32_t>(av::vkproto::kCmd_vkDestroyInstance));
  CHECK_EQ(rig.kmt().window_chunks(), 0u);
  CHECK(rig.rig().root().connection_of(rig.device(), Kind::device) == nullptr); }

TEST(icd_memory, an_instance_or_device_it_did_not_make_is_nothing_to_destroy)
{ MemoryRig rig;
  const std::uint32_t commands = rig.kmt().commands();
  int stack = 0;
  rig.rig().root().destroy_instance(reinterpret_cast<VkInstance>(&stack), nullptr);
  rig.rig().root().destroy_device(reinterpret_cast<VkDevice>(&stack), nullptr);
  CHECK_EQ(rig.kmt().commands(), commands); }

TEST(icd_memory, memory_of_a_device_it_did_not_make_is_refused)
{ MemoryRig rig;
  const std::uint32_t commands = rig.kmt().commands();
  int stack = 0;
  const auto foreign = reinterpret_cast<VkDevice>(&stack);
  av::icd::IcdRoot &root = rig.rig().root();
  const VkMemoryAllocateInfo info = allocation(page, 1u);
  VkDeviceMemory memory = VK_NULL_HANDLE;
  CHECK_EQ(root.allocate_memory(foreign, &info, nullptr, &memory), VK_ERROR_DEVICE_LOST);
  root.free_memory(foreign, as_memory(imported), nullptr);
  void *data = nullptr;
  CHECK_EQ(root.map_memory(foreign, as_memory(imported), 0u, 1u, 0u, &data), VK_ERROR_MEMORY_MAP_FAILED);
  const auto hooked = static_cast<VkDevice>(root.wrap(Kind::device, ScriptedHost::vk_device + 7u, &rig.connection()));
  CHECK(hooked != VK_NULL_HANDLE);
  CHECK_EQ(root.allocate_memory(hooked, &info, nullptr, &memory), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.kmt().commands(), commands);
  CHECK_EQ(rig.rig().log().holding("did not make through a physical device"), 4u);
  root.destroy_wrapper(hooked, Kind::device); }

TEST(icd_memory, a_device_that_never_allocated_maps_nothing)
{ MemoryRig rig;
  void *data = nullptr;
  CHECK_EQ(rig.map(as_memory(imported), 0u, 1u, data), VK_ERROR_MEMORY_MAP_FAILED); }

TEST(icd_memory, no_memory_for_a_device_s_allocations_is_out_of_host_memory)
{ MemoryRig rig;
  rig.rig().platform().fail_allocation_at(1u);
  VkDeviceMemory memory = VK_NULL_HANDLE;
  CHECK_EQ(rig.allocate(page, 1u, memory), VK_ERROR_OUT_OF_HOST_MEMORY);
  CHECK_EQ(rig.rig().log().holding("no memory to keep"), 1u);
  const std::uint32_t commands = rig.kmt().commands();
  rig.rig().platform().fail_allocation_at(1u);
  rig.free(as_memory(allocated));
  CHECK_EQ(rig.kmt().commands(), commands);
  CHECK_EQ(rig.rig().log().holding("no memory to keep"), 2u); }

TEST(device_memory, holds_nothing_and_releases_nothing_until_it_allocates)
{ MemoryRig    rig;
  DeviceMemory memory(rig.kmt(), rig.connection(), rig.rig().root().wire_handles(), rig.rig().lines());
  CHECK(memory.invariants_hold());
  CHECK_STATUS(memory.release_all(), Status::ok);
  void *data = nullptr;
  CHECK_EQ(memory.map(as_memory(imported), 0u, 1u, 0u, data), VK_ERROR_MEMORY_MAP_FAILED); }

TEST(device_memory, releases_every_host_allocation_at_once)
{ MemoryRig    rig;
  DeviceMemory memory(rig.kmt(), rig.connection(), rig.rig().root().wire_handles(), rig.rig().lines());
  rig.host().vk_memory_properties(types, visible);
  rig.host().blob_import(0u, imported);
  rig.host().blob_import(0u, imported + 1u);
  const VkMemoryAllocateInfo info = allocation(page, 2u);
  VkDeviceMemory first = VK_NULL_HANDLE;
  VkDeviceMemory second = VK_NULL_HANDLE;
  CHECK_EQ(memory.allocate(rig.physical(), rig.device(), info, nullptr, first), VK_SUCCESS);
  CHECK_EQ(memory.allocate(rig.physical(), rig.device(), info, nullptr, second), VK_SUCCESS);
  CHECK(memory.invariants_hold());
  rig.host().nop();
  rig.host().nop();
  CHECK_STATUS(memory.release_all(), Status::ok);
  CHECK(memory.invariants_hold());
  CHECK_EQ(rig.kmt().window_chunks(), 0u);
  void *data = nullptr;
  CHECK_EQ(memory.map(first, 0u, 1u, 0u, data), VK_ERROR_MEMORY_MAP_FAILED); }

TEST(device_memory, release_all_answers_the_first_refusal_and_lets_everything_go)
{ MemoryRig    rig;
  DeviceMemory memory(rig.kmt(), rig.connection(), rig.rig().root().wire_handles(), rig.rig().lines());
  rig.host().vk_memory_properties(types, visible);
  rig.host().blob_import(0u, imported);
  rig.host().blob_import(0u, imported + 1u);
  const VkMemoryAllocateInfo info = allocation(page, 1u);
  VkDeviceMemory first = VK_NULL_HANDLE;
  VkDeviceMemory second = VK_NULL_HANDLE;
  CHECK_EQ(memory.allocate(rig.physical(), rig.device(), info, nullptr, first), VK_SUCCESS);
  CHECK_EQ(memory.allocate(rig.physical(), rig.device(), info, nullptr, second), VK_SUCCESS);
  rig.host().blob_import(0u, 1u);
  rig.host().nop();
  CHECK_STATUS(memory.release_all(), Status::malformed);
  CHECK(memory.invariants_hold());
  CHECK_EQ(rig.kmt().window_chunks(), 0u);
  CHECK_STATUS(memory.release_all(), Status::ok); }

TEST(icd_memory, root_invariants_refuse_memory_no_device_owns_or_that_is_broken)
{ MemoryRig rig;
  av::icd::IcdRoot &root = rig.rig().root();
  const VkDeviceMemory memory = rig.host_visible(page, imported, true);
  CHECK(root.invariants_hold());
  av::ObjectPool<DeviceMemory> &memories = IcdRootProbe::memories(root);
  DeviceMemory *stray = nullptr;
  CHECK_STATUS(memories.create(stray, rig.kmt(), rig.connection(), rig.rig().root().wire_handles(), rig.rig().lines()),
               Status::ok);
  CHECK(!root.invariants_hold());
  CHECK_STATUS(memories.destroy(stray), Status::ok);
  CHECK(root.invariants_hold());
  ObjectPoolProbe::miscount(memories, 1);
  CHECK(!root.invariants_hold());
  ObjectPoolProbe::miscount(memories, -1);
  CHECK(root.invariants_hold());
  rig.host().nop();
  rig.free(memory); }
