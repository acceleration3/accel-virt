#include "core/log/driver_log.hpp"
#include "executor/host_ram_ledger.hpp"
#include "harness/executor/fake_memory_gauge.hpp"
#include "executor/vram_account.hpp"
#include "executor/vram_ledger.hpp"
#include "executor/vulkan/vram_budget.hpp"
#include "proto/context/vram_policy.hpp"
#include "executor/vulkan/vulkan_backend.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/failing_blocks.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstring>

using av::HostRamLedger;
using av::VramAccount;
using av::VramBudget;
using av::VramLedger;
using av::VramPolicy;

namespace {

constexpr std::uint64_t gib = 1ull << 30;

struct MemoryDriver {
    VkResult      allocate_result = VK_SUCCESS;
    std::uint32_t refuse_next = 0u;
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
    Rig() : blocks_(), sink_(), lines_(sink_, "vulkan: "), backend_(blocks_, lines_), ledger_()
    { driver = MemoryDriver{};
      host_vram = 8 * gib;
      ledger_.learn(8 * gib);
      backend_.use_vram(&ledger_, nullptr); }
    ~Rig() { backend_.shut_down(); }

    av::VulkanBackend &backend() { return backend_; }
    av::FailingBlocks &blocks() { return blocks_; }
    av::RecordingLog  &lines() { return sink_; }
    VramLedger        &ledger() { return ledger_; }

    VkPhysicalDevice physical() const { return reinterpret_cast<VkPhysicalDevice>(std::uintptr_t{0x200}); }
    VkDevice         device() const { return reinterpret_cast<VkDevice>(std::uintptr_t{0x300}); }

    VkPhysicalDeviceMemoryProperties properties()
    { VkPhysicalDeviceMemoryProperties answer = {};
      backend_.on_vkGetPhysicalDeviceMemoryProperties(physical(), &answer);
      return answer; }

    std::uint32_t host_visible_type()
    { const VkPhysicalDeviceMemoryProperties known = properties();
      for (std::uint32_t type = 0u; type < known.memoryTypeCount; type++)
          if ((known.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u) return type;
      return 0u; }

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
    VramLedger        ledger_;
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
    if (driver.refuse_next != 0u) {
        driver.refuse_next--;
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    if (driver.allocate_result != VK_SUCCESS) return driver.allocate_result;
    driver.next += 0x10u;
    *memory = reinterpret_cast<VkDeviceMemory>(driver.next);
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties(VkPhysicalDevice, VkPhysicalDeviceProperties *host)
{ *host = VkPhysicalDeviceProperties{};
  std::strcpy(host->deviceName, "stub gpu"); }

VKAPI_ATTR void VKAPI_CALL vkFreeMemory(VkDevice, VkDeviceMemory, const VkAllocationCallbacks *)
{ driver.frees++; }

}

VkPhysicalDeviceMemoryProperties guest_view()
{ return host_with_vram(8 * gib); }

TEST(vram_budget, a_commit_that_cannot_be_recorded_leaves_the_books_as_they_were)
{ av::FailingBlocks blocks;
  VramLedger ledger;
  ledger.learn(8 * gib);
  VramBudget budget(blocks);
  budget.bind(&ledger, nullptr);
  budget.learn(guest_view());
  CHECK(budget.admit(0u, gib));
  CHECK(budget.commit(0u, 0x10u, gib) == av::Status::ok);
  blocks.fail_from(blocks.requests() + 1u);
  av::Status refused = av::Status::ok;
  std::uint64_t key = 0x20u;
  for (; key < 0x20u + 64u && refused == av::Status::ok; key++) {
      CHECK(budget.admit(0u, 1u));
      refused = budget.commit(0u, key, 1u);
  }
  CHECK(refused != av::Status::ok);
  budget.refund(0u, 1u);
  CHECK(budget.invariants_hold());
  budget.release(0x10u);
  for (std::uint64_t each = 0x20u; each < key - 1u; each++) budget.release(each);
  CHECK(budget.invariants_hold());
  CHECK_EQ(ledger.used(), std::uint64_t{0}); }

TEST(vram_budget, only_device_local_heaps_are_capped)
{ av::FailingBlocks blocks;
  VramLedger ledger;
  ledger.learn(8 * gib);
  VramBudget budget(blocks);
  budget.bind(&ledger, nullptr);
  budget.learn(guest_view());
  VkPhysicalDeviceMemoryProperties answer = guest_view();
  budget.clamp(answer);
  CHECK_EQ(answer.memoryHeaps[0].size, 7 * gib);
  CHECK_EQ(answer.memoryHeaps[1].size, 32 * gib);
  CHECK(!budget.admit(0u, 7 * gib + 1u));
  CHECK(budget.admit(0u, 7 * gib));
  CHECK(!budget.admit(0u, 1u));
  CHECK(budget.admit(1u, 31 * gib));
  CHECK_EQ(ledger.used(), 7 * gib); }

TEST(vram_budget, the_budget_report_follows_commits_and_releases)
{ av::FailingBlocks blocks;
  VramLedger ledger;
  ledger.learn(8 * gib);
  VramBudget budget(blocks);
  budget.bind(&ledger, nullptr);
  budget.learn(guest_view());
  CHECK(budget.admit(0u, 2 * gib));
  CHECK(budget.commit(0u, 0x10u, 2 * gib) == av::Status::ok);
  VkPhysicalDeviceMemoryBudgetPropertiesEXT report = {};
  budget.report(report);
  CHECK_EQ(report.heapBudget[0], 7 * gib);
  CHECK_EQ(report.heapUsage[0], 2 * gib);
  CHECK_EQ(report.heapBudget[2], std::uint64_t{0});
  CHECK(!budget.admit(0u, 5 * gib + 1u));
  budget.release(0x10u);
  budget.release(0x10u);
  CHECK(budget.admit(0u, 7 * gib));
  CHECK_EQ(budget.used(0u), std::uint64_t{0});
  CHECK(budget.invariants_hold()); }

TEST(vram_budget, usage_is_the_sessions_own_and_the_budget_shrinks_with_what_other_apis_hold)
{ av::FailingBlocks blocks;
  VramLedger ledger;
  ledger.learn(8 * gib);
  VramAccount other(ledger);
  CHECK(other.take(3 * gib));
  VramBudget budget(blocks);
  budget.bind(&ledger, nullptr);
  budget.learn(guest_view());
  VkPhysicalDeviceMemoryBudgetPropertiesEXT report = {};
  budget.report(report);
  CHECK_EQ(report.heapUsage[0], std::uint64_t{0});
  CHECK_EQ(report.heapBudget[0], 4 * gib);
  CHECK(!budget.admit(0u, 4 * gib + 1u));
  CHECK(budget.admit(0u, 4 * gib));
  ledger.reserve_vram(4096u);
  VkPhysicalDeviceMemoryProperties answer = guest_view();
  budget.clamp(answer);
  CHECK_EQ(answer.memoryHeaps[0].size, ledger.cap());
  CHECK(answer.memoryHeaps[0].size < 7 * gib); }

TEST(vram_budget, closing_returns_everything_the_backend_held)
{ av::FailingBlocks blocks;
  VramLedger ledger;
  ledger.learn(8 * gib);
  VramBudget budget(blocks);
  budget.bind(&ledger, nullptr);
  budget.learn(guest_view());
  CHECK(budget.admit(0u, 2 * gib));
  CHECK(budget.commit(0u, 0x10u, 2 * gib) == av::Status::ok);
  CHECK_EQ(budget.held(), 2 * gib);
  budget.close();
  CHECK_EQ(ledger.used(), std::uint64_t{0});
  CHECK_EQ(budget.held(), std::uint64_t{0}); }

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

TEST(vulkan_vram_cap, an_allocation_the_driver_refuses_is_given_back_to_the_ledger)
{ Rig rig;
  static_cast<void>(rig.properties());
  driver.allocate_result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
  VkDeviceMemory made = VK_NULL_HANDLE;
  CHECK_EQ(static_cast<int>(rig.allocate(rig.device_local_type(), gib, made)), static_cast<int>(VK_ERROR_OUT_OF_DEVICE_MEMORY));
  CHECK_EQ(rig.ledger().used(), std::uint64_t{0}); }

TEST(vulkan_vram_cap, a_refusal_is_logged_with_clean_probes_and_a_clean_retry_that_succeeds_is_never_used)
{ Rig rig;
  static_cast<void>(rig.properties());
  driver.refuse_next = 1u;
  VkDeviceMemory made = reinterpret_cast<VkDeviceMemory>(std::uintptr_t{1});
  CHECK_EQ(static_cast<int>(rig.allocate(rig.device_local_type(), gib, made)), static_cast<int>(VK_ERROR_OUT_OF_DEVICE_MEMORY));
  CHECK_EQ(rig.ledger().used(), std::uint64_t{0});
  CHECK_EQ(driver.allocations, 4u);
  CHECK_EQ(driver.frees, 3u);
  CHECK_EQ(rig.lines().holding("the host driver refused an allocation"), 1u);
  CHECK_EQ(rig.lines().holding("on stub gpu"), 1u);
  CHECK_EQ(rig.lines().holding("without the guest's chain is answered 0"), 1u);
  CHECK_EQ(rig.lines().holding("a clean 4096 byte allocation by type: 0=0 1=0"), 1u); }

TEST(vulkan_vram_cap, an_allocation_the_host_cannot_record_is_given_back_to_the_ledger)
{ Rig rig;
  static_cast<void>(rig.properties());
  rig.blocks().fail_from(rig.blocks().requests() + 1u);
  VkDeviceMemory made = VK_NULL_HANDLE;
  VkResult result = VK_SUCCESS;
  std::uint32_t kept = 0u;
  while (result == VK_SUCCESS && kept < 200u) {
      result = rig.allocate(rig.device_local_type(), 1u, made);
      if (result == VK_SUCCESS) kept++;
  }
  CHECK_EQ(static_cast<int>(result), static_cast<int>(VK_ERROR_OUT_OF_HOST_MEMORY));
  CHECK_EQ(rig.ledger().used(), std::uint64_t{kept}); }

TEST(vulkan_vram_cap, vulkan_and_another_api_on_the_same_gpu_share_one_cap)
{ Rig rig;
  static_cast<void>(rig.properties());
  VramAccount other(rig.ledger());
  CHECK(other.take(5 * gib));
  VkDeviceMemory made = VK_NULL_HANDLE;
  CHECK_EQ(static_cast<int>(rig.allocate(rig.device_local_type(), 3 * gib, made)), static_cast<int>(VK_ERROR_OUT_OF_DEVICE_MEMORY));
  CHECK_EQ(driver.allocations, 0u);
  CHECK_EQ(static_cast<int>(rig.allocate(rig.device_local_type(), 2 * gib, made)), static_cast<int>(VK_SUCCESS));
  CHECK_EQ(rig.ledger().used(), 7 * gib);
  CHECK(!other.take(1u));
  VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {};
  budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
  VkPhysicalDeviceMemoryProperties2 query = {};
  query.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
  query.pNext = &budget;
  rig.backend().on_vkGetPhysicalDeviceMemoryProperties2(rig.physical(), &query);
  CHECK_EQ(budget.heapUsage[0], 2 * gib);
  CHECK_EQ(budget.heapBudget[0], 2 * gib);
  other.give(5 * gib);
  rig.backend().on_vkGetPhysicalDeviceMemoryProperties2(rig.physical(), &query);
  CHECK_EQ(budget.heapUsage[0], 2 * gib);
  CHECK_EQ(budget.heapBudget[0], 7 * gib); }

TEST(vulkan_vram_cap, leaving_a_lost_device_gives_the_held_memory_back_without_shutting_down)
{ Rig rig;
  static_cast<void>(rig.properties());
  VkDeviceMemory made = VK_NULL_HANDLE;
  CHECK_EQ(static_cast<int>(rig.allocate(rig.device_local_type(), 3 * gib, made)), static_cast<int>(VK_SUCCESS));
  CHECK_EQ(rig.ledger().used(), 3 * gib);
  rig.backend().leave_device();
  CHECK_EQ(rig.ledger().used(), std::uint64_t{0}); }

TEST(vulkan_vram_cap, shutting_down_gives_the_held_memory_back)
{ Rig rig;
  static_cast<void>(rig.properties());
  VkDeviceMemory made = VK_NULL_HANDLE;
  CHECK_EQ(static_cast<int>(rig.allocate(rig.device_local_type(), 3 * gib, made)), static_cast<int>(VK_SUCCESS));
  CHECK_EQ(rig.ledger().used(), 3 * gib);
  rig.backend().shut_down();
  CHECK_EQ(rig.ledger().used(), std::uint64_t{0}); }

TEST(vram_budget, memory_in_a_heap_that_is_not_capped_never_touches_the_ledger)
{ av::FailingBlocks blocks;
  VramLedger ledger;
  ledger.learn(8 * gib);
  VramBudget budget(blocks);
  budget.bind(&ledger, nullptr);
  budget.learn(guest_view());
  CHECK(budget.admit(0u, gib));
  CHECK(budget.commit(0u, 0x10u, gib) == av::Status::ok);
  CHECK(budget.admit(1u, gib));
  CHECK(budget.commit(1u, 0x20u, gib) == av::Status::ok);
  CHECK_EQ(ledger.used(), gib);
  budget.release(0x20u);
  CHECK_EQ(ledger.used(), gib);
  budget.refund(1u, gib);
  CHECK_EQ(ledger.used(), gib);
  budget.release(0x10u);
  CHECK_EQ(ledger.used(), std::uint64_t{0}); }

namespace {

constexpr std::uint32_t vulkan_kill_steps = 10u;
constexpr std::uint32_t vulkan_kill_points = 20u;

void vulkan_step(Rig &rig, std::uint32_t index, VkDeviceMemory (&held)[5], std::uint32_t &released)
{
    const std::uint32_t type = rig.device_local_type();
    const auto make = [&](std::uint32_t slot, std::uint64_t size) {
        if (rig.allocate(type, size, held[slot]) != VK_SUCCESS) held[slot] = VK_NULL_HANDLE;
    };
    const auto give = [&](std::uint32_t slot) {
        if (held[slot] == VK_NULL_HANDLE) return;
        rig.backend().on_vkFreeMemory(rig.device(), held[slot], nullptr);
        held[slot] = VK_NULL_HANDLE;
        released++;
    };
    switch (index) {
    case 0u: make(0u, gib); break;
    case 1u: make(1u, 2 * gib); break;
    case 2u: give(0u); break;
    case 3u: make(2u, 3 * gib); break;
    case 4u: make(3u, 4 * gib); break;
    case 5u: give(1u); break;
    case 6u: make(3u, 4 * gib); break;
    case 7u: give(2u); break;
    case 8u: make(4u, gib); break;
    default: give(3u); break;
    }
}

}

TEST(vulkan_vram_cap, a_backend_killed_after_any_of_its_memory_calls_gives_back_every_byte_and_frees_nothing_twice)
{ std::uint32_t seed = 0x2545F491u;
  for (std::uint32_t point = 0u; point < vulkan_kill_points; point++) {
      seed = seed * 1664525u + 1013904223u;
      const std::uint32_t killed_after = (seed >> 16u) % (vulkan_kill_steps + 1u);
      Rig rig;
      static_cast<void>(rig.properties());
      VkDeviceMemory held[5] = {};
      std::uint32_t  released = 0u;
      for (std::uint32_t index = 0u; index < killed_after; index++) vulkan_step(rig, index, held, released);
      rig.backend().shut_down();
      CHECK_EQ(rig.ledger().used(), std::uint64_t{0});
      CHECK_EQ(driver.frees, released);
      rig.backend().shut_down();
      CHECK_EQ(rig.ledger().used(), std::uint64_t{0});
      CHECK_EQ(driver.frees, released);
  } }

TEST(vram_budget, memory_in_a_system_heap_is_taken_from_the_vms_host_ram_ledger_and_given_back_on_release)
{ av::FailingBlocks blocks;
  VramLedger ledger;
  HostRamLedger host;
  ledger.learn(8 * gib);
  host.budget(10 * gib);
  VramBudget budget(blocks);
  budget.bind(&ledger, &host);
  budget.learn(guest_view());
  CHECK(budget.system(1u));
  CHECK(!budget.system(0u));
  CHECK(budget.admit(1u, 6 * gib));
  CHECK(budget.commit(1u, 0x20u, 6 * gib) == av::Status::ok);
  CHECK(!budget.admit(1u, 5 * gib));
  CHECK_EQ(host.used(), 6 * gib);
  CHECK_EQ(ledger.used(), std::uint64_t{0});
  CHECK_EQ(budget.host_held(), 6 * gib);
  budget.release(0x20u);
  CHECK_EQ(host.used(), std::uint64_t{0});
  CHECK(budget.admit(1u, 10 * gib));
  CHECK(budget.invariants_hold()); }

TEST(vram_budget, a_refused_system_allocation_is_refunded_and_the_device_local_heap_does_not_touch_the_host_ledger)
{ av::FailingBlocks blocks;
  VramLedger ledger;
  HostRamLedger host;
  ledger.learn(8 * gib);
  host.budget(4 * gib);
  VramBudget budget(blocks);
  budget.bind(&ledger, &host);
  budget.learn(guest_view());
  CHECK(budget.admit(0u, 2 * gib));
  CHECK_EQ(host.used(), std::uint64_t{0});
  CHECK(budget.admit(1u, 3 * gib));
  budget.refund(1u, 3 * gib);
  CHECK_EQ(host.used(), std::uint64_t{0});
  budget.refund(0u, 2 * gib);
  CHECK_EQ(ledger.used(), std::uint64_t{0}); }

TEST(vram_budget, the_guest_is_told_the_host_ram_budget_for_system_heaps_and_what_the_vm_has_used)
{ av::FailingBlocks blocks;
  VramLedger ledger;
  HostRamLedger host;
  ledger.learn(8 * gib);
  host.budget(12 * gib);
  VramAccount other(host);
  CHECK(other.take(3 * gib));
  VramBudget budget(blocks);
  budget.bind(&ledger, &host);
  budget.learn(guest_view());
  VkPhysicalDeviceMemoryProperties answer = guest_view();
  budget.clamp(answer);
  CHECK_EQ(answer.memoryHeaps[0].size, 7 * gib);
  CHECK_EQ(answer.memoryHeaps[1].size, 12 * gib);
  VkPhysicalDeviceMemoryBudgetPropertiesEXT report = {};
  budget.report(report);
  CHECK_EQ(report.heapBudget[1], 12 * gib);
  CHECK_EQ(report.heapUsage[1], 3 * gib);
  CHECK_EQ(report.heapBudget[0], 7 * gib);
  host.budget(2 * gib);
  budget.report(report);
  CHECK_EQ(report.heapBudget[1], 3 * gib); }

TEST(vram_budget, closing_returns_the_host_ram_the_backend_held)
{ av::FailingBlocks blocks;
  VramLedger ledger;
  HostRamLedger host;
  ledger.learn(8 * gib);
  VramBudget budget(blocks);
  budget.bind(&ledger, &host);
  budget.learn(guest_view());
  CHECK(budget.admit(1u, 2 * gib));
  CHECK(budget.commit(1u, 0x10u, 2 * gib) == av::Status::ok);
  budget.close();
  CHECK_EQ(host.used(), std::uint64_t{0});
  CHECK_EQ(budget.host_held(), std::uint64_t{0}); }

TEST(vulkan_host_ram_cap, a_host_visible_allocation_past_the_budget_is_out_of_host_memory_and_never_reaches_the_driver)
{ HostRamLedger host;
  Rig rig;
  host.budget(5 * gib);
  rig.backend().use_vram(&rig.ledger(), &host);
  static_cast<void>(rig.properties());
  VkDeviceMemory made = VK_NULL_HANDLE;
  CHECK_EQ(static_cast<int>(rig.allocate(rig.host_visible_type(), 3 * gib, made)), static_cast<int>(VK_SUCCESS));
  VkDeviceMemory refused = VK_NULL_HANDLE;
  CHECK_EQ(static_cast<int>(rig.allocate(rig.host_visible_type(), 3 * gib, refused)),
           static_cast<int>(VK_ERROR_OUT_OF_HOST_MEMORY));
  CHECK_EQ(driver.allocations, 1u);
  CHECK_EQ(host.used(), 3 * gib);
  CHECK_EQ(host.refusals(), 1u);
  CHECK_EQ(static_cast<int>(rig.allocate(rig.host_visible_type(), 2 * gib, refused)), static_cast<int>(VK_SUCCESS)); }

TEST(vulkan_host_ram_cap, the_device_local_refusal_stays_out_of_device_memory_with_a_host_budget_in_force)
{ HostRamLedger host;
  Rig rig;
  host.budget(5 * gib);
  rig.backend().use_vram(&rig.ledger(), &host);
  static_cast<void>(rig.properties());
  VkDeviceMemory refused = VK_NULL_HANDLE;
  CHECK_EQ(static_cast<int>(rig.allocate(rig.device_local_type(), 8 * gib, refused)),
           static_cast<int>(VK_ERROR_OUT_OF_DEVICE_MEMORY));
  CHECK_EQ(host.used(), std::uint64_t{0}); }

TEST(vulkan_host_ram_cap, an_allocation_the_driver_refuses_gives_the_host_ram_back)
{ HostRamLedger host;
  Rig rig;
  host.budget(5 * gib);
  rig.backend().use_vram(&rig.ledger(), &host);
  static_cast<void>(rig.properties());
  driver.allocate_result = VK_ERROR_OUT_OF_HOST_MEMORY;
  VkDeviceMemory made = VK_NULL_HANDLE;
  CHECK_EQ(static_cast<int>(rig.allocate(rig.host_visible_type(), gib, made)), static_cast<int>(VK_ERROR_OUT_OF_HOST_MEMORY));
  CHECK_EQ(host.used(), std::uint64_t{0}); }

TEST(vulkan_host_ram_cap, shutting_down_gives_the_held_host_ram_back_and_the_guest_sees_the_heap_shrink)
{ HostRamLedger host;
  Rig rig;
  host.budget(5 * gib);
  rig.backend().use_vram(&rig.ledger(), &host);
  CHECK_EQ(rig.properties().memoryHeaps[1].size, 5 * gib);
  VkDeviceMemory made = VK_NULL_HANDLE;
  CHECK_EQ(static_cast<int>(rig.allocate(rig.host_visible_type(), 2 * gib, made)), static_cast<int>(VK_SUCCESS));
  VkPhysicalDeviceMemoryBudgetPropertiesEXT budget = {};
  budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
  VkPhysicalDeviceMemoryProperties2 query = {};
  query.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
  query.pNext = &budget;
  rig.backend().on_vkGetPhysicalDeviceMemoryProperties2(rig.physical(), &query);
  CHECK_EQ(budget.heapUsage[1], 2 * gib);
  CHECK_EQ(budget.heapBudget[1], 5 * gib);
  rig.backend().shut_down();
  CHECK_EQ(host.used(), std::uint64_t{0}); }

TEST(vram_budget, a_system_heap_allocation_is_refused_while_the_host_is_below_its_floor_and_the_device_local_heap_is_not)
{ av::FailingBlocks blocks;
  VramLedger ledger;
  HostRamLedger host;
  av::FakeMemoryGauge gauge;
  gauge.free_bytes = 5 * gib;
  ledger.learn(8 * gib);
  host.watch(&gauge);
  host.floor(4 * gib);
  VramBudget budget(blocks);
  budget.bind(&ledger, &host);
  budget.learn(guest_view());
  CHECK(budget.admit(0u, 2 * gib));
  CHECK(!budget.admit(1u, 2 * gib));
  CHECK_EQ(host.used(), std::uint64_t{0});
  gauge.set(20 * gib, HostRamLedger::refresh_ms);
  CHECK(budget.admit(1u, 2 * gib)); }
