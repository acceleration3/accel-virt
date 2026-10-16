/* A session's GPU timeline as its stream and transport keep it
 * (docs/mapped-buffers-plan.md, 3.3): the batch being recorded is the one
 * the transport publishes next, the fence page says how far the GPU has got,
 * and a wait sends what is recorded when the fence asked for is still being
 * recorded. A driver with no fence page is waited on for the whole context,
 * after which every batch waited for is known done. */
#ifndef AV_D3D_STREAM_TIMELINE_HPP
#define AV_D3D_STREAM_TIMELINE_HPP

#include "icd/d3d/gpu_timeline.hpp"

#include <stdint.h>

namespace av {
class KmtTransport;
class ThreadLock;
}  // namespace av

namespace av::d3d {

class D3dCommandStream;

class StreamTimeline final : public GpuTimeline {
public:
    /* `lock` is the one every command of the stream is written under. */
    StreamTimeline(D3dCommandStream &stream, KmtTransport &transport, ThreadLock &lock);

    uint64_t           recording() const override;
    uint64_t           completed() const override;
    /* Takes the stream's lock: a caller must not hold it. */
    [[nodiscard]] bool wait(uint64_t fence) override;

private:
    D3dCommandStream &stream_;
    KmtTransport     &transport_;
    ThreadLock       &lock_;
    /* The newest fence a wait returned for, which a driver with no fence page
     * knows done though its page says nothing. */
    uint64_t          waited_;
};

}  // namespace av::d3d

#endif  // AV_D3D_STREAM_TIMELINE_HPP
