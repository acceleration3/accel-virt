#ifndef AV_D3D_SESSION_TIMELINE_HPP
#define AV_D3D_SESSION_TIMELINE_HPP

#include "icd/common/transport_timeline.hpp"
#include "icd/d3d/gpu_timeline.hpp"

#include <stdint.h>

namespace av {
class ThreadLock;
}

namespace av::icd {
class HostChannel;
}

namespace av::d3d {

class SessionTimeline final : public GpuTimeline {
public:
    SessionTimeline(icd::HostChannel &channel, ThreadLock &lock);

    uint64_t           recording() const override;
    uint64_t           completed() const override;
    [[nodiscard]] bool wait(uint64_t fence) override;

private:
    icd::TransportTimeline timeline_;
    ThreadLock            &lock_;
};

}

#endif
