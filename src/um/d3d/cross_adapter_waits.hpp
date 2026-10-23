/* What keeps a device's work on the host behind the GPU waits the runtime
 * queues on the device's kernel context, for a device that shares a
 * cross-adapter resource.
 *
 * Two adapters' devices order their work over a shared resource with a shared
 * fence: one signals it from the GPU after the work that wrote the resource,
 * the other has the runtime queue a wait for it on its kernel context before
 * the work that reads it (ID3D11DeviceContext4::Wait, ID3D12CommandQueue::Wait).
 * On a real GPU the same context then runs the work; here the work is sent to
 * the host on the transport's ring, which dxgkrnl never sees, and would run
 * before the wait is satisfied. A CPU wait for the context cannot say when
 * that is (it stalled or went too soon), so the scheduler does the ordering:
 * once a device shares such a resource its transport releases every batch by
 * packet (ReleaseRecord) on the kernel context the device submitted on, and
 * the miniport publishes the batch only when dxgkrnl hands it the packet -
 * after every wait queued on that context before it - and every batch before
 * it in the ring has been published (ReleaseQueue). A present's reuse wait is
 * a wait on the same context, so the packets after the present are ordered
 * behind it without a gate of PresentOrder's.
 *
 * Direct3D 11 devices only: one context, so the packets reach the miniport in
 * ring order. A Direct3D 12 device with a cross-adapter heap publishes as it
 * always did, unordered against the other adapter, because its queues would
 * share one ring and a wait on one queue for a fence a later batch on another
 * signals could never be satisfied (the real fix is one av ring per kernel
 * context, docs/roadmap.md). The same limit holds for a Direct3D 11 session
 * whose application satisfies a queued wait from its own thread later and
 * then makes a call that waits for the host's answer: that call waits for a
 * batch held behind the wait. Such a wait is logged once after a few seconds.
 *
 * The state here is the session's: a batch is released under the session's
 * send lock, so everything that changes it takes that lock - which the
 * caller does not hold. */
#ifndef AV_UM_CROSS_ADAPTER_WAITS_HPP
#define AV_UM_CROSS_ADAPTER_WAITS_HPP

#include "core/platform/thread_lock.hpp"
#include "guest/transport/release_route.hpp"
#include "um/d3d/d3d11_ddi.hpp"

#include <stdint.h>

namespace av {

class KeyedLog;
class KmtTransport;

}  // namespace av

namespace av::d3d {

class CrossAdapterWaits final : public ReleaseRoute {
public:
    /* Over the runtime's `kernel` callbacks and `device` handle, releasing
     * `transport`'s batches - whose send lock is `lock` - on `context`, the
     * one kernel context a Direct3D 11 device submits on. */
    CrossAdapterWaits(const D3DDDI_DEVICECALLBACKS &kernel, HANDLE device, KmtTransport &transport,
                      ThreadLock &lock, KeyedLog &lines, HANDLE context);
    /* The transport goes back to publishing for itself, once what was
     * released has run. */
    ~CrossAdapterWaits();
    CrossAdapterWaits(const CrossAdapterWaits &) = delete;
    CrossAdapterWaits &operator=(const CrossAdapterWaits &) = delete;

    /* The device shares a cross-adapter resource: from now on its batches are
     * released by packet. Takes the send lock. */
    void shares();
    bool shared() const;
    Status release(const uint8_t *record, uint32_t size) override;
    void   waited_long() override;

private:
    const D3DDDI_DEVICECALLBACKS &kernel_;
    HANDLE                        device_;
    KmtTransport                 &transport_;
    ThreadLock                   &lock_;
    KeyedLog                     &lines_;
    HANDLE                        context_;
    bool                          shared_;
};

}  // namespace av::d3d

#endif  // AV_UM_CROSS_ADAPTER_WAITS_HPP
