/* How far the host GPU has got with what a session's Direct3D 11 batches
 * recorded.
 *
 * Every Direct3D 11 session records into its GPU's one immediate context
 * (D3d11HostDevice), and a command recorded there has not run: DXVK submits
 * it when the context is flushed. A batch's fence once told the guest the
 * batch had finished as soon as its commands were recorded, so the guest's
 * kernel signalled what the runtime queued behind it - DWM giving an
 * application's buffer back among it - while the host GPU had not yet
 * composed from that buffer, and the application's next frame was drawn into
 * it while DWM read it.
 *
 * So each batch ends with a signal of the session's own fence with the next
 * mark, flushed, and the ring's worker completes the batch only once the fence
 * passes that mark - as UTM's driver completes a virtio-gpu submit only when
 * the host's fence for it retires. The signal itself is the host device's to
 * make (D3d11HostDevice::signal); this counts. */
#ifndef AV_D3D11_BATCH_PROGRESS_HPP
#define AV_D3D11_BATCH_PROGRESS_HPP

#include <stdint.h>

struct ID3D11Fence;

namespace av {

class D3d11BatchProgress {
public:
    D3d11BatchProgress();
    ~D3d11BatchProgress();
    D3d11BatchProgress(const D3d11BatchProgress &) = delete;
    D3d11BatchProgress &operator=(const D3d11BatchProgress &) = delete;

    /* The session's fence, made at marked() and owned from here; null takes
     * nothing. A fence held already is kept and `fence` let go. */
    void         adopt(ID3D11Fence *fence);
    /* The fence a batch's end signals, null before one is adopted. */
    ID3D11Fence *fence() const;
    /* The mark the next signal sets. */
    uint64_t     next() const;
    /* The fence was signalled with next(): that batch is marked. */
    void         signalled();
    /* The fence goes, with nothing waiting on it; marks keep rising. */
    void         release();

    /* The mark the latest batch left, 0 before the first. */
    uint64_t marked() const;
    /* The highest mark the GPU has finished. Any thread: reads only the
     * fence's completed value. */
    uint64_t reached() const;
    /* Whether the GPU has a marked batch still to finish. */
    bool     pending() const;

    bool invariants_hold() const;

private:
    ID3D11Fence *fence_;
    uint64_t     marked_;
    /* What reached() answers without a fence: every mark made before the
     * fence went was waited out, or went with the session. */
    uint64_t     released_at_;
};

}  // namespace av

#endif  // AV_D3D11_BATCH_PROGRESS_HPP
