/* Holds a kernel context's next packets behind the newest batch its session
 * published. The rendering runs on the host from the session's ring, which
 * dxgkrnl never sees, so nothing it schedules on another context waits for it:
 * a present queued straight after the flush would be flipped, or handed to
 * DWM, and a fence the runtime signals would be signaled, while the host is
 * still drawing. This queues a packet whose private data names that batch
 * (FollowRecord), which the miniport completes only once the host has run it;
 * dxgkrnl works through a context in order, so what the runtime queues on the
 * context next waits too. */
#ifndef AV_UM_BATCH_FOLLOWER_HPP
#define AV_UM_BATCH_FOLLOWER_HPP

#include "um/d3d/d3d11_ddi.hpp"

#include <stdint.h>

namespace av {

class KmtTransport;

}  // namespace av

namespace av::d3d {

class BatchFollower {
public:
    BatchFollower(const D3DDDI_DEVICECALLBACKS &kernel, HANDLE device, KmtTransport &transport);
    BatchFollower(const BatchFollower &) = delete;
    BatchFollower &operator=(const BatchFollower &) = delete;

    /* S_OK once the packet is queued on `context`, or at once when the last
     * one queued there already names the newest batch; the kernel's refusal
     * otherwise. */
    [[nodiscard]] HRESULT follow(HANDLE context);

private:
    const D3DDDI_DEVICECALLBACKS &kernel_;
    HANDLE                        device_;
    KmtTransport                 &transport_;
    /* What the last packet followed: a context runs its packets in order, so
     * one there behind the same batch would wait for nothing more. */
    HANDLE                        followed_context_;
    uint64_t                      followed_fence_;
};

}  // namespace av::d3d

#endif  // AV_UM_BATCH_FOLLOWER_HPP
