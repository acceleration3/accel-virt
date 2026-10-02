/* Timing each guest submission on the host's own Vulkan: only when
 * AV_VK_SUBMIT_TIMING asks, only on the one queue it was told of, and a
 * submission it times is still the guest's - its fence signals as the guest
 * asked. A host with no Vulkan GPU returns after the part it can judge. */
#include "executor/vulkan/vulkan_submit_timing.hpp"
#include "harness/framework/test_registry.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

using av::VulkanSubmitTiming;

namespace {

/* An instance, the first physical device and a device with one queue of its
 * first graphics or compute family, or nothing. */
class HostQueue {
public:
    HostQueue() : instance_(VK_NULL_HANDLE), physical_(VK_NULL_HANDLE), device_(VK_NULL_HANDLE), queue_(VK_NULL_HANDLE),
                  family_(0u)
    {
        VkApplicationInfo application = {};
        application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application.apiVersion = VK_API_VERSION_1_1;
        VkInstanceCreateInfo instance = {};
        instance.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance.pApplicationInfo = &application;
        if (vkCreateInstance(&instance, nullptr, &instance_) != VK_SUCCESS) return;
        std::uint32_t count = 1u;
        if (vkEnumeratePhysicalDevices(instance_, &count, &physical_) < 0 || count == 0u) return;
        VkQueueFamilyProperties families[16] = {};
        std::uint32_t           listed = 16u;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_, &listed, families);
        for (family_ = 0u; family_ < listed; family_++)
            if ((families[family_].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) != 0u &&
                families[family_].timestampValidBits != 0u)
                break;
        if (family_ == listed) return;
        const float             priority = 1.0f;
        VkDeviceQueueCreateInfo queue = {};
        queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue.queueFamilyIndex = family_;
        queue.queueCount = 1u;
        queue.pQueuePriorities = &priority;
        VkDeviceCreateInfo device = {};
        device.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device.queueCreateInfoCount = 1u;
        device.pQueueCreateInfos = &queue;
        if (vkCreateDevice(physical_, &device, nullptr, &device_) != VK_SUCCESS) return;
        vkGetDeviceQueue(device_, family_, 0u, &queue_);
    }
    ~HostQueue()
    {
        if (device_ != VK_NULL_HANDLE) vkDestroyDevice(device_, nullptr);
        if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);
    }
    HostQueue(const HostQueue &) = delete;
    HostQueue &operator=(const HostQueue &) = delete;

    bool             made() const { return queue_ != VK_NULL_HANDLE; }
    VkPhysicalDevice physical() const { return physical_; }
    VkDevice         device() const { return device_; }
    VkQueue          queue() const { return queue_; }
    std::uint32_t    family() const { return family_; }

    /* An empty submission through `timing`, awaited by its fence. */
    bool submit_and_wait(VulkanSubmitTiming &timing, std::uint32_t count)
    {
        VkFenceCreateInfo made = {};
        made.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkFence fence = VK_NULL_HANDLE;
        if (vkCreateFence(device_, &made, nullptr, &fence) != VK_SUCCESS) return false;
        VkSubmitInfo submits[2] = {};
        for (VkSubmitInfo &submit : submits) submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        const bool ran = timing.submit(queue_, count, submits, fence) == VK_SUCCESS &&
                         vkWaitForFences(device_, 1u, &fence, VK_TRUE, 5000000000ull) == VK_SUCCESS;
        vkDestroyFence(device_, fence, nullptr);
        return ran;
    }

private:
    VkInstance       instance_;
    VkPhysicalDevice physical_;
    VkDevice         device_;
    VkQueue          queue_;
    std::uint32_t    family_;
};

/* What the timing writes to stderr while `body` runs. */
template <typename Body>
void capture_stderr(char *text, std::size_t capacity, Body body)
{
    std::fflush(stderr);
    const int saved = dup(2);
    const int file = memfd_create("stderr", 0);
    dup2(file, 2);
    body();
    std::fflush(stderr);
    dup2(saved, 2);
    close(saved);
    const ssize_t got = pread(file, text, capacity - 1u, 0);
    text[got > 0 ? got : 0] = '\0';
    close(file);
}

}  // namespace

TEST(vulkan_submit_timing, is_enabled_only_where_the_environment_asks)
{ unsetenv("AV_VK_SUBMIT_TIMING");
  VulkanSubmitTiming off;
  CHECK(!off.enabled());
  setenv("AV_VK_SUBMIT_TIMING", "1", 1);
  VulkanSubmitTiming on;
  CHECK(on.enabled());
  unsetenv("AV_VK_SUBMIT_TIMING");
  /* Nothing was made: forgetting any device is nothing. */
  on.forget(VK_NULL_HANDLE); }

TEST(vulkan_submit_timing_queue_made, a_disabled_timing_takes_no_queue_and_passes_submissions_through)
{ HostQueue host;
  if (!host.made()) return;
  unsetenv("AV_VK_SUBMIT_TIMING");
  VulkanSubmitTiming timing;
  char said[512] = {};
  capture_stderr(said, sizeof said, [&] { timing.queue_made(host.physical(), host.device(), host.family(), host.queue()); });
  CHECK(std::strstr(said, "av-vk-timing") == nullptr);
  CHECK(host.submit_and_wait(timing, 1u));
  timing.forget(host.device()); }

TEST(vulkan_submit_timing_queue_made, an_enabled_timing_takes_the_first_queue_it_is_told_of_only)
{ HostQueue host;
  if (!host.made()) return;
  setenv("AV_VK_SUBMIT_TIMING", "1", 1);
  VulkanSubmitTiming timing;
  unsetenv("AV_VK_SUBMIT_TIMING");
  char said[512] = {};
  capture_stderr(said, sizeof said, [&] {
      timing.queue_made(host.physical(), host.device(), host.family(), VK_NULL_HANDLE);   /* no queue: ignored */
      timing.queue_made(host.physical(), host.device(), host.family(), host.queue());
      timing.queue_made(host.physical(), host.device(), host.family(), host.queue());   /* one queue is timed */
  });
  char expected[64];
  std::snprintf(expected, sizeof expected, "av-vk-timing: timing queue family %u\n", host.family());
  CHECK(std::strcmp(said, expected) == 0);
  /* Timed submissions - one, two, and none at all - still signal the guest's fence. */
  for (std::uint32_t round = 0u; round < 70u; round++) CHECK(host.submit_and_wait(timing, round % 3u));
  /* Forgetting another device keeps this one's timing; forgetting this one
   * lets it go, twice harmlessly, and submissions pass straight through. */
  timing.forget(VK_NULL_HANDLE);
  CHECK(host.submit_and_wait(timing, 1u));
  timing.forget(host.device());
  timing.forget(host.device());
  CHECK(host.submit_and_wait(timing, 1u)); }
