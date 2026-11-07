#include "executor/shim/borrowed_queue.hpp"
#include "executor/shim/queue_work.hpp"
#include "executor/shim/vulkan_device_table.hpp"
#include "harness/framework/test_registry.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstring>

namespace {

struct FakeDriver {
    std::uint32_t submits = 0u;
    std::uint64_t submitted_value = 0u;
    bool          locked_at_submit = false;
    std::uint32_t waits = 0u;
    std::uint64_t waited_value = 0u;
    std::uint64_t waited_nanoseconds = 0u;
    bool          locked_at_wait = false;
    std::uint32_t idles = 0u;
    std::uint32_t destroyed_semaphores = 0u;
    bool          wait_times_out = false;
};

FakeDriver driver;

class FakeQueue final : public av::BorrowedQueue {
public:
    VkQueue  queue() const override { return reinterpret_cast<VkQueue>(std::uintptr_t{0x10}); }
    uint32_t family() const override { return 0u; }
    void     flush() override {}
    void     lock() override { held = true; }
    void     unlock() override { held = false; }

    bool held = false;
};

FakeQueue *current = nullptr;

constexpr std::uintptr_t device_number = 0x20u;

class Rig {
public:
    Rig()
    {
        driver = FakeDriver{};
        current = &queue_;
        CHECK(table_.load(reinterpret_cast<VkInstance>(std::uintptr_t{0x30}),
                          reinterpret_cast<VkPhysicalDevice>(std::uintptr_t{0x40}),
                          reinterpret_cast<VkDevice>(device_number)));
        CHECK(work_.open(table_, queue_));
    }
    ~Rig()
    {
        work_.close();
        current = nullptr;
    }

    av::QueueWork &work() { return work_; }

private:
    FakeQueue            queue_;
    av::VulkanDeviceTable table_;
    av::QueueWork        work_;
};

}

extern "C" {

VKAPI_ATTR void VKAPI_CALL fake_properties(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties *properties)
{ *properties = VkPhysicalDeviceMemoryProperties{}; }

VKAPI_ATTR VkResult VKAPI_CALL fake_create_semaphore(VkDevice, const VkSemaphoreCreateInfo *, const VkAllocationCallbacks *,
                                                      VkSemaphore *out)
{ *out = reinterpret_cast<VkSemaphore>(std::uintptr_t{0x50});
  return VK_SUCCESS; }

VKAPI_ATTR void VKAPI_CALL fake_destroy_semaphore(VkDevice, VkSemaphore, const VkAllocationCallbacks *)
{ driver.destroyed_semaphores++; }

VKAPI_ATTR VkResult VKAPI_CALL fake_wait_semaphores(VkDevice, const VkSemaphoreWaitInfo *info, uint64_t nanoseconds)
{ driver.waits++;
  driver.waited_value = info->pValues[0];
  driver.waited_nanoseconds = nanoseconds;
  driver.locked_at_wait = current->held;
  return driver.wait_times_out ? VK_TIMEOUT : VK_SUCCESS; }

VKAPI_ATTR VkResult VKAPI_CALL fake_submit(VkQueue, uint32_t, const VkSubmitInfo *submits, VkFence)
{ driver.submits++;
  const auto *values = static_cast<const VkTimelineSemaphoreSubmitInfo *>(submits[0].pNext);
  driver.submitted_value = values->pSignalSemaphoreValues[0];
  driver.locked_at_submit = current->held;
  return VK_SUCCESS; }

VKAPI_ATTR VkResult VKAPI_CALL fake_idle(VkQueue)
{ driver.idles++;
  return VK_SUCCESS; }

VKAPI_ATTR VkResult VKAPI_CALL fake_create_pool(VkDevice, const VkCommandPoolCreateInfo *, const VkAllocationCallbacks *,
                                                VkCommandPool *out)
{ *out = reinterpret_cast<VkCommandPool>(std::uintptr_t{0x60});
  return VK_SUCCESS; }

VKAPI_ATTR void VKAPI_CALL fake_destroy_pool(VkDevice, VkCommandPool, const VkAllocationCallbacks *) {}

VKAPI_ATTR VkResult VKAPI_CALL fake_allocate(VkDevice, const VkCommandBufferAllocateInfo *info, VkCommandBuffer *out)
{ for (uint32_t at = 0u; at < info->commandBufferCount; at++)
      out[at] = reinterpret_cast<VkCommandBuffer>(std::uintptr_t{0x70} + at);
  return VK_SUCCESS; }

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance, const char *name)
{ if (std::strcmp(name, "vkGetPhysicalDeviceMemoryProperties") == 0)
      return reinterpret_cast<PFN_vkVoidFunction>(&fake_properties);
  return nullptr; }

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice, const char *name)
{ struct Entry { const char *name; PFN_vkVoidFunction function; };
  const Entry entries[] = {
      {"vkCreateSemaphore", reinterpret_cast<PFN_vkVoidFunction>(&fake_create_semaphore)},
      {"vkDestroySemaphore", reinterpret_cast<PFN_vkVoidFunction>(&fake_destroy_semaphore)},
      {"vkWaitSemaphores", reinterpret_cast<PFN_vkVoidFunction>(&fake_wait_semaphores)},
      {"vkQueueSubmit", reinterpret_cast<PFN_vkVoidFunction>(&fake_submit)},
      {"vkQueueWaitIdle", reinterpret_cast<PFN_vkVoidFunction>(&fake_idle)},
      {"vkCreateCommandPool", reinterpret_cast<PFN_vkVoidFunction>(&fake_create_pool)},
      {"vkDestroyCommandPool", reinterpret_cast<PFN_vkVoidFunction>(&fake_destroy_pool)},
      {"vkAllocateCommandBuffers", reinterpret_cast<PFN_vkVoidFunction>(&fake_allocate)},
  };
  for (const Entry &entry : entries)
      if (std::strcmp(entry.name, name) == 0) return entry.function;
  return nullptr; }

}

TEST(queue_work_idle, marking_the_queue_idle_signals_a_timeline_under_the_queue_lock_and_never_idles_the_queue)
{ Rig rig;
  std::uint64_t ticket = 0u;
  CHECK(rig.work().mark_idle(ticket));
  CHECK_EQ(ticket, 1u);
  CHECK_EQ(driver.submits, 1u);
  CHECK_EQ(driver.submitted_value, 1u);
  CHECK(driver.locked_at_submit);
  CHECK(!current->held);
  CHECK_EQ(driver.idles, 0u);
  CHECK(rig.work().mark_idle(ticket));
  CHECK_EQ(ticket, 2u);
  CHECK_EQ(driver.submitted_value, 2u); }

TEST(queue_work_idle, waiting_for_a_ticket_is_bounded_and_happens_with_the_queue_unlocked)
{ Rig rig;
  std::uint64_t ticket = 0u;
  CHECK(rig.work().mark_idle(ticket));
  CHECK(rig.work().reached(ticket, 123456u));
  CHECK_EQ(driver.waits, 1u);
  CHECK_EQ(driver.waited_value, ticket);
  CHECK_EQ(driver.waited_nanoseconds, 123456u);
  CHECK(!driver.locked_at_wait);
  driver.wait_times_out = true;
  CHECK(!rig.work().reached(ticket, 1u));
  CHECK_EQ(driver.idles, 0u); }

TEST(queue_work_idle, a_queue_that_was_never_opened_has_nothing_to_wait_for)
{ av::QueueWork work;
  std::uint64_t ticket = 7u;
  CHECK(work.mark_idle(ticket));
  CHECK_EQ(ticket, 0u);
  CHECK(work.reached(ticket, 1u)); }

TEST(queue_work_idle, closing_the_queue_releases_the_marker_timeline)
{ Rig rig;
  std::uint64_t ticket = 0u;
  CHECK(rig.work().mark_idle(ticket));
  CHECK_EQ(driver.destroyed_semaphores, 0u);
  rig.work().close();
  CHECK_EQ(driver.destroyed_semaphores, 1u); }
