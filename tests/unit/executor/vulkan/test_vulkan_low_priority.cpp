#include "core/log/driver_log.hpp"
#include "executor/vulkan/low_priority_queues.hpp"
#include "executor/vulkan/vulkan_backend.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstring>

namespace {

struct DeviceDriver {
    const char   *offered[4] = {};
    std::uint32_t offered_count = 0u;
    std::uint32_t creates = 0u;
    VkResult      results[4] = {VK_SUCCESS, VK_SUCCESS, VK_SUCCESS, VK_SUCCESS};
    bool          chained[4] = {};
    VkQueueGlobalPriorityKHR priority[4] = {};
    bool          prioritised_extension[4] = {};
    std::uint32_t extensions_seen[4] = {};
};

DeviceDriver driver;

class Rig {
public:
    Rig() : sink_(), lines_(sink_, "vulkan: "), backend_(lines_) { driver = DeviceDriver{}; }
    ~Rig() { backend_.shut_down(); }

    VkResult create(const VkDeviceCreateInfo &info)
    { VkDevice made = VK_NULL_HANDLE;
      return backend_.on_vkCreateDevice(reinterpret_cast<VkPhysicalDevice>(std::uintptr_t{0x200}), &info, nullptr, &made); }

private:
    av::RecordingLog  sink_;
    av::DriverLog     lines_;
    av::VulkanBackend backend_;
};

VkDeviceCreateInfo device_with(const VkDeviceQueueCreateInfo *queues, std::uint32_t count)
{ VkDeviceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  info.queueCreateInfoCount = count;
  info.pQueueCreateInfos = queues;
  return info; }

}

extern "C" {

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures(VkPhysicalDevice, VkPhysicalDeviceFeatures *features)
{
    *features = {};
    features->robustBufferAccess = VK_TRUE;
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties(VkPhysicalDevice, const char *, uint32_t *count,
                                                                    VkExtensionProperties *properties)
{
    if (properties == nullptr) {
        *count = driver.offered_count;
        return VK_SUCCESS;
    }
    for (std::uint32_t at = 0u; at < *count && at < driver.offered_count; at++) {
        properties[at] = {};
        std::strncpy(properties[at].extensionName, driver.offered[at], VK_MAX_EXTENSION_NAME_SIZE - 1u);
    }
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice, const VkDeviceCreateInfo *info,
                                              const VkAllocationCallbacks *, VkDevice *device)
{
    const std::uint32_t call = driver.creates < 4u ? driver.creates : 3u;
    driver.creates++;
    driver.extensions_seen[call] = info->enabledExtensionCount;
    for (std::uint32_t at = 0u; at < info->enabledExtensionCount; at++)
        if (std::strcmp(info->ppEnabledExtensionNames[at], VK_KHR_GLOBAL_PRIORITY_EXTENSION_NAME) == 0)
            driver.prioritised_extension[call] = true;
    for (auto *link = static_cast<const VkBaseInStructure *>(info->pQueueCreateInfos[0].pNext); link != nullptr;
         link = link->pNext) {
        if (link->sType != VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO) continue;
        driver.chained[call] = true;
        driver.priority[call] = reinterpret_cast<const VkDeviceQueueGlobalPriorityCreateInfoKHR *>(link)->globalPriority;
    }
    *device = reinterpret_cast<VkDevice>(std::uintptr_t{0x300});
    return driver.results[call];
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance, const char *) { return nullptr; }
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice, const char *) { return nullptr; }
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties *p) { *p = {}; }
VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(VkDevice, const VkAllocationCallbacks *) {}

}

TEST(vulkan_low_priority, a_host_that_offers_global_priority_gets_every_queue_at_the_lowest_priority)
{ Rig rig;
  driver.offered[0] = VK_KHR_GLOBAL_PRIORITY_EXTENSION_NAME;
  driver.offered_count = 1u;
  VkDeviceQueueCreateInfo queues[2] = {};
  queues[0].sType = queues[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  CHECK_EQ(static_cast<int>(rig.create(device_with(queues, 2u))), static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.creates, 1u);
  CHECK(driver.chained[0]);
  CHECK_EQ(static_cast<int>(driver.priority[0]), static_cast<int>(VK_QUEUE_GLOBAL_PRIORITY_LOW));
  CHECK(driver.prioritised_extension[0]); }

TEST(vulkan_low_priority, a_host_without_it_gets_the_guest_s_device_unchanged)
{ Rig rig;
  VkDeviceQueueCreateInfo queue = {};
  queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  CHECK_EQ(static_cast<int>(rig.create(device_with(&queue, 1u))), static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.creates, 1u);
  CHECK(!driver.chained[0]);
  CHECK(!driver.prioritised_extension[0]); }

TEST(vulkan_low_priority, a_refused_priority_falls_back_to_the_guest_s_device_so_it_is_never_lost)
{ Rig rig;
  driver.offered[0] = VK_KHR_GLOBAL_PRIORITY_EXTENSION_NAME;
  driver.offered_count = 1u;
  driver.results[0] = VK_ERROR_NOT_PERMITTED_KHR;
  VkDeviceQueueCreateInfo queue = {};
  queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  CHECK_EQ(static_cast<int>(rig.create(device_with(&queue, 1u))), static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.creates, 2u);
  CHECK(driver.chained[0]);
  CHECK(!driver.chained[1]);
  CHECK(!driver.prioritised_extension[1]); }

TEST(vulkan_low_priority, a_priority_the_guest_chose_itself_is_left_alone)
{ Rig rig;
  driver.offered[0] = VK_KHR_GLOBAL_PRIORITY_EXTENSION_NAME;
  driver.offered_count = 1u;
  VkDeviceQueueGlobalPriorityCreateInfoKHR chosen = {};
  chosen.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO;
  chosen.globalPriority = VK_QUEUE_GLOBAL_PRIORITY_MEDIUM;
  VkDeviceQueueCreateInfo queue = {};
  queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue.pNext = &chosen;
  CHECK_EQ(static_cast<int>(rig.create(device_with(&queue, 1u))), static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.creates, 1u);
  CHECK_EQ(static_cast<int>(driver.priority[0]), static_cast<int>(VK_QUEUE_GLOBAL_PRIORITY_MEDIUM)); }

TEST(vulkan_low_priority, more_queue_families_than_the_table_holds_leave_the_guest_s_device_unchanged)
{ Rig rig;
  driver.offered[0] = VK_KHR_GLOBAL_PRIORITY_EXTENSION_NAME;
  driver.offered_count = 1u;
  VkDeviceQueueCreateInfo queues[av::LowPriorityQueues::most_queue_infos + 1u] = {};
  for (auto &queue : queues) queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  CHECK_EQ(static_cast<int>(rig.create(device_with(queues, av::LowPriorityQueues::most_queue_infos + 1u))),
           static_cast<int>(VK_SUCCESS));
  CHECK(!driver.chained[0]); }

TEST(vulkan_low_priority, the_ext_spelling_is_used_when_only_it_is_offered)
{ av::LowPriorityQueues queues;
  driver = DeviceDriver{};
  driver.offered[0] = VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME;
  driver.offered_count = 1u;
  CHECK(queues.offered_by(reinterpret_cast<VkPhysicalDevice>(std::uintptr_t{0x200})));
  CHECK(std::strcmp(queues.extension(), VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME) == 0);
  CHECK(queues.invariants_hold()); }
