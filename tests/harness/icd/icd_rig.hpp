/* An IcdRoot as the loader finds it, over MockKmt with the rvkgpu adapter
 * present: the test plays the host through ScriptedHost, the root's two locks
 * record their order in one trace, and its log is kept. The root binds itself
 * for the rig's life, so the generated wrappers' hooks reach it. The rig reaches
 * the ICD only through the entry points the root hands out, as the loader does. */
#ifndef AV_ICD_RIG_HPP
#define AV_ICD_RIG_HPP

#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/icd/scripted_adapter_luids.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/vulkan/icd_root.hpp"

#include <cstdint>

#include <vulkan/vulkan_core.h>

namespace av {

class IcdRig {
public:
    IcdRig();
    IcdRig(const IcdRig &) = delete;
    IcdRig &operator=(const IcdRig &) = delete;

    MockPlatform         &platform();
    MockKmt              &kmt();
    ScriptedHost         &host();
    ScriptedAdapterLuids &luids();
    LockTrace            &trace();
    RecordingThreadLock  &lock();
    RecordingThreadLock  &send_lock();
    RecordingLog         &log();
    icd::IcdRoot         &root();

    /* The entry point the root answers for `name` through `instance`, as the
     * loader would call it; null when it answers none. */
    template <typename Function>
    Function entry(VkInstance instance, const char *name)
    {
        return reinterpret_cast<Function>(root_.instance_proc_addr(instance, name));
    }

    /* vkCreateInstance through the root's table, the host answering the handle
     * ScriptedHost names; null when it failed. */
    VkInstance       create_instance();
    /* The instance's one physical device, asked for by the adapter's LUID. */
    VkPhysicalDevice physical_device(VkInstance instance);
    /* vkDestroyInstance through the instance's table. */
    void             destroy_instance(VkInstance instance);

private:
    MockPlatform         platform_;
    MockKmt              kmt_;
    ScriptedHost         host_;
    ScriptedAdapterLuids luids_;
    LockTrace            trace_;
    RecordingThreadLock  lock_;
    RecordingThreadLock  send_lock_;
    RecordingLog         log_;
    icd::IcdRoot         root_;
};

}  // namespace av

#endif  // AV_ICD_RIG_HPP
