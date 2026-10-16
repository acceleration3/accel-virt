/* A Direct3D session's GPU timeline as its devices' threads share it: the
 * channel's TransportTimeline, waited on under the session's lock, which
 * every command of the session is written under. */
#ifndef AV_D3D_SESSION_TIMELINE_HPP
#define AV_D3D_SESSION_TIMELINE_HPP

#include "icd/common/transport_timeline.hpp"
#include "icd/d3d/gpu_timeline.hpp"

#include <stdint.h>

namespace av {
class ThreadLock;
}  // namespace av

namespace av::icd {
class HostChannel;
}  // namespace av::icd

namespace av::d3d {

class SessionTimeline final : public GpuTimeline {
public:
    /* `lock` is `channel`'s send lock. */
    SessionTimeline(icd::HostChannel &channel, ThreadLock &lock);

    uint64_t           recording() const override;
    uint64_t           completed() const override;
    /* Takes the session's lock: a caller must not hold it. */
    [[nodiscard]] bool wait(uint64_t fence) override;

private:
    icd::TransportTimeline timeline_;
    ThreadLock            &lock_;
};

}  // namespace av::d3d

#endif  // AV_D3D_SESSION_TIMELINE_HPP
