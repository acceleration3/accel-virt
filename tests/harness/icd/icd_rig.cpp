#include "harness/icd/icd_rig.hpp"

#include "guest/suite/adapter_scenario.hpp"
#include "icd/vulkan/icd_binding.hpp"

namespace av {

namespace {

const PciIdentity rvkgpu(AdapterScenario::virtio_vendor, AdapterScenario::virtio_gpu_device,
                         AdapterScenario::virtio_vendor, AdapterScenario::rvk_subsystem);

}  // namespace

IcdRig::IcdRig()
    : platform_(), kmt_(), host_(kmt_), luids_(), trace_(), lock_(trace_, 'H'), send_lock_(trace_, 'S'), log_(),
      root_(platform_, kmt_, luids_, lock_, send_lock_, log_, icd::icd_binding)
{
    kmt_.add_adapter(rvkgpu, AdapterScenario::wddm_3_2);
}

MockPlatform         &IcdRig::platform() { return platform_; }
MockKmt              &IcdRig::kmt() { return kmt_; }
ScriptedHost         &IcdRig::host() { return host_; }
ScriptedAdapterLuids &IcdRig::luids() { return luids_; }
LockTrace            &IcdRig::trace() { return trace_; }
RecordingThreadLock  &IcdRig::lock() { return lock_; }
RecordingThreadLock  &IcdRig::send_lock() { return send_lock_; }
RecordingLog         &IcdRig::log() { return log_; }
icd::IcdRoot         &IcdRig::root() { return root_; }

VkInstance IcdRig::create_instance()
{
    const auto create = entry<VkResult (*)(const VkInstanceCreateInfo *, const VkAllocationCallbacks *,
                                           VkInstance *)>(VK_NULL_HANDLE, "vkCreateInstance");
    VkInstanceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    VkInstance instance = VK_NULL_HANDLE;
    host_.vk_handle(0, ScriptedHost::vk_instance);
    return create(&info, nullptr, &instance) == VK_SUCCESS ? instance : VK_NULL_HANDLE;
}

VkPhysicalDevice IcdRig::physical_device(VkInstance instance)
{
    const auto enumerate = entry<VkResult (*)(VkInstance, std::uint64_t, std::uint32_t *, VkPhysicalDevice *)>(
        VK_NULL_HANDLE, "vk_icdEnumerateAdapterPhysicalDevices");
    std::uint32_t    count = 1u;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    host_.vk_physical_devices(0, 1u);
    const VkResult result = enumerate(instance, luids_.default_luid, &count, &physical_device);
    return result == VK_SUCCESS && count == 1u ? physical_device : VK_NULL_HANDLE;
}

void IcdRig::destroy_instance(VkInstance instance)
{
    const auto destroy = entry<void (*)(VkInstance, const VkAllocationCallbacks *)>(instance, "vkDestroyInstance");
    destroy(instance, nullptr);
}

}  // namespace av
