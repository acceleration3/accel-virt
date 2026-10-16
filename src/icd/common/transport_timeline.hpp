/* How far the host GPU has got with one channel's batches
 * (docs/mapped-buffers-plan.md, 3.3): the batch the commands made now go out
 * in is the one the transport publishes next, the fence page says how far the
 * GPU has got, and a wait sends what is held when the fence asked for is
 * still held. A driver with no fence page is waited on for the whole context,
 * after which every batch waited for is known done though its page says
 * nothing.
 *
 * What memory the GPU reads is reused by - Direct3D's buffer slices - and how
 * far ahead of the host an application runs - OpenGL's frames in flight. */
#ifndef AV_ICD_TRANSPORT_TIMELINE_HPP
#define AV_ICD_TRANSPORT_TIMELINE_HPP

#include <stdint.h>

namespace av::icd {

class HostChannel;

class TransportTimeline {
public:
    /* Over `channel`, read only while it is open. */
    explicit TransportTimeline(HostChannel &channel);
    TransportTimeline(const TransportTimeline &) = delete;
    TransportTimeline &operator=(const TransportTimeline &) = delete;

    /* The fence of the batch the commands made now go out in. Any thread. */
    uint64_t           recording() const;
    /* The highest fence the host GPU has finished. Any thread. */
    uint64_t           completed() const;
    /* Returns once completed() reaches `fence`, what is held sent first when
     * the fence is not yet published; a fence never published - nothing went
     * out in it - is waited for as the newest that was. The caller holds the
     * transport: the channel's send lock, or the one thread it is sent on.
     * False when it cannot be waited for: the channel is lost. */
    [[nodiscard]] bool wait(uint64_t fence);
    /* The channel was opened again, its fences starting from 1. */
    void               forget();

private:
    HostChannel &channel_;
    /* The newest fence a wait returned for, which another thread reads. */
    uint64_t     waited_;
};

}  // namespace av::icd

#endif  // AV_ICD_TRANSPORT_TIMELINE_HPP
