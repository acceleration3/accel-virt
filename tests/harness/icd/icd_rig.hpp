#ifndef AV_ICD_RIG_HPP
#define AV_ICD_RIG_HPP

#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/icd/scripted_window_chains.hpp"
#include "harness/icd/scripted_window_surface.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/vulkan/icd_root.hpp"

#include <cstdint>

#include <vulkan/vulkan_core.h>

namespace av {

class IcdRig {
public:
    static constexpr std::uint64_t adapter_luid = 0x0000000200001234ull;

    IcdRig();
    IcdRig(const IcdRig &) = delete;
    IcdRig &operator=(const IcdRig &) = delete;

    MockPlatform         &platform();
    MockKmt              &kmt();
    ScriptedHost         &host();
    LockTrace            &trace();
    RecordingThreadLock  &lock();
    RecordingThreadLock  &send_lock();
    RecordingThreadLock  &memory_lock();
    RecordingLog         &log();
    ScriptedWindowSurface &windows();
    ScriptedWindowChains &chains();
    icd::IcdRoot         &root();

    template <typename Function>
    Function entry(VkInstance instance, const char *name)
    {
        return reinterpret_cast<Function>(root_.instance_proc_addr(instance, name));
    }

    VkInstance       create_instance();
    VkInstance       create_instance(const char *const *names, std::uint32_t count);
    VkPhysicalDevice physical_device(VkInstance instance);
    void             destroy_instance(VkInstance instance);

private:
    MockPlatform         platform_;
    MockKmt              kmt_;
    ScriptedHost         host_;
    LockTrace            trace_;
    RecordingThreadLock  lock_;
    RecordingThreadLock  send_lock_;
    RecordingThreadLock  memory_lock_;
    RecordingLog         log_;
    ScriptedWindowSurface windows_;
    ScriptedWindowChains  chains_;
    icd::IcdRoot         root_;
};

}

#endif
