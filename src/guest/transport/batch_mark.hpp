/* Where one batch a context sent completes, for a thread other than the one
 * sending: the device, the context, the hardware queue's progress fence the
 * device raises past each batch it finishes, and the batch's own fence. Taken
 * right after the sending thread flushes, so another can wait for that much
 * of the context's work without touching its transport. With no hardware
 * queue the wait is for the context itself - everything it was sent by then,
 * which is that batch and whatever followed it in the meantime. */
#ifndef AV_BATCH_MARK_HPP
#define AV_BATCH_MARK_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Kmt;

class BatchMark {
public:
    /* What the Kmt's last code is after a wait the kernel bounded, where the
     * host is behind rather than gone: a caller asks again. */
    static constexpr uint32_t wait_timed_out = 0xC00000B5u;

    BatchMark();
    BatchMark(uint32_t device, uint32_t context, uint32_t progress_fence, uint64_t fence);

    bool     waitable() const;
    uint64_t fence() const;
    /* Waits for the batch through `kmt`, which must be one no other thread
     * waits on: a Kmt's waits share its event. The kernel's wait is bounded,
     * so a host behind answers the timeout it gave; invalid_state for a mark
     * of no batch. */
    [[nodiscard]] Status wait(Kmt &kmt) const;

private:
    uint32_t device_;
    uint32_t context_;
    uint32_t progress_fence_;
    uint64_t fence_;
};

}  // namespace av

#endif  // AV_BATCH_MARK_HPP
