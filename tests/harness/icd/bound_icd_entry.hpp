/* vk_icdGetInstanceProcAddr as a test process reaches it: the bound IcdRoot's
 * answer, as rvkvk.dll's export answers the Windows loader. A bare function
 * pointer, since that is what a loader entry point is; one line into the
 * binding, like the ICD's own local entries. */
#ifndef AV_BOUND_ICD_ENTRY_HPP
#define AV_BOUND_ICD_ENTRY_HPP

#include <vulkan/vulkan_core.h>

namespace av {

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL bound_icd_instance_proc_addr(VkInstance instance, const char *name);

}  // namespace av

#endif  // AV_BOUND_ICD_ENTRY_HPP
