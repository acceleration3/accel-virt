#ifndef AV_BOUND_ICD_ENTRY_HPP
#define AV_BOUND_ICD_ENTRY_HPP

#include <vulkan/vulkan_core.h>

namespace av {

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL bound_icd_instance_proc_addr(VkInstance instance, const char *name);
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL bound_icd_device_proc_addr(VkDevice device, const char *name);

}

#endif
