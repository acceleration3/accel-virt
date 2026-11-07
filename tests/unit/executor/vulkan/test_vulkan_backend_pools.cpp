#include "core/log/driver_log.hpp"
#include "device/abi/av_executor.h"
#include "executor/exported_memory.hpp"
#include "executor/source_memory.hpp"
#include "executor/vulkan/vulkan_backend.hpp"
#include "harness/executor/vulkan_backend_probe.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "proto/vulkan/fence_wake.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstring>
#include <time.h>

using av::VulkanBackend;
using av::VulkanBackendProbe;
using HandleKind = av::vkproto::HandleKind;

namespace {

constexpr std::uint32_t most_fences = 64u;

std::uint32_t raised_wakes = 0u;

void count_wake(void *)
{ __atomic_fetch_add(&raised_wakes, 1u, __ATOMIC_RELEASE); }

void time_passes()
{ timespec began = {};
  clock_gettime(CLOCK_MONOTONIC, &began);
  for (;;) {
      timespec now = {};
      clock_gettime(CLOCK_MONOTONIC, &now);
      if ((now.tv_sec - began.tv_sec) * 1000000000L + (now.tv_nsec - began.tv_nsec) > 20000000L) break;
  } }

bool wake_arrives()
{ timespec began = {};
  clock_gettime(CLOCK_MONOTONIC, &began);
  for (;;) {
      if (__atomic_load_n(&raised_wakes, __ATOMIC_ACQUIRE) != 0u) return true;
      timespec now = {};
      clock_gettime(CLOCK_MONOTONIC, &now);
      if (now.tv_sec - began.tv_sec > 5) return false;
  } }

struct FakeFence {
    bool alive = false;
    bool signalled = false;
};

struct FakeDriver {
    FakeFence     fences[most_fences] = {};
    std::uint32_t made = 0u;
    std::uint32_t waits = 0u;
    std::uint32_t idles = 0u;
    std::uint32_t pool_destroys = 0u;
    std::uint32_t command_pool_destroys = 0u;
    std::uint32_t status_polls = 0u;
    std::uint32_t pool_resets = 0u;
    std::uint32_t allocations = 0u;
    std::uint32_t command_pool_resets = 0u;
    std::uint32_t command_buffer_resets = 0u;
    std::uint32_t begins = 0u;
    std::uint32_t memory_frees = 0u;
    std::uint64_t freed_memory = 0u;
    std::uint32_t device_destroys = 0u;
};

FakeDriver driver;

template <typename H>
H as_handle(std::uint64_t raw)
{ H handle = VK_NULL_HANDLE;
  std::memcpy(&handle, &raw, sizeof handle);
  return handle; }

template <typename H>
std::uint64_t as_wire(H handle)
{ std::uint64_t raw = 0u;
  std::memcpy(&raw, &handle, sizeof handle);
  return raw; }

std::uint32_t index_of(VkFence fence)
{ return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(fence)) - 1u; }

constexpr std::uint64_t instance = 0x100u;
constexpr std::uint64_t physical = 0x200u;
constexpr std::uint64_t device = 0x300u;
constexpr std::uint64_t queue = 0x400u;
constexpr std::uint64_t command_pool = 0x500u;
constexpr std::uint64_t other_command_pool = 0x501u;
constexpr std::uint64_t cb = 0x600u;
constexpr std::uint64_t other_cb = 0x601u;
constexpr std::uint64_t descriptor_pool = 0x700u;
constexpr std::uint64_t other_descriptor_pool = 0x701u;
constexpr std::uint64_t set = 0x800u;
constexpr std::uint64_t other_set = 0x801u;
constexpr std::uint64_t imported = 0x900u;

class Rig {
public:
    Rig() : sink_(), lines_(sink_, "vulkan: "), backend_(lines_), probe_(backend_)
    {
        driver = FakeDriver{};
        backend_.made(HandleKind::instance, instance);
        backend_.made_through(instance);
        backend_.made(HandleKind::physical_device, physical);
        backend_.made_through(0u);
        VkDeviceCreateInfo create = {};
        create.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        VkDevice made = VK_NULL_HANDLE;
        CHECK_EQ(static_cast<int>(backend_.on_vkCreateDevice(as_handle<VkPhysicalDevice>(physical), &create, nullptr, &made)),
                 static_cast<int>(VK_SUCCESS));
        backend_.made(HandleKind::device, as_wire(made));
        backend_.made_through(device);
        backend_.made(HandleKind::queue, queue);
        backend_.made(HandleKind::command_pool, command_pool);
        backend_.made(HandleKind::command_pool, other_command_pool);
        backend_.made(HandleKind::descriptor_pool, descriptor_pool);
        backend_.made(HandleKind::descriptor_pool, other_descriptor_pool);
        backend_.made_through(command_pool);
        backend_.made(HandleKind::command_buffer, cb);
        backend_.made_through(other_command_pool);
        backend_.made(HandleKind::command_buffer, other_cb);
        backend_.made_through(descriptor_pool);
        backend_.made(HandleKind::descriptor_set, set);
        backend_.made_through(other_descriptor_pool);
        backend_.made(HandleKind::descriptor_set, other_set);
        backend_.made_through(0u);
    }
    ~Rig() { backend_.shut_down(); }

    VulkanBackend &backend() { return backend_; }
    VulkanBackendProbe &probe() { return probe_; }

    void record(std::uint64_t buffer, std::uint64_t named_set)
    { probe_.begin_frame(true);
      std::uint64_t handle = 0u;
      CHECK(backend_.handle_for(HandleKind::command_buffer, buffer, handle));
      CHECK(backend_.handle_for(HandleKind::descriptor_set, named_set, handle));
      probe_.begin_frame(false); }

    void submit(std::uint64_t buffer)
    { VkCommandBuffer buffers[1];
      buffers[0] = as_handle<VkCommandBuffer>(buffer);
      VkSubmitInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      info.commandBufferCount = 1u;
      info.pCommandBuffers = buffers;
      CHECK_EQ(static_cast<int>(backend_.on_vkQueueSubmit(as_handle<VkQueue>(queue), 1u, &info, VK_NULL_HANDLE)),
               static_cast<int>(VK_SUCCESS)); }

    void record_secondary(std::uint64_t primary, std::uint64_t secondary)
    { probe_.begin_frame(true);
      std::uint64_t handle = 0u;
      CHECK(backend_.handle_for(HandleKind::command_buffer, primary, handle));
      CHECK(backend_.handle_for(HandleKind::command_buffer, secondary, handle));
      probe_.begin_frame(false); }

    void command()
    { probe_.begin_frame(true);
      probe_.guard().reap(); }

    void gpu_finishes()
    { for (std::uint32_t index = 0u; index < driver.made; index++) driver.fences[index].signalled = true;
      backend_.end_batch();
      probe_.guard().reap(); }

    VkDevice host_device() const { return as_handle<VkDevice>(device); }

private:
    av::RecordingLog   sink_;
    av::DriverLog      lines_;
    VulkanBackend      backend_;
    VulkanBackendProbe probe_;
};

}

extern "C" {

VKAPI_ATTR VkResult VKAPI_CALL vkCreateFence(VkDevice, const VkFenceCreateInfo *, const VkAllocationCallbacks *, VkFence *fence)
{ if (driver.made == most_fences) return VK_ERROR_OUT_OF_HOST_MEMORY;
  driver.fences[driver.made] = FakeFence{true, false};
  *fence = reinterpret_cast<VkFence>(static_cast<std::uintptr_t>(driver.made++) + 1u);
  return VK_SUCCESS; }

VKAPI_ATTR void VKAPI_CALL vkDestroyFence(VkDevice, VkFence fence, const VkAllocationCallbacks *)
{ driver.fences[index_of(fence)].alive = false; }

VKAPI_ATTR VkResult VKAPI_CALL vkResetFences(VkDevice, uint32_t count, const VkFence *fences)
{ for (uint32_t at = 0u; at < count; at++) driver.fences[index_of(fences[at])].signalled = false;
  return VK_SUCCESS; }

VKAPI_ATTR VkResult VKAPI_CALL vkGetFenceStatus(VkDevice, VkFence fence)
{ driver.status_polls++;
  return driver.fences[index_of(fence)].signalled ? VK_SUCCESS : VK_NOT_READY; }

VKAPI_ATTR VkResult VKAPI_CALL vkWaitForFences(VkDevice, uint32_t, const VkFence *fences, VkBool32, uint64_t)
{ driver.waits++;
  return driver.fences[index_of(fences[0])].signalled ? VK_SUCCESS : VK_TIMEOUT; }

VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(VkQueue, uint32_t, const VkSubmitInfo *, VkFence)
{ return VK_SUCCESS; }

VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice)
{ driver.idles++;
  return VK_SUCCESS; }

VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorPool(VkDevice, VkDescriptorPool, const VkAllocationCallbacks *)
{ driver.pool_destroys++; }

VKAPI_ATTR void VKAPI_CALL vkDestroyCommandPool(VkDevice, VkCommandPool, const VkAllocationCallbacks *)
{ driver.command_pool_destroys++; }

VKAPI_ATTR VkResult VKAPI_CALL vkResetDescriptorPool(VkDevice, VkDescriptorPool, VkDescriptorPoolResetFlags)
{ driver.pool_resets++;
  return VK_SUCCESS; }

VKAPI_ATTR VkResult VKAPI_CALL vkAllocateDescriptorSets(VkDevice, const VkDescriptorSetAllocateInfo *info, VkDescriptorSet *sets)
{ driver.allocations++;
  for (std::uint32_t at = 0u; at < info->descriptorSetCount; at++) sets[at] = as_handle<VkDescriptorSet>(0x900u + at);
  return VK_SUCCESS; }

VKAPI_ATTR VkResult VKAPI_CALL vkResetCommandPool(VkDevice, VkCommandPool, VkCommandPoolResetFlags)
{ driver.command_pool_resets++;
  return VK_SUCCESS; }

VKAPI_ATTR VkResult VKAPI_CALL vkResetCommandBuffer(VkCommandBuffer, VkCommandBufferResetFlags)
{ driver.command_buffer_resets++;
  return VK_SUCCESS; }

VKAPI_ATTR VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo *)
{ driver.begins++;
  return VK_SUCCESS; }

VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(VkInstance, const VkAllocationCallbacks *)
{}

VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(VkDevice, const VkAllocationCallbacks *)
{ driver.device_destroys++; }

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures(VkPhysicalDevice, VkPhysicalDeviceFeatures *features)
{ *features = {};
  features->robustBufferAccess = VK_TRUE; }

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties(VkPhysicalDevice, const char *, uint32_t *count,
                                                                    VkExtensionProperties *)
{ *count = 0u;
  return VK_SUCCESS; }

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice, const VkDeviceCreateInfo *, const VkAllocationCallbacks *,
                                              VkDevice *out)
{ *out = as_handle<VkDevice>(device);
  return VK_SUCCESS; }

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties *properties)
{ *properties = VkPhysicalDeviceMemoryProperties{}; }

VKAPI_ATTR void VKAPI_CALL vkFreeMemory(VkDevice, VkDeviceMemory memory, const VkAllocationCallbacks *)
{ driver.memory_frees++;
  driver.freed_memory = as_wire(memory); }

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance, const char *name)
{ if (std::strcmp(name, "vkGetPhysicalDeviceMemoryProperties") == 0)
      return reinterpret_cast<PFN_vkVoidFunction>(&vkGetPhysicalDeviceMemoryProperties);
  return nullptr; }

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice, const char *name)
{ if (std::strcmp(name, "vkFreeMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&vkFreeMemory);
  return nullptr; }

}

TEST(vulkan_backend_pools, a_descriptor_pool_destroyed_with_a_submitted_set_it_never_freed_is_destroyed_once_the_gpu_is_past_the_set)
{ Rig rig;
  rig.record(cb, set);
  rig.submit(cb);
  rig.backend().on_vkDestroyDescriptorPool(rig.host_device(), as_handle<VkDescriptorPool>(descriptor_pool), nullptr);
  CHECK_EQ(driver.pool_destroys, 0u);
  rig.gpu_finishes();
  CHECK_EQ(driver.pool_destroys, 1u); }

TEST(vulkan_backend_pools, a_command_pool_destroyed_with_a_submitted_secondary_it_never_freed_is_destroyed_once_the_gpu_is_past_it)
{ Rig rig;
  rig.record_secondary(cb, other_cb);
  rig.submit(cb);
  rig.backend().on_vkDestroyCommandPool(rig.host_device(), as_handle<VkCommandPool>(other_command_pool), nullptr);
  CHECK_EQ(driver.command_pool_destroys, 0u);
  rig.gpu_finishes();
  CHECK_EQ(driver.command_pool_destroys, 1u); }

TEST(vulkan_backend_pools, the_fences_are_polled_once_per_batch_however_many_commands_reap_between)
{ Rig rig;
  rig.record(cb, set);
  rig.submit(cb);
  rig.backend().on_vkDestroyDescriptorPool(rig.host_device(), as_handle<VkDescriptorPool>(descriptor_pool), nullptr);
  const std::uint32_t polled = driver.status_polls;
  for (std::uint32_t command = 0u; command < 10u; command++) rig.command();
  CHECK_EQ(driver.status_polls, polled);
  rig.backend().end_batch();
  for (std::uint32_t command = 0u; command < 10u; command++) rig.command();
  CHECK_EQ(driver.status_polls, polled + 1u);
  rig.backend().end_batch();
  rig.command();
  CHECK_EQ(driver.status_polls, polled + 2u); }

TEST(vulkan_backend_pools, resetting_a_descriptor_pool_waits_for_its_own_sets_only_and_never_for_other_work)
{ Rig rig;
  rig.record(cb, set);
  rig.submit(cb);
  const VkResult reset = rig.backend().on_vkResetDescriptorPool(rig.host_device(),
                                                                as_handle<VkDescriptorPool>(other_descriptor_pool), 0u);
  CHECK_EQ(static_cast<int>(reset), static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.waits, 0u);
  CHECK_EQ(driver.pool_resets, 1u); }

TEST(vulkan_backend_pools, a_descriptor_pool_reset_the_gpu_does_not_finish_in_time_never_fails_and_is_done_later)
{ Rig rig;
  rig.record(cb, set);
  rig.submit(cb);
  const VkResult reset = rig.backend().on_vkResetDescriptorPool(rig.host_device(),
                                                                as_handle<VkDescriptorPool>(descriptor_pool), 0u);
  CHECK_EQ(static_cast<int>(reset), static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.pool_resets, 0u);
  rig.gpu_finishes();
  CHECK_EQ(driver.pool_resets, 1u); }

TEST(vulkan_backend_pools, resetting_a_command_pool_waits_for_its_own_buffers_only)
{ Rig rig;
  rig.record(cb, set);
  rig.submit(cb);
  CHECK_EQ(static_cast<int>(rig.backend().on_vkResetCommandPool(rig.host_device(),
                                                                as_handle<VkCommandPool>(other_command_pool), 0u)),
           static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.waits, 0u);
  CHECK_EQ(driver.command_pool_resets, 1u); }

TEST(vulkan_backend_pools, a_command_pool_reset_the_gpu_does_not_finish_in_time_is_refused_with_a_code_the_entry_point_may_return)
{ Rig rig;
  rig.record(cb, set);
  rig.submit(cb);
  CHECK_EQ(static_cast<int>(rig.backend().on_vkResetCommandPool(rig.host_device(), as_handle<VkCommandPool>(command_pool), 0u)),
           static_cast<int>(VK_ERROR_OUT_OF_DEVICE_MEMORY));
  CHECK_EQ(driver.command_pool_resets, 0u);
  rig.gpu_finishes();
  CHECK_EQ(static_cast<int>(rig.backend().on_vkResetCommandPool(rig.host_device(), as_handle<VkCommandPool>(command_pool), 0u)),
           static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.command_pool_resets, 1u); }

TEST(vulkan_backend_pools, a_command_buffer_reset_the_gpu_does_not_finish_in_time_is_refused_with_a_code_the_entry_point_may_return)
{ Rig rig;
  rig.record(cb, set);
  rig.submit(cb);
  CHECK_EQ(static_cast<int>(rig.backend().on_vkResetCommandBuffer(as_handle<VkCommandBuffer>(cb), 0u)),
           static_cast<int>(VK_ERROR_OUT_OF_DEVICE_MEMORY));
  CHECK_EQ(static_cast<int>(rig.backend().on_vkBeginCommandBuffer(as_handle<VkCommandBuffer>(cb), nullptr)),
           static_cast<int>(VK_ERROR_OUT_OF_HOST_MEMORY));
  CHECK_EQ(driver.command_buffer_resets, 0u);
  CHECK_EQ(driver.begins, 0u);
  CHECK_EQ(static_cast<int>(rig.backend().on_vkResetCommandBuffer(as_handle<VkCommandBuffer>(other_cb), 0u)),
           static_cast<int>(VK_SUCCESS)); }

TEST(vulkan_backend_pools, imported_memory_abandoned_while_a_submission_may_use_it_is_freed_after_the_gpu_is_past_it)
{ Rig rig;
  rig.backend().made_through(device);
  rig.backend().made(HandleKind::device_memory, imported);
  rig.backend().made_through(0u);
  rig.record(cb, set);
  rig.submit(cb);
  const av::SourceMemory memory(1u, 2u, 0u, 0u, av::ExportedMemory(1u, -1, nullptr, 0u));
  rig.backend().abandon(memory, imported);
  CHECK_EQ(driver.memory_frees, 0u);
  CHECK(!rig.backend().knows_handle(imported));
  rig.gpu_finishes();
  CHECK_EQ(driver.memory_frees, 1u);
  CHECK_EQ(driver.freed_memory, imported); }

TEST(vulkan_backend_pools, imported_memory_abandoned_while_the_gpu_is_idle_is_freed_at_once)
{ Rig rig;
  rig.backend().made_through(device);
  rig.backend().made(HandleKind::device_memory, imported);
  rig.backend().made_through(0u);
  const av::SourceMemory memory(1u, 2u, 0u, 0u, av::ExportedMemory(1u, -1, nullptr, 0u));
  rig.backend().abandon(memory, imported);
  CHECK_EQ(driver.memory_frees, 1u); }

TEST(vulkan_backend_pools, shared_memory_released_while_a_submission_may_use_it_is_freed_after_the_gpu_is_past_it)
{ Rig rig;
  rig.backend().made_through(device);
  rig.backend().made(HandleKind::device_memory, imported);
  rig.backend().made_through(0u);
  rig.record(cb, set);
  rig.submit(cb);
  rig.backend().release_object(AV_SHARED_MEMORY, imported, 0u);
  CHECK_EQ(driver.memory_frees, 0u);
  CHECK(!rig.backend().knows_handle(imported));
  rig.gpu_finishes();
  CHECK_EQ(driver.memory_frees, 1u);
  CHECK_EQ(driver.freed_memory, imported); }

TEST(vulkan_backend_pools, shared_memory_released_while_the_gpu_is_idle_is_freed_at_once)
{ Rig rig;
  rig.backend().made_through(device);
  rig.backend().made(HandleKind::device_memory, imported);
  rig.backend().made_through(0u);
  rig.backend().release_object(AV_SHARED_MEMORY, imported, 0u);
  CHECK_EQ(driver.memory_frees, 1u); }

TEST(vulkan_backend_pools, a_pool_reset_waits_only_for_the_submissions_its_sets_were_stamped_by_and_not_for_older_work_elsewhere)
{ Rig rig;
  rig.record(cb, set);
  rig.submit(cb);
  rig.record(other_cb, other_set);
  rig.submit(other_cb);
  driver.fences[1].signalled = true;
  const VkResult reset = rig.backend().on_vkResetDescriptorPool(rig.host_device(),
                                                                as_handle<VkDescriptorPool>(other_descriptor_pool), 0u);
  CHECK_EQ(static_cast<int>(reset), static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.pool_resets, 1u); }

TEST(vulkan_backend_pools, a_command_pool_reset_waits_only_for_the_submissions_its_buffers_were_stamped_by)
{ Rig rig;
  rig.record(cb, set);
  rig.submit(cb);
  rig.record(other_cb, other_set);
  rig.submit(other_cb);
  driver.fences[1].signalled = true;
  CHECK_EQ(static_cast<int>(rig.backend().on_vkResetCommandPool(rig.host_device(),
                                                                as_handle<VkCommandPool>(other_command_pool), 0u)),
           static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.command_pool_resets, 1u); }

TEST(vulkan_backend_pools, allocations_from_a_pool_with_a_reset_still_pending_are_refused_until_the_reset_is_done)
{ Rig rig;
  rig.record(cb, set);
  rig.submit(cb);
  VkDescriptorSetLayout layouts[1] = {};
  VkDescriptorSetAllocateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  info.descriptorPool = as_handle<VkDescriptorPool>(descriptor_pool);
  info.descriptorSetCount = 1u;
  info.pSetLayouts = layouts;
  VkDescriptorSet made[1] = {};
  CHECK_EQ(static_cast<int>(rig.backend().on_vkResetDescriptorPool(rig.host_device(), info.descriptorPool, 0u)),
           static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.pool_resets, 0u);
  CHECK_EQ(static_cast<int>(rig.backend().on_vkAllocateDescriptorSets(rig.host_device(), &info, made)),
           static_cast<int>(VK_ERROR_OUT_OF_POOL_MEMORY));
  CHECK_EQ(driver.allocations, 0u);
  info.descriptorPool = as_handle<VkDescriptorPool>(other_descriptor_pool);
  CHECK_EQ(static_cast<int>(rig.backend().on_vkAllocateDescriptorSets(rig.host_device(), &info, made)),
           static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.allocations, 1u);
  rig.gpu_finishes();
  CHECK_EQ(driver.pool_resets, 1u);
  info.descriptorPool = as_handle<VkDescriptorPool>(descriptor_pool);
  CHECK_EQ(static_cast<int>(rig.backend().on_vkAllocateDescriptorSets(rig.host_device(), &info, made)),
           static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.allocations, 2u); }

TEST(vulkan_backend_pools, a_second_reset_of_a_pool_whose_first_is_pending_waits_for_it_and_resets_the_pool_once)
{ Rig rig;
  rig.record(cb, set);
  rig.submit(cb);
  const VkDescriptorPool pool = as_handle<VkDescriptorPool>(descriptor_pool);
  CHECK_EQ(static_cast<int>(rig.backend().on_vkResetDescriptorPool(rig.host_device(), pool, 0u)),
           static_cast<int>(VK_SUCCESS));
  CHECK_EQ(static_cast<int>(rig.backend().on_vkResetDescriptorPool(rig.host_device(), pool, 0u)),
           static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.pool_resets, 0u);
  rig.gpu_finishes();
  CHECK_EQ(driver.pool_resets, 1u);
  CHECK_EQ(static_cast<int>(rig.backend().on_vkResetDescriptorPool(rig.host_device(), pool, 0u)),
           static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.pool_resets, 2u);
  rig.gpu_finishes();
  CHECK_EQ(driver.pool_resets, 2u); }

TEST(vulkan_backend_pools, a_reset_after_the_gpu_passed_the_pending_one_runs_it_first_and_the_pool_takes_allocations_again)
{ Rig rig;
  rig.record(cb, set);
  rig.submit(cb);
  const VkDescriptorPool pool = as_handle<VkDescriptorPool>(descriptor_pool);
  CHECK_EQ(static_cast<int>(rig.backend().on_vkResetDescriptorPool(rig.host_device(), pool, 0u)),
           static_cast<int>(VK_SUCCESS));
  for (std::uint32_t index = 0u; index < driver.made; index++) driver.fences[index].signalled = true;
  rig.backend().end_batch();
  CHECK_EQ(static_cast<int>(rig.backend().on_vkResetDescriptorPool(rig.host_device(), pool, 0u)),
           static_cast<int>(VK_SUCCESS));
  CHECK(driver.pool_resets >= 1u);
  VkDescriptorSetLayout layouts[1] = {};
  VkDescriptorSetAllocateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  info.descriptorPool = pool;
  info.descriptorSetCount = 1u;
  info.pSetLayouts = layouts;
  VkDescriptorSet made[1] = {};
  CHECK_EQ(static_cast<int>(rig.backend().on_vkAllocateDescriptorSets(rig.host_device(), &info, made)),
           static_cast<int>(VK_SUCCESS));
  const std::uint32_t resets = driver.pool_resets;
  rig.gpu_finishes();
  CHECK_EQ(driver.pool_resets, resets); }

TEST(vulkan_backend_pools, a_fence_wait_that_asks_for_a_wake_answers_at_once_and_wakes_the_guest_when_the_fence_signals)
{ Rig rig;
  raised_wakes = 0u;
  rig.backend().bind_signal(&count_wake, nullptr);
  driver.fences[driver.made] = FakeFence{true, false};
  const VkFence fence = reinterpret_cast<VkFence>(static_cast<std::uintptr_t>(driver.made++) + 1u);
  CHECK_EQ(static_cast<int>(rig.backend().on_vkWaitForFences(rig.host_device(), 1u, &fence, VK_TRUE,
                                                              av::vkproto::FenceWake().arm_timeout())),
           static_cast<int>(VK_TIMEOUT));
  CHECK_EQ(__atomic_load_n(&raised_wakes, __ATOMIC_ACQUIRE), 0u);
  driver.fences[index_of(fence)].signalled = true;
  CHECK(wake_arrives());
  rig.backend().bind_signal(nullptr, nullptr); }

TEST(vulkan_backend_pools, a_fence_wait_that_asks_for_a_wake_on_a_signalled_fence_answers_success_and_wakes_nobody)
{ Rig rig;
  raised_wakes = 0u;
  rig.backend().bind_signal(&count_wake, nullptr);
  driver.fences[driver.made] = FakeFence{true, true};
  const VkFence fence = reinterpret_cast<VkFence>(static_cast<std::uintptr_t>(driver.made++) + 1u);
  CHECK_EQ(static_cast<int>(rig.backend().on_vkWaitForFences(rig.host_device(), 1u, &fence, VK_TRUE,
                                                              av::vkproto::FenceWake().arm_timeout())),
           static_cast<int>(VK_SUCCESS));
  rig.backend().bind_signal(nullptr, nullptr);
  CHECK_EQ(__atomic_load_n(&raised_wakes, __ATOMIC_ACQUIRE), 0u); }

TEST(vulkan_backend_pools, a_fence_wait_that_asks_for_a_wake_with_nothing_to_wake_the_guest_says_not_ready)
{ Rig rig;
  driver.fences[driver.made] = FakeFence{true, false};
  const VkFence fence = reinterpret_cast<VkFence>(static_cast<std::uintptr_t>(driver.made++) + 1u);
  CHECK_EQ(static_cast<int>(rig.backend().on_vkWaitForFences(rig.host_device(), 1u, &fence, VK_TRUE,
                                                              av::vkproto::FenceWake().arm_timeout())),
           static_cast<int>(VK_NOT_READY)); }

TEST(vulkan_backend_pools, any_other_fence_wait_is_passed_to_the_driver_untouched)
{ Rig rig;
  driver.fences[driver.made] = FakeFence{true, false};
  const VkFence fence = reinterpret_cast<VkFence>(static_cast<std::uintptr_t>(driver.made++) + 1u);
  const std::uint32_t before = driver.waits;
  CHECK_EQ(static_cast<int>(rig.backend().on_vkWaitForFences(rig.host_device(), 1u, &fence, VK_TRUE, 1000u)),
           static_cast<int>(VK_TIMEOUT));
  CHECK_EQ(driver.waits, before + 1u); }

TEST(vulkan_backend_pools, destroying_or_resetting_a_fence_a_wake_waits_on_ends_the_wake_first)
{ Rig rig;
  raised_wakes = 0u;
  rig.backend().bind_signal(&count_wake, nullptr);
  driver.fences[driver.made] = FakeFence{true, false};
  const VkFence fence = reinterpret_cast<VkFence>(static_cast<std::uintptr_t>(driver.made++) + 1u);
  CHECK_EQ(static_cast<int>(rig.backend().on_vkWaitForFences(rig.host_device(), 1u, &fence, VK_TRUE,
                                                              av::vkproto::FenceWake().arm_timeout())),
           static_cast<int>(VK_TIMEOUT));
  CHECK_EQ(static_cast<int>(rig.backend().on_vkResetFences(rig.host_device(), 1u, &fence)), static_cast<int>(VK_SUCCESS));
  time_passes();
  const std::uint32_t after_reset = driver.waits;
  time_passes();
  CHECK_EQ(driver.waits, after_reset);
  driver.fences[index_of(fence)].signalled = true;
  CHECK_EQ(static_cast<int>(rig.backend().on_vkWaitForFences(rig.host_device(), 1u, &fence, VK_TRUE,
                                                              av::vkproto::FenceWake().arm_timeout())),
           static_cast<int>(VK_SUCCESS));
  driver.fences[index_of(fence)].signalled = false;
  CHECK_EQ(static_cast<int>(rig.backend().on_vkWaitForFences(rig.host_device(), 1u, &fence, VK_TRUE,
                                                              av::vkproto::FenceWake().arm_timeout())),
           static_cast<int>(VK_TIMEOUT));
  rig.backend().on_vkDestroyFence(rig.host_device(), fence, nullptr);
  CHECK(!driver.fences[index_of(fence)].alive);
  const std::uint32_t settled = driver.waits;
  time_passes();
  CHECK_EQ(driver.waits, settled);
  CHECK_EQ(__atomic_load_n(&raised_wakes, __ATOMIC_ACQUIRE), 0u);
  rig.backend().bind_signal(nullptr, nullptr); }

TEST(vulkan_backend_pools, a_command_buffer_forgotten_and_made_again_under_another_device_is_guarded_under_that_device)
{ Rig rig;
  constexpr std::uint64_t other_device = 0x310u;
  rig.backend().made_through(physical);
  rig.backend().made(HandleKind::device, other_device);
  rig.backend().made_through(0u);
  rig.record(cb, set);
  CHECK_EQ(rig.probe().guard().devices(), 1u);
  rig.backend().ended(cb);
  rig.backend().made_through(other_device);
  rig.backend().made(HandleKind::command_buffer, cb);
  rig.backend().made_through(0u);
  rig.record(cb, set);
  CHECK_EQ(rig.probe().guard().devices(), 2u); }

TEST(vulkan_backend_pools, destroying_the_device_a_wake_waits_on_ends_the_wake_first)
{ Rig rig;
  raised_wakes = 0u;
  rig.backend().bind_signal(&count_wake, nullptr);
  driver.fences[driver.made] = FakeFence{true, false};
  const VkFence fence = reinterpret_cast<VkFence>(static_cast<std::uintptr_t>(driver.made++) + 1u);
  CHECK_EQ(static_cast<int>(rig.backend().on_vkWaitForFences(rig.host_device(), 1u, &fence, VK_TRUE,
                                                              av::vkproto::FenceWake().arm_timeout())),
           static_cast<int>(VK_TIMEOUT));
  rig.backend().on_vkDestroyDevice(rig.host_device(), nullptr);
  const std::uint32_t settled = driver.waits;
  time_passes();
  CHECK_EQ(driver.waits, settled);
  CHECK_EQ(__atomic_load_n(&raised_wakes, __ATOMIC_ACQUIRE), 0u);
  rig.backend().bind_signal(nullptr, nullptr); }
