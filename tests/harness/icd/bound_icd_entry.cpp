#include "harness/icd/bound_icd_entry.hpp"

#include "icd/vulkan/icd_binding.hpp"
#include "icd/vulkan/icd_root.hpp"

namespace av {

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL bound_icd_instance_proc_addr(VkInstance instance, const char *name)
{ return icd::icd_binding.root().instance_proc_addr(instance, name); }

}  // namespace av
