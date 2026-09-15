#include "icd/vulkan/local_entries.hpp"

#include "icd/vulkan/icd_binding.hpp"
#include "icd/vulkan/icd_root.hpp"

namespace av::icd {

VKAPI_ATTR VkResult VKAPI_CALL local_vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t *pSupportedVersion)
{ return icd_binding.root().negotiate(pSupportedVersion); }

VKAPI_ATTR VkResult VKAPI_CALL local_vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                                                      const VkAllocationCallbacks *pAllocator, VkInstance *pInstance)
{ return icd_binding.root().create_instance(pCreateInfo, pAllocator, pInstance); }

VKAPI_ATTR VkResult VKAPI_CALL local_vkEnumerateInstanceExtensionProperties(const char *pLayerName,
                                                                            uint32_t *pPropertyCount,
                                                                            VkExtensionProperties *pProperties)
{ return icd_binding.root().enumerate_instance_extensions(pLayerName, pPropertyCount, pProperties); }

VKAPI_ATTR VkResult VKAPI_CALL local_vkEnumerateInstanceVersion(uint32_t *pApiVersion)
{ return icd_binding.root().enumerate_instance_version(pApiVersion); }

/* The loader asks this only for a physical-device command it does not know
 * itself, to learn whether to dispatch it by physical device. Whether a
 * generated wrapper is one is not something the 063 table records, and naming a
 * device-level function here would have the loader dispatch it on the wrong
 * object, so the honest answer is none (integration point: an emitter change). */
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL local_vk_icdGetPhysicalDeviceProcAddr(VkInstance, const char *)
{ return nullptr; }

VKAPI_ATTR VkResult VKAPI_CALL local_vk_icdEnumerateAdapterPhysicalDevices(VkInstance instance, uint64_t adapterLUID,
                                                                           uint32_t *pPhysicalDeviceCount,
                                                                           VkPhysicalDevice *pPhysicalDevices)
{ return icd_binding.root().enumerate_adapter_physical_devices(instance, adapterLUID, pPhysicalDeviceCount,
                                                                pPhysicalDevices); }

VKAPI_ATTR void VKAPI_CALL local_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator)
{ icd_binding.root().destroy_device(device, pAllocator); }

VKAPI_ATTR void VKAPI_CALL local_vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice,
                                                               VkPhysicalDeviceProperties *pProperties)
{ icd_binding.root().physical_device_properties(physicalDevice, pProperties); }

VKAPI_ATTR void VKAPI_CALL local_vkGetPhysicalDeviceProperties2(VkPhysicalDevice physicalDevice,
                                                                VkPhysicalDeviceProperties2 *pProperties)
{ icd_binding.root().physical_device_properties2(physicalDevice, pProperties); }

}  // namespace av::icd
