#ifndef AV_ICD_LOCKED_TIMELINE_HPP
#define AV_ICD_LOCKED_TIMELINE_HPP

#include "icd/common/gpu_timeline.hpp"
#include "icd/common/transport_timeline.hpp"

#include <stdint.h>

namespace av {
class ThreadLock;
}

namespace av::icd {

class HostChannel;

class LockedTimeline final : public GpuTimeline {
public:
    LockedTimeline(HostChannel &channel, ThreadLock &lock);

    uint64_t           recording() const override;
    uint64_t           covering() const override;
    uint64_t           completed() const override;
    [[nodiscard]] bool wait(uint64_t fence) override;
    void               forget();

private:
    TransportTimeline timeline_;
    ThreadLock       &lock_;
};

}

#endif
