#ifndef AV_FENCE_READY_HPP
#define AV_FENCE_READY_HPP

#include "icd/common/frame_wait.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av::icd {

class EntryLookup;

class FenceReady {
public:
    struct Mark {
        VkDevice device_;
        VkFence  fence_;
    };

    static constexpr uint64_t slice_ns = 100000000u;

    explicit FenceReady(EntryLookup &entries);
    FenceReady(const FenceReady &) = delete;
    FenceReady &operator=(const FenceReady &) = delete;

    FrameWait wait(const Mark &mark);

private:
    EntryLookup &entries_;
};

}

#endif
