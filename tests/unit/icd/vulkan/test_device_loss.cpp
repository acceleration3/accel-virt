#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/icd_rig.hpp"
#include "icd/vulkan/connection.hpp"
#include "icd/vulkan/icd_root.hpp"
#include "icd/vulkan/loader_handle.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

using av::IcdRig;
using av::MockKmt;
using av::ScriptedHost;
using av::WireWriter;
using Kind = av::icd::LoaderHandle::Kind;

namespace {

using CreateDevice = VkResult (*)(VkPhysicalDevice, const VkDeviceCreateInfo *, const VkAllocationCallbacks *,
                                  VkDevice *);
using DestroyDevice = void (*)(VkDevice, const VkAllocationCallbacks *);
using EnumerateAdapter = VkResult (*)(VkInstance, std::uint64_t, std::uint32_t *, VkPhysicalDevice *);
using EnumerateDeviceExtensions = VkResult (*)(VkPhysicalDevice, const char *, std::uint32_t *,
                                               VkExtensionProperties *);
using MemoryProperties = void (*)(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties *);
using MemoryProperties2 = void (*)(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties2 *);
using AllocateMemory = VkResult (*)(VkDevice, const VkMemoryAllocateInfo *, const VkAllocationCallbacks *,
                                    VkDeviceMemory *);
using FreeMemory = void (*)(VkDevice, VkDeviceMemory, const VkAllocationCallbacks *);
using MapMemory = VkResult (*)(VkDevice, VkDeviceMemory, VkDeviceSize, VkDeviceSize, VkMemoryMapFlags, void **);
using UnmapMemory = void (*)(VkDevice, VkDeviceMemory);
using MappedRanges = VkResult (*)(VkDevice, std::uint32_t, const VkMappedMemoryRange *);
using GetDeviceQueue = void (*)(VkDevice, std::uint32_t, std::uint32_t, VkQueue *);
using AllocateCommandBuffers = VkResult (*)(VkDevice, const VkCommandBufferAllocateInfo *, VkCommandBuffer *);
using FreeCommandBuffers = void (*)(VkDevice, VkCommandPool, std::uint32_t, const VkCommandBuffer *);
using BeginCommandBuffer = VkResult (*)(VkCommandBuffer, const VkCommandBufferBeginInfo *);
using EndCommandBuffer = VkResult (*)(VkCommandBuffer);
using ResetCommandBuffer = VkResult (*)(VkCommandBuffer, VkCommandBufferResetFlags);
using CmdDraw = void (*)(VkCommandBuffer, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
using WaitForFences = VkResult (*)(VkDevice, std::uint32_t, const VkFence *, VkBool32, std::uint64_t);
using WaitSemaphores = VkResult (*)(VkDevice, const VkSemaphoreWaitInfo *, std::uint64_t);
using QueueWaitIdle = VkResult (*)(VkQueue);
using DeviceWaitIdle = VkResult (*)(VkDevice);
using GetFenceStatus = VkResult (*)(VkDevice, VkFence);
using ResetFences = VkResult (*)(VkDevice, std::uint32_t, const VkFence *);
using CreateBuffer = VkResult (*)(VkDevice, const VkBufferCreateInfo *, const VkAllocationCallbacks *, VkBuffer *);
using QueueSubmit = VkResult (*)(VkQueue, std::uint32_t, const VkSubmitInfo *, VkFence);

constexpr std::uint64_t page = 4096u;
constexpr std::uint32_t types = 4u;
constexpr std::uint32_t visible = 0x6u;
constexpr std::uint64_t imported = 0x0000700000000040ull;
constexpr std::uint64_t allocated = 0x0000600000000010ull;
constexpr std::uint64_t queue_host = 0x6600u;
constexpr std::uint64_t command_buffer_host = 0x7700u;
constexpr std::uint64_t pool_host = 0x7F00u;
constexpr std::uint64_t fence_host = 0x8800u;
constexpr std::uint8_t  pattern = 0x5Au;

std::uint32_t kmt_calls(const MockKmt &kmt)
{
    std::uint32_t total = 0u;
    for (unsigned call = 0u; call < static_cast<unsigned>(MockKmt::Call::count); call++)
        total += kmt.calls(static_cast<MockKmt::Call>(call));
    return total;
}

VkMemoryAllocateInfo allocation(std::uint64_t bytes, std::uint32_t type)
{
    VkMemoryAllocateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.allocationSize = bytes;
    info.memoryTypeIndex = type;
    return info;
}

VkDeviceMemory as_memory(std::uint64_t host) { return reinterpret_cast<VkDeviceMemory>(host); }

class LossRig {
public:
    LossRig()
    {
        instance_ = rig_.create_instance();
        physical_ = rig_.physical_device(instance_);
        device_ = make_device(ScriptedHost::vk_device);
    }
    ~LossRig()
    {
        if (instance_ != VK_NULL_HANDLE) rig_.destroy_instance(instance_);
        CHECK(rig_.root().invariants_hold());
        CHECK_EQ(rig_.kmt().window_chunks(), 0u);
        CHECK_EQ(rig_.kmt().open_objects(), 0u);
        CHECK_EQ(rig_.kmt().misuses(), 0u);
    }

    IcdRig          &rig() { return rig_; }
    MockKmt         &kmt() { return rig_.kmt(); }
    ScriptedHost    &host() { return rig_.host(); }
    VkInstance       instance() const { return instance_; }
    VkPhysicalDevice physical() const { return physical_; }
    VkDevice         device() const { return device_; }
    av::icd::Connection *connection() { return rig_.root().connection_of(instance_, Kind::instance); }

    void forget_instance() { instance_ = VK_NULL_HANDLE; }

    template <typename Function>
    Function entry(const char *name) { return rig_.entry<Function>(instance_, name); }

    VkDevice make_device(std::uint64_t host)
    {
        VkDeviceCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        rig_.host().vk_handle(0, host);
        VkDevice made = VK_NULL_HANDLE;
        CHECK_EQ(entry<CreateDevice>("vkCreateDevice")(physical_, &info, nullptr, &made), VK_SUCCESS);
        return made;
    }

    VkQueue queue()
    {
        VkQueue made = VK_NULL_HANDLE;
        rig_.host().vk_handle(0, queue_host);
        entry<GetDeviceQueue>("vkGetDeviceQueue")(device_, 0u, 0u, &made);
        CHECK(made != VK_NULL_HANDLE);
        return made;
    }

    VkCommandBuffer command_buffer(std::uint64_t host)
    {
        rig_.host().vk_handle(0, host);
        VkCommandBufferAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.commandPool = reinterpret_cast<VkCommandPool>(pool_host);
        info.commandBufferCount = 1u;
        VkCommandBuffer made = VK_NULL_HANDLE;
        CHECK_EQ(entry<AllocateCommandBuffers>("vkAllocateCommandBuffers")(device_, &info, &made), VK_SUCCESS);
        return made;
    }

    VkResult allocate(std::uint64_t bytes, std::uint32_t type, VkDeviceMemory &memory)
    {
        const VkMemoryAllocateInfo info = allocation(bytes, type);
        const VkResult result = entry<AllocateMemory>("vkAllocateMemory")(device_, &info, nullptr, &memory);
        CHECK(rig_.root().invariants_hold());
        return result;
    }

    VkDeviceMemory host_visible()
    {
        rig_.host().vk_memory_properties(types, visible);
        rig_.host().blob_import(0u, imported);
        VkDeviceMemory memory = VK_NULL_HANDLE;
        CHECK_EQ(allocate(page, 1u, memory), VK_SUCCESS);
        CHECK(memory == as_memory(imported));
        return memory;
    }

    VkResult map(VkDeviceMemory memory, void *&data)
    {
        data = nullptr;
        return entry<MapMemory>("vkMapMemory")(device_, memory, 0u, page, 0u, &data);
    }

    void lose()
    {
        kmt().withhold_answer_at(1u);
        CHECK_EQ(entry<GetFenceStatus>("vkGetFenceStatus")(device_, reinterpret_cast<VkFence>(fence_host)),
                 VK_ERROR_DEVICE_LOST);
        CHECK(connection()->faulted());
        CHECK(rig_.root().invariants_hold());
    }

private:
    IcdRig           rig_;
    VkInstance       instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_ = VK_NULL_HANDLE;
    VkDevice         device_ = VK_NULL_HANDLE;
};

}

TEST(device_loss, generated_entry_points_answer_lost_with_no_kmt_call)
{ LossRig rig;
  const VkQueue queue = rig.queue();
  const VkCommandBuffer command_buffer = rig.command_buffer(command_buffer_host);
  rig.lose();
  const std::uint32_t before = kmt_calls(rig.kmt());
  const VkFence fence = reinterpret_cast<VkFence>(fence_host);
  CHECK_EQ(rig.entry<GetFenceStatus>("vkGetFenceStatus")(rig.device(), fence), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.entry<ResetFences>("vkResetFences")(rig.device(), 1u, &fence), VK_ERROR_DEVICE_LOST);
  VkBufferCreateInfo buffer_info = {};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = page;
  VkBuffer buffer = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<CreateBuffer>("vkCreateBuffer")(rig.device(), &buffer_info, nullptr, &buffer),
           VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.entry<QueueSubmit>("vkQueueSubmit")(queue, 0u, nullptr, fence), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.entry<ResetCommandBuffer>("vkResetCommandBuffer")(command_buffer, 0u), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(kmt_calls(rig.kmt()), before);
  CHECK(rig.rig().root().invariants_hold()); }

TEST(device_loss, local_entry_points_answer_lost_with_no_kmt_call)
{ LossRig rig;
  const VkDevice idle = rig.make_device(ScriptedHost::vk_device + 1u);
  const VkQueue queue = rig.queue();
  const VkCommandBuffer recording = rig.command_buffer(command_buffer_host);
  const VkCommandBuffer ended = rig.command_buffer(command_buffer_host + 1u);
  const VkDeviceMemory memory = rig.host_visible();
  VkCommandBufferBeginInfo begin = {};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  CHECK_EQ(rig.entry<BeginCommandBuffer>("vkBeginCommandBuffer")(ended, &begin), VK_SUCCESS);
  rig.lose();
  const std::uint32_t before = kmt_calls(rig.kmt());
  const std::uint32_t logged = rig.rig().log().holding("on a lost device");
  std::uint32_t count = 1u;
  VkPhysicalDevice physical = VK_NULL_HANDLE;
  const auto enumerate = rig.entry<EnumerateAdapter>("vk_icdEnumerateAdapterPhysicalDevices");
  CHECK_EQ(enumerate(rig.instance(), IcdRig::adapter_luid, &count, &physical), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(enumerate(rig.instance(), IcdRig::adapter_luid + 1u, &count, &physical), VK_ERROR_DEVICE_LOST);
  CHECK(physical == VK_NULL_HANDLE);
  CHECK_EQ(rig.entry<EnumerateDeviceExtensions>("vkEnumerateDeviceExtensionProperties")(rig.physical(), nullptr,
                                                                                       &count, nullptr),
           VK_ERROR_DEVICE_LOST);
  VkDeviceCreateInfo device_info = {};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  VkDevice device = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<CreateDevice>("vkCreateDevice")(rig.physical(), &device_info, nullptr, &device),
           VK_ERROR_DEVICE_LOST);
  CHECK(device == VK_NULL_HANDLE);
  VkDeviceMemory more = VK_NULL_HANDLE;
  CHECK_EQ(rig.allocate(page, 0u, more), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.allocate(page, 1u, more), VK_ERROR_DEVICE_LOST);
  CHECK(more == VK_NULL_HANDLE);
  void *data = nullptr;
  CHECK_EQ(rig.map(memory, data), VK_ERROR_DEVICE_LOST);
  CHECK(data == nullptr);
  VkMappedMemoryRange range = {};
  range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  range.memory = memory;
  range.size = VK_WHOLE_SIZE;
  CHECK_EQ(rig.entry<MappedRanges>("vkFlushMappedMemoryRanges")(rig.device(), 1u, &range), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.entry<MappedRanges>("vkInvalidateMappedMemoryRanges")(rig.device(), 1u, &range),
           VK_ERROR_DEVICE_LOST);
  VkCommandBufferAllocateInfo command_info = {};
  command_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  command_info.commandPool = reinterpret_cast<VkCommandPool>(pool_host);
  command_info.commandBufferCount = 1u;
  VkCommandBuffer allocated_buffer = VK_NULL_HANDLE;
  CHECK_EQ(rig.entry<AllocateCommandBuffers>("vkAllocateCommandBuffers")(rig.device(), &command_info,
                                                                          &allocated_buffer),
           VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.entry<BeginCommandBuffer>("vkBeginCommandBuffer")(recording, &begin), VK_ERROR_DEVICE_LOST);
  rig.entry<CmdDraw>("vkCmdDraw")(ended, 3u, 1u, 0u, 0u);
  CHECK_EQ(rig.entry<EndCommandBuffer>("vkEndCommandBuffer")(ended), VK_ERROR_DEVICE_LOST);
  const VkFence fence = reinterpret_cast<VkFence>(fence_host);
  CHECK_EQ(rig.entry<WaitForFences>("vkWaitForFences")(rig.device(), 1u, &fence, VK_TRUE, UINT64_MAX),
           VK_ERROR_DEVICE_LOST);
  VkSemaphoreWaitInfo wait_info = {};
  wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
  CHECK_EQ(rig.entry<WaitSemaphores>("vkWaitSemaphores")(rig.device(), &wait_info, 0u), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.entry<WaitSemaphores>("vkWaitSemaphoresKHR")(rig.device(), &wait_info, 0u), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.entry<QueueWaitIdle>("vkQueueWaitIdle")(queue), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.entry<DeviceWaitIdle>("vkDeviceWaitIdle")(rig.device()), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.entry<DeviceWaitIdle>("vkDeviceWaitIdle")(idle), VK_ERROR_DEVICE_LOST);
  CHECK_EQ(kmt_calls(rig.kmt()), before);
  CHECK_EQ(rig.rig().log().holding("on a lost device"), logged + 14u);
  CHECK_EQ(rig.rig().log().holding("avvk: vkQueueWaitIdle on a lost device"), 1u);
  CHECK_EQ(rig.rig().log().holding("avvk: vkDeviceWaitIdle on a lost device"), 1u);
  CHECK(rig.rig().root().invariants_hold()); }

TEST(device_loss, the_memory_properties_of_a_lost_device_are_empty)
{ LossRig rig;
  rig.lose();
  const std::uint32_t before = kmt_calls(rig.kmt());
  VkPhysicalDeviceMemoryProperties properties = {};
  properties.memoryTypeCount = 9u;
  properties.memoryHeapCount = 9u;
  rig.entry<MemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(rig.physical(), &properties);
  CHECK_EQ(properties.memoryTypeCount, 0u);
  CHECK_EQ(properties.memoryHeapCount, 0u);
  VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {};
  budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
  budget.heapBudget[0] = 77u;
  VkPhysicalDeviceMemoryProperties2 properties2 = {};
  properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
  properties2.pNext = &budget;
  properties2.memoryProperties.memoryTypeCount = 9u;
  rig.entry<MemoryProperties2>("vkGetPhysicalDeviceMemoryProperties2")(rig.physical(), &properties2);
  CHECK_EQ(properties2.memoryProperties.memoryTypeCount, 0u);
  CHECK_EQ(budget.heapBudget[0], 0u);
  CHECK(properties2.pNext == &budget);
  CHECK_EQ(kmt_calls(rig.kmt()), before); }

TEST(device_loss, a_memory_properties_query_the_host_faults_asks_no_budget)
{ LossRig rig;
  const std::uint32_t budgets = rig.kmt().calls(MockKmt::Call::query_video_memory);
  rig.kmt().withhold_answer_at(1u);
  VkPhysicalDeviceMemoryProperties properties = {};
  properties.memoryTypeCount = 9u;
  rig.entry<MemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(rig.physical(), &properties);
  CHECK(rig.connection()->faulted());
  CHECK_EQ(properties.memoryTypeCount, 0u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::query_video_memory), budgets); }

TEST(device_loss, a_view_mapped_before_the_loss_stays_readable_until_freed)
{ LossRig rig;
  const VkDeviceMemory memory = rig.host_visible();
  void *data = nullptr;
  CHECK_EQ(rig.map(memory, data), VK_SUCCESS);
  auto *const view = static_cast<std::uint8_t *>(data);
  for (std::uint64_t offset = 0u; offset < page; offset++) view[offset] = static_cast<std::uint8_t>(pattern ^ offset);
  rig.lose();
  for (std::uint64_t offset = 0u; offset < page; offset++)
      CHECK_EQ(view[offset], static_cast<std::uint8_t>(pattern ^ offset));
  rig.entry<UnmapMemory>("vkUnmapMemory")(rig.device(), memory);
  CHECK_EQ(view[page - 1u], static_cast<std::uint8_t>(pattern ^ (page - 1u)));
  CHECK(rig.kmt().window_chunk_memory(MockKmt::first_window_resource) == view);
  const std::uint32_t objects = rig.kmt().open_objects();
  const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
  const std::uint32_t commands = rig.kmt().commands();
  const std::uint32_t submissions = rig.kmt().calls(MockKmt::Call::submit);
  rig.entry<FreeMemory>("vkFreeMemory")(rig.device(), memory, nullptr);
  CHECK(rig.rig().root().invariants_hold());
  CHECK_EQ(rig.kmt().open_objects(), objects);
  CHECK_EQ(rig.kmt().window_chunks(), 1u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), submissions);
  CHECK_EQ(rig.kmt().commands(), commands);
  CHECK_EQ(rig.map(memory, data), VK_ERROR_DEVICE_LOST); }

TEST(device_loss, a_window_chunk_freed_after_the_loss_is_left_to_the_kernel)
{ LossRig rig;
  rig.kmt().open_host_window(std::uint64_t{128} << 20u);
  const VkDeviceMemory memory = rig.host_visible();
  CHECK_EQ(rig.kmt().window_chunks(), 1u);
  void *data = nullptr;
  CHECK_EQ(rig.map(memory, data), VK_SUCCESS);
  CHECK(data == rig.kmt().window_base());
  static_cast<std::uint8_t *>(data)[page - 1u] = pattern;
  rig.lose();
  CHECK_EQ(static_cast<std::uint8_t *>(data)[page - 1u], pattern);
  rig.entry<UnmapMemory>("vkUnmapMemory")(rig.device(), memory);
  const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
  const std::uint32_t commands = rig.kmt().commands();
  rig.entry<FreeMemory>("vkFreeMemory")(rig.device(), memory, nullptr);
  CHECK(rig.rig().root().invariants_hold());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes);
  CHECK_EQ(rig.kmt().commands(), commands);
  CHECK_EQ(rig.kmt().window_chunks(), 1u); }

TEST(device_loss, freeing_and_destroying_after_the_loss_succeed_locally)
{ LossRig rig;
  static_cast<void>(rig.queue());
  const VkCommandBuffer command_buffers[] = {rig.command_buffer(command_buffer_host),
                                             rig.command_buffer(command_buffer_host + 1u)};
  const VkDeviceMemory mapped = rig.host_visible();
  rig.host().vk_handle(0, allocated);
  VkDeviceMemory local = VK_NULL_HANDLE;
  CHECK_EQ(rig.allocate(page, 0u, local), VK_SUCCESS);
  CHECK(local == as_memory(allocated));
  rig.lose();
  const std::uint32_t submissions = rig.kmt().calls(MockKmt::Call::submit);
  const std::uint32_t waits = rig.kmt().calls(MockKmt::Call::wait_for_context);
  const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
  rig.entry<FreeCommandBuffers>("vkFreeCommandBuffers")(rig.device(), reinterpret_cast<VkCommandPool>(pool_host), 2u,
                                                       command_buffers);
  CHECK(rig.rig().root().invariants_hold());
  CHECK(rig.rig().root().connection_of(command_buffers[0], Kind::command_buffer) == nullptr);
  CHECK(rig.rig().root().connection_of(command_buffers[1], Kind::command_buffer) == nullptr);
  rig.entry<FreeMemory>("vkFreeMemory")(rig.device(), local, nullptr);
  rig.entry<FreeMemory>("vkFreeMemory")(rig.device(), mapped, nullptr);
  CHECK(rig.rig().root().invariants_hold());
  rig.entry<DestroyDevice>("vkDestroyDevice")(rig.device(), nullptr);
  CHECK(rig.rig().root().invariants_hold());
  CHECK(rig.rig().root().connection_of(rig.device(), Kind::device) == nullptr);
  rig.rig().destroy_instance(rig.instance());
  CHECK(rig.rig().root().connection_of(rig.instance(), Kind::instance) == nullptr);
  rig.forget_instance();
  CHECK(rig.rig().root().invariants_hold());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::submit), submissions);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::wait_for_context), waits);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes); }

TEST(device_loss, destroying_a_lost_device_gives_its_chunks_back)
{ LossRig rig;
  static_cast<void>(rig.host_visible());
  rig.lose();
  const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
  const std::uint32_t commands = rig.kmt().commands();
  rig.entry<DestroyDevice>("vkDestroyDevice")(rig.device(), nullptr);
  CHECK(rig.rig().root().invariants_hold());
  CHECK_EQ(rig.kmt().window_chunks(), 1u);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes);
  CHECK_EQ(rig.kmt().commands(), commands); }

TEST(device_loss, a_fault_while_the_types_are_learnt_is_lost)
{ LossRig rig;
  const std::uint32_t objects = rig.kmt().open_objects();
  const std::uint32_t budgets = rig.kmt().calls(MockKmt::Call::query_video_memory);
  rig.kmt().withhold_answer_at(1u);
  VkDeviceMemory memory = VK_NULL_HANDLE;
  CHECK_EQ(rig.allocate(page, 1u, memory), VK_ERROR_DEVICE_LOST);
  CHECK(memory == VK_NULL_HANDLE);
  CHECK(rig.connection()->faulted());
  CHECK_EQ(rig.kmt().open_objects(), objects);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::query_video_memory), budgets);
  CHECK_EQ(rig.rig().log().holding("cannot be learnt"), 1u); }

TEST(device_loss, a_fault_while_importing_is_lost_and_the_chunk_goes_back)
{ LossRig rig;
  rig.host().vk_memory_properties(types, visible);
  rig.kmt().withhold_answer_at(2u);
  const std::uint32_t objects = rig.kmt().open_objects();
  const std::uint32_t escapes = rig.kmt().calls(MockKmt::Call::escape);
  VkDeviceMemory memory = VK_NULL_HANDLE;
  CHECK_EQ(rig.allocate(page, 1u, memory), VK_ERROR_DEVICE_LOST);
  CHECK(memory == VK_NULL_HANDLE);
  CHECK(rig.connection()->faulted());
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::escape), escapes + 1u);
  CHECK_EQ(rig.kmt().window_chunks(), 1u);
  CHECK_EQ(rig.kmt().open_objects(), objects); }

TEST(device_loss, an_import_refused_without_a_loss_is_out_of_device_memory)
{ LossRig rig;
  rig.host().vk_memory_properties(types, visible);
  rig.host().blob_import(4u, 0u);
  VkDeviceMemory memory = VK_NULL_HANDLE;
  CHECK_EQ(rig.allocate(page, 1u, memory), VK_ERROR_OUT_OF_DEVICE_MEMORY);
  CHECK(!rig.connection()->faulted()); }

TEST(device_loss, a_new_instance_opens_a_new_connection_that_works)
{ LossRig rig;
  rig.lose();
  const av::icd::Connection *const lost = rig.connection();
  const VkInstance instance = rig.rig().create_instance();
  CHECK(instance != VK_NULL_HANDLE);
  CHECK(rig.rig().root().invariants_hold());
  av::icd::Connection *const fresh = rig.rig().root().connection_of(instance, Kind::instance);
  CHECK(fresh != nullptr);
  CHECK(fresh != lost);
  CHECK(!fresh->faulted());
  const VkPhysicalDevice physical = rig.rig().physical_device(instance);
  CHECK(physical != VK_NULL_HANDLE);
  VkDeviceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  rig.host().vk_handle(0, ScriptedHost::vk_device);
  VkDevice device = VK_NULL_HANDLE;
  CHECK_EQ(rig.rig().entry<CreateDevice>(instance, "vkCreateDevice")(physical, &info, nullptr, &device), VK_SUCCESS);
  const VkFence fence = reinterpret_cast<VkFence>(fence_host);
  rig.host().vk_result(VK_SUCCESS);
  CHECK_EQ(rig.rig().entry<GetFenceStatus>(instance, "vkGetFenceStatus")(device, fence), VK_SUCCESS);
  CHECK_EQ(rig.entry<GetFenceStatus>("vkGetFenceStatus")(rig.device(), fence), VK_ERROR_DEVICE_LOST);
  rig.rig().destroy_instance(instance);
  CHECK(rig.rig().root().invariants_hold()); }

TEST(device_loss, mapped_ranges_answer_success_live_and_lost_for_a_foreign_device)
{ LossRig rig;
  av::icd::IcdRoot &root = rig.rig().root();
  const std::uint32_t before = kmt_calls(rig.kmt());
  CHECK_EQ(root.mapped_ranges(rig.device(), "vkFlushMappedMemoryRanges"), VK_SUCCESS);
  VkMappedMemoryRange range = {};
  range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  CHECK_EQ(rig.entry<MappedRanges>("vkInvalidateMappedMemoryRanges")(rig.device(), 1u, &range), VK_SUCCESS);
  int stack = 0;
  CHECK_EQ(root.mapped_ranges(reinterpret_cast<VkDevice>(&stack), "vkFlushMappedMemoryRanges"),
           VK_ERROR_DEVICE_LOST);
  CHECK_EQ(rig.rig().log().holding("mapped ranges of a device this driver did not make"), 1u);
  CHECK_EQ(kmt_calls(rig.kmt()), before); }
