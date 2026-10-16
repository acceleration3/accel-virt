#include "executor/shim/physical_device_list.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/host_gpu.hpp"
#include "executor/shim/physical_device_identity.hpp"

namespace av {

PhysicalDeviceList::PhysicalDeviceList() : listed_{}
{
    AV_SCOPE("PhysicalDeviceList::PhysicalDeviceList");
}

VkPhysicalDevice PhysicalDeviceList::find(VkInstance instance, const HostGpu &gpu)
{
    AV_SCOPE("PhysicalDeviceList::find");
    uint32_t found = max_devices;
    /* VK_INCOMPLETE still lists the first sixteen, and a GPU past them is one
     * this host does not have. */
    if (vkEnumeratePhysicalDevices(instance, &found, listed_) < 0) return VK_NULL_HANDLE;
    if (found > max_devices) found = max_devices;
    for (uint32_t i = 0u; i < found; i++)
        if (gpu.matches(PhysicalDeviceIdentity(instance, listed_[i]).uuid().raw())) return listed_[i];
    return VK_NULL_HANDLE;
}

}  // namespace av
