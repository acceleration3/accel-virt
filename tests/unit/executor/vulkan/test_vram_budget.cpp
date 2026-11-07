#include "core/log/driver_log.hpp"
#include "executor/vulkan/vram_budget.hpp"
#include "executor/vulkan/vram_policy.hpp"
#include "executor/vulkan/vulkan_backend.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/failing_blocks.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstring>

using av::VramBudget;
using av::VramPolicy;

namespace {

constexpr std::uint64_t gib = 1ull << 30;

struct MemoryDriver {
    VkResult      allocate_result = VK_SUCCESS;
    std::uint32_t allocations = 0u;
    std::uint32_t frees = 0u;
    std::uint64_t next = 0x4000u;
};

MemoryDriver driver;

VkPhysicalDeviceMemoryProperties host_with_vram(std::uint64_t vram)
{ VkPhysicalDeviceMemoryProperties host = {};
  host.memoryHeapCount = 2u;
  host.memoryHeaps[0].size = vram;
  host.memoryHeaps[0].flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
  host.memoryHeaps[1].size = 32 * gib;
  host.memoryTypeCount = 2u;
  host.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  host.memoryTypes[0].heapIndex = 0u;
  host.memoryTypes[1].propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  host.memoryTypes[1].heapIndex = 1u;
  return host; }

std::uint64_t host_vram = 8 * gib;

class Rig {
public:
    Rig() : blocks_(), sink_(), lines_(sink_, "vulkan: "), backend_(blocks_, lines_)
    { driver = MemoryDriver{};
      host_vram = 8 * gib; }
    ~Rig() { backend_.shut_down(); }

    av::VulkanBackend &backend() { return backend_; }
    av::FailingBlocks &blocks() { return blocks_; }

    VkPhysicalDevice physical() const { return reinterpret_cast<VkPhysicalDevice>(std::uintptr_t{0x200}); }
    VkDevice         device() const { return reinterpret_cast<VkDevice>(std::uintptr_t{0x300}); }

    VkPhysicalDeviceMemoryProperties properties()
    { VkPhysicalDeviceMemoryProperties answer = {};
      backend_.on_vkGetPhysicalDeviceMemoryProperties(physical(), &answer);
      return answer; }

    std::uint32_t device_local_type()
    { const VkPhysicalDeviceMemoryProperties known = properties();
      for (std::uint32_t type = 0u; type < known.memoryTypeCount; type++)
          if ((known.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0u) return type;
      return 0u; }

    VkResult allocate(std::uint32_t type, std::uint64_t size, VkDeviceMemory &made)
    { VkMemoryAllocateInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      info.allocationSize = size;
      info.memoryTypeIndex = type;
      return backend_.on_vkAllocateMemory(device(), &info, nullptr, &made); }

private:
    av::FailingBlocks blocks_;
    av::RecordingLog  sink_;
    av::DriverLog     lines_;
    av::VulkanBackend backend_;
};

}

extern "C" {

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties *host)
{ *host = host_with_vram(host_vram); }

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties2(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties2 *host)
{ host->memoryProperties = host_with_vram(host_vram); }

VKAPI_ATTR VkResult VKAPI_CALL vkAllocateMemory(VkDevice, const VkMemoryAllocateInfo *, const VkAllocationCallbacks *,
                                                VkDeviceMemory *memory)
{
    driver.allocations++;
    if (driver.allocate_result != VK_SUCCESS) return driver.allocate_result;
    driver.next += 0x10u;
    *memory = reinterpret_cast<VkDeviceMemory>(driver.next);
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkFreeMemory(VkDevice, VkDeviceMemory, const VkAllocationCallbacks *)
{ driver.frees++; }

}

TEST(vram_policy, auto_reserves_an_eighth_of_the_card_and_never_less_than_a_gibibyte)
{ const VramPolicy policy;
  CHECK_EQ(policy.cap_for(16 * gib), 14 * gib);
  CHECK_EQ(policy.cap_for(8 * gib), 7 * gib);
  CHECK_EQ(policy.cap_for(4 * gib), 3 * gib);
  CHECK_EQ(policy.cap_for(2 * gib), 1 * gib);
  CHECK_EQ(policy.cap_for(1 * gib), gib / 2u);
  CHECK_EQ(policy.cap_for(24 * gib), 21 * gib);
  CHECK_EQ(policy.reserve_for(16 * gib), 2 * gib);
  CHECK_EQ(policy.reserve_for(4 * gib), gib); }

TEST(vram_policy, an_explicit_reserve_is_clamped_between_256_mib_and_the_card_less_512_mib)
{ const std::uint64_t mib = 1ull << 20;
  CHECK_EQ(VramPolicy(4096u).reserve_for(16 * gib), 4096 * mib);
  CHECK_EQ(VramPolicy(4096u).cap_for(16 * gib), 12 * gib);
  CHECK_EQ(VramPolicy(1u).reserve_for(16 * gib), 256 * mib);
  CHECK_EQ(VramPolicy(65536u).reserve_for(16 * gib), 16 * gib - 512 * mib);
  CHECK_EQ(VramPolicy(65536u).cap_for(16 * gib), 8 * gib);
  CHECK_EQ(VramPolicy(4096u).reserve_for(256 * mib), 0u);
  CHECK_EQ(VramPolicy(4096u).cap_for(256 * mib), 256 * mib); }

TEST(vram_budget, a_commit_that_cannot_be_recorded_leaves_the_books_as_they_were)
{ av::FailingBlocks blocks;
  VramBudget budget(blocks);
  budget.learn(host_with_vram(8 * gib), VramPolicy());
  CHECK(budget.commit(0u, 0x10u, gib) == av::Status::ok);
  blocks.fail_from(blocks.requests() + 1u);
  av::Status refused = av::Status::ok;
  for (std::uint64_t key = 0x20u; key < 0x20u + 64u && refused == av::Status::ok; key++)
      refused = budget.commit(0u, key, 1u);
  CHECK(refused != av::Status::ok);
  CHECK(budget.invariants_hold());
  budget.release(0x10u);
  CHECK(budget.invariants_hold()); }

TEST(vram_budget, only_device_local_heaps_are_capped)
{ av::FailingBlocks blocks;
  VramBudget budget(blocks);
  budget.learn(host_with_vram(8 * gib), VramPolicy());
  VkPhysicalDeviceMemoryProperties answer = host_with_vram(8 * gib);
  budget.clamp(answer);
  CHECK_EQ(answer.memoryHeaps[0].size, 7 * gib);
  CHECK_EQ(answer.memoryHeaps[1].size, 32 * gib);
  CHECK(budget.fits(0u, 7 * gib));
  CHECK(!budget.fits(0u, 7 * gib + 1u));
  CHECK(budget.fits(1u, 31 * gib)); }

TEST(vram_budget, the_budget_report_follows_commits_and_releases)
{ av::FailingBlocks blocks;
  VramBudget budget(blocks);
  budget.learn(host_with_vram(8 * gib), VramPolicy());
  CHECK(budget.commit(0u, 0x10u, 2 * gib) == av::Status::ok);
  VkPhysicalDeviceMemoryBudgetPropertiesEXT report = {};
  budget.report(report);
  CHECK_EQ(report.heapBudget[0], 7 * gib);
  CHECK_EQ(report.heapUsage[0], 2 * gib);
  CHECK_EQ(report.heapBudget[2], std::uint64_t{0});
  CHECK(!budget.fits(0u, 5 * gib + 1u));
  budget.release(0x10u);
  budget.release(0x10u);
  CHECK(budget.fits(0u, 7 * gib));
  CHECK_EQ(budget.used(0u), std::uint64_t{0});
  CHECK(budget.invariants_hold()); }

TEST(vulkan_vram_cap, the_guest_sees_the_capped_heap_and_a_budget_that_shrinks_as_it_allocates)
{ Rig rig;
  CHECK_EQ(rig.properties().memoryHeaps[0].size, 7 * gib);
  VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {};
  budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
  VkPhysicalDeviceMemoryProperties2 query = {};
  query.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
  query.pNext = &budget;
  rig.backend().on_vkGetPhysicalDeviceMemoryProperties2(rig.physical(), &query);
  CHECK_EQ(budget.heapBudget[0], 7 * gib);
  CHECK_EQ(budget.heapUsage[0], std::uint64_t{0});
  VkDeviceMemory made = VK_NULL_HANDLE;
  CHECK_EQ(static_cast<int>(rig.allocate(rig.device_local_type(), 3 * gib, made)), static_cast<int>(VK_SUCCESS));
  rig.backend().on_vkGetPhysicalDeviceMemoryProperties2(rig.physical(), &query);
  CHECK_EQ(budget.heapUsage[0], 3 * gib);
  CHECK_EQ(budget.heapBudget[0], 7 * gib); }

TEST(vulkan_vram_cap, an_allocation_past_the_cap_is_refused_without_reaching_the_driver)
{ Rig rig;
  static_cast<void>(rig.properties());
  VkDeviceMemory made = VK_NULL_HANDLE;
  CHECK_EQ(static_cast<int>(rig.allocate(rig.device_local_type(), 4 * gib, made)), static_cast<int>(VK_SUCCESS));
  VkDeviceMemory refused = VK_NULL_HANDLE;
  CHECK_EQ(static_cast<int>(rig.allocate(rig.device_local_type(), 4 * gib, refused)), static_cast<int>(VK_ERROR_OUT_OF_DEVICE_MEMORY));
  CHECK_EQ(driver.allocations, 1u);
  CHECK_EQ(static_cast<int>(rig.allocate(rig.device_local_type(), 3 * gib, refused)), static_cast<int>(VK_SUCCESS)); }

TEST(vulkan_vram_cap, a_driver_that_runs_out_passes_its_error_through_and_books_nothing)
{ Rig rig;
  static_cast<void>(rig.properties());
  driver.allocate_result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
  VkDeviceMemory made = VK_NULL_HANDLE;
  CHECK_EQ(static_cast<int>(rig.allocate(rig.device_local_type(), gib, made)), static_cast<int>(VK_ERROR_OUT_OF_DEVICE_MEMORY));
  driver.allocate_result = VK_ERROR_OUT_OF_HOST_MEMORY;
  CHECK_EQ(static_cast<int>(rig.allocate(rig.device_local_type(), gib, made)), static_cast<int>(VK_ERROR_OUT_OF_HOST_MEMORY));
  driver.allocate_result = VK_SUCCESS;
  CHECK_EQ(static_cast<int>(rig.allocate(rig.device_local_type(), 7 * gib, made)), static_cast<int>(VK_SUCCESS)); }

TEST(vulkan_vram_cap, an_allocation_the_host_cannot_record_is_given_back_and_reported_as_host_memory)
{ Rig rig;
  static_cast<void>(rig.properties());
  rig.blocks().fail_from(rig.blocks().requests() + 1u);
  VkDeviceMemory made = reinterpret_cast<VkDeviceMemory>(std::uintptr_t{1});
  VkResult result = VK_SUCCESS;
  std::uint32_t tries = 0u;
  while (result == VK_SUCCESS && tries < 200u) {
      result = rig.allocate(rig.device_local_type(), 1u, made);
      tries++;
  }
  CHECK_EQ(static_cast<int>(result), static_cast<int>(VK_ERROR_OUT_OF_HOST_MEMORY));
  CHECK(made == VK_NULL_HANDLE);
  CHECK_EQ(driver.frees, 1u); }
