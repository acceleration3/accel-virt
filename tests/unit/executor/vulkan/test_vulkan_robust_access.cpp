#include "core/log/driver_log.hpp"
#include "executor/vulkan/vulkan_backend.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>

namespace {

struct DeviceDriver {
    VkBool32      offers_robust = VK_TRUE;
    std::uint32_t creates = 0u;
    VkBool32      core_robust = VK_FALSE;
    bool          core_seen = false;
    VkBool32      features2_robust = VK_FALSE;
    VkBool32      image_robust = VK_FALSE;
    VkBool32      vulkan13_robust = VK_FALSE;
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

VkDeviceCreateInfo plain_device()
{ VkDeviceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  return info; }

}

extern "C" {

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures(VkPhysicalDevice, VkPhysicalDeviceFeatures *features)
{
    *features = {};
    features->robustBufferAccess = driver.offers_robust;
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties(VkPhysicalDevice, const char *, uint32_t *count,
                                                                    VkExtensionProperties *)
{
    *count = 0u;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice, const VkDeviceCreateInfo *info,
                                              const VkAllocationCallbacks *, VkDevice *device)
{
    driver.creates++;
    if (info->pEnabledFeatures != nullptr) {
        driver.core_seen = true;
        driver.core_robust = info->pEnabledFeatures->robustBufferAccess;
    }
    for (auto *link = static_cast<const VkBaseInStructure *>(info->pNext); link != nullptr; link = link->pNext) {
        if (link->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2)
            driver.features2_robust = reinterpret_cast<const VkPhysicalDeviceFeatures2 *>(link)->features.robustBufferAccess;
        if (link->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES)
            driver.image_robust = reinterpret_cast<const VkPhysicalDeviceImageRobustnessFeatures *>(link)->robustImageAccess;
        if (link->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES)
            driver.vulkan13_robust = reinterpret_cast<const VkPhysicalDeviceVulkan13Features *>(link)->robustImageAccess;
    }
    *device = reinterpret_cast<VkDevice>(std::uintptr_t{0x300});
    return VK_SUCCESS;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance, const char *) { return nullptr; }
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice, const char *) { return nullptr; }
VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties *p) { *p = {}; }
VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(VkDevice, const VkAllocationCallbacks *) {}

}

TEST(vulkan_robust_access, a_device_with_no_features_gets_robust_buffer_access)
{ Rig rig;
  CHECK_EQ(static_cast<int>(rig.create(plain_device())), static_cast<int>(VK_SUCCESS));
  CHECK(driver.core_seen);
  CHECK_EQ(driver.core_robust, VK_TRUE); }

TEST(vulkan_robust_access, the_guest_s_core_features_are_kept_and_robustness_is_added)
{ VkPhysicalDeviceFeatures chosen = {};
  chosen.shaderInt64 = VK_TRUE;
  VkDeviceCreateInfo info = plain_device();
  info.pEnabledFeatures = &chosen;
  Rig rig;
  CHECK_EQ(static_cast<int>(rig.create(info)), static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.core_robust, VK_TRUE);
  CHECK_EQ(chosen.robustBufferAccess, VK_FALSE); }

TEST(vulkan_robust_access, features2_in_the_chain_is_made_robust_in_place)
{ VkPhysicalDeviceFeatures2 features = {};
  features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  VkDeviceCreateInfo info = plain_device();
  info.pNext = &features;
  Rig rig;
  CHECK_EQ(static_cast<int>(rig.create(info)), static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.features2_robust, VK_TRUE);
  CHECK(!driver.core_seen); }

TEST(vulkan_robust_access, image_robustness_the_guest_chained_is_switched_on)
{ VkPhysicalDeviceImageRobustnessFeatures image = {};
  image.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES;
  VkPhysicalDeviceVulkan13Features v13 = {};
  v13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  image.pNext = &v13;
  VkDeviceCreateInfo info = plain_device();
  info.pNext = &image;
  Rig rig;
  CHECK_EQ(static_cast<int>(rig.create(info)), static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.image_robust, VK_TRUE);
  CHECK_EQ(driver.vulkan13_robust, VK_TRUE); }

TEST(vulkan_robust_access, a_host_without_robust_buffer_access_gets_the_device_unchanged)
{ Rig rig;
  driver.offers_robust = VK_FALSE;
  CHECK_EQ(static_cast<int>(rig.create(plain_device())), static_cast<int>(VK_SUCCESS));
  CHECK(!driver.core_seen);
  CHECK_EQ(driver.creates, 1u); }
