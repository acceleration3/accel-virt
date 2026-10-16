/* How Vulkan's present queue waits for a presented frame (WindowPresentQueue's
 * readiness): the fence of the copy that put it where it is shown from,
 * waited through the ICD's own vkWaitForFences - which asks the host a slice
 * at a time (HostCompletionWait), never holding the instance's worker for the
 * whole wait - and reset for the slot's next copy once it signalled. A wait
 * returns after slice_ns so the thread can see it is asked to stop. */
#ifndef AV_FENCE_READY_HPP
#define AV_FENCE_READY_HPP

#include "icd/common/frame_wait.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av::icd {

class EntryLookup;

class FenceReady {
public:
    /* The copy a frame waits on: its device and fence. */
    struct Mark {
        VkDevice device_;
        VkFence  fence_;
    };

    static constexpr uint64_t slice_ns = 100000000u;

    explicit FenceReady(EntryLookup &entries);
    FenceReady(const FenceReady &) = delete;
    FenceReady &operator=(const FenceReady &) = delete;

    /* drawn once the fence signalled, which is then reset; later while it
     * has not; gone for a device that is lost or a fence that cannot be
     * waited on. */
    FrameWait wait(const Mark &mark);

private:
    EntryLookup &entries_;
};

}  // namespace av::icd

#endif  // AV_FENCE_READY_HPP
