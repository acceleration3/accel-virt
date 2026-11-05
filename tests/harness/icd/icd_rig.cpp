#include "harness/icd/icd_rig.hpp"

#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "icd/vulkan/icd_binding.hpp"

namespace av {

namespace {

const PciIdentity avgpu(PciIdentity::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                         PciIdentity::virtio_vendor, AvgpuAdapters::av_subsystem);

}

IcdRig::IcdRig()
    : platform_(), kmt_(), host_(kmt_), trace_(), lock_(trace_, 'H'), send_lock_(trace_, 'S'), memory_lock_(trace_, 'M'),
      log_(), lines_(platform_, log_, "avvk: "), windows_(), chains_(), present_lock_(trace_, 'P'), present_thread_(),
      handed_(), shown_(),
      root_(platform_, kmt_, lock_, send_lock_, memory_lock_, lines_, icd::icd_binding, windows_, chains_,
            present_thread_, handed_, shown_, present_lock_)
{
    kmt_.add_adapter(avgpu, AdapterScenario::wddm_3_2);
    kmt_.open_host_window(MockKmt::rig_window_bytes);
    kmt_.set_adapter_luid(0u, adapter_luid);
}

MockPlatform         &IcdRig::platform() { return platform_; }
MockKmt              &IcdRig::kmt() { return kmt_; }
ScriptedHost         &IcdRig::host() { return host_; }
LockTrace            &IcdRig::trace() { return trace_; }
RecordingThreadLock  &IcdRig::lock() { return lock_; }
RecordingThreadLock  &IcdRig::send_lock() { return send_lock_; }
RecordingThreadLock  &IcdRig::memory_lock() { return memory_lock_; }
RecordingLog         &IcdRig::log() { return log_; }
KeyedLog             &IcdRig::lines() { return lines_; }
ScriptedThread       &IcdRig::present_thread() { return present_thread_; }
CountedWakeSignal    &IcdRig::handed() { return handed_; }
CountedWakeSignal    &IcdRig::shown() { return shown_; }
ScriptedWindowSurface &IcdRig::windows() { return windows_; }
ScriptedWindowChains &IcdRig::chains() { return chains_; }
icd::IcdRoot         &IcdRig::root() { return root_; }

VkInstance IcdRig::create_instance() { return create_instance(nullptr, 0u); }

VkInstance IcdRig::create_instance(const char *const *names, std::uint32_t count)
{
    const auto create = entry<VkResult (*)(const VkInstanceCreateInfo *, const VkAllocationCallbacks *,
                                           VkInstance *)>(VK_NULL_HANDLE, "vkCreateInstance");
    VkInstanceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.enabledExtensionCount = count;
    info.ppEnabledExtensionNames = names;
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
    const VkResult result = enumerate(instance, adapter_luid, &count, &physical_device);
    return result == VK_SUCCESS && count == 1u ? physical_device : VK_NULL_HANDLE;
}

void IcdRig::destroy_instance(VkInstance instance)
{
    const auto destroy = entry<void (*)(VkInstance, const VkAllocationCallbacks *)>(instance, "vkDestroyInstance");
    destroy(instance, nullptr);
}

}
