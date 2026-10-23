/* Which way one submission goes (feature 049, US1 and US2).
 *
 * Two ways exist and both must work: through the doorbell, where the process
 * publishes its own ring tail and stores to a page, and through
 * `D3DKMTSubmitCommand`, where the miniport publishes the tail once it has
 * checked the submission. The doorbell's status page decides, and it is read
 * per submission - dxgkrnl disconnects a doorbell for suspend, reset,
 * migration and TDR without telling the process (research.md §6).
 *
 * The two are the same submission: the same batch, in the same ring, at the
 * same tail. A fallback resubmits the tail the process already wrote, and a
 * device that has passed that tail consumes nothing twice (US2 scenario 2).
 */
#ifndef AV_SUBMISSION_PATH_HPP
#define AV_SUBMISSION_PATH_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class GuestDoorbell;
class Kmt;
class ReleaseRoute;
class TransportRing;
struct SubmissionPathProbe;

class SubmissionPath {
public:
    SubmissionPath(Kmt &kmt, GuestDoorbell &doorbell, TransportRing &ring);
    SubmissionPath(const SubmissionPath &) = delete;
    SubmissionPath &operator=(const SubmissionPath &) = delete;

    /* One batch, already written into the ring: through the doorbell where its
     * status allows, through the kernel otherwise. `descriptor` is what the
     * kernel path carries - the miniport reads the tail out of it - and
     * `fence` is what this process has queued. */
    [[nodiscard]] Status submit(uint32_t context, uint64_t dma_address, const uint8_t *descriptor,
                                uint32_t size, uint64_t fence);

    /* How each way has been taken, which is what the benchmark measures and
     * what says a disconnect was survived rather than missed. */
    uint64_t through_doorbell() const;
    /* The driver took this process's word that it stores its own tail: a batch
     * submitted through the kernel is published first, so a device already
     * watching the ring takes it without waiting for the scheduler. */
    void     publish_before_kernel();
    uint64_t through_kernel() const;
    /* From now on every batch is released by packet through `route`, whose
     * packets name the ring `ring`: nothing here publishes the tail or rings
     * a doorbell, which would let the device take a batch before the packet
     * that releases it has run. A null route is the ways above again. */
    void     release_through(ReleaseRoute *route, uint32_t ring);
    bool     releases() const;
    uint64_t through_release() const;
    /* Every batch submitted, whichever way it went. */
    uint64_t submitted() const;

    /* Every submission went one way or the other. */
    bool invariants_hold() const;

private:
    Kmt           &kmt_;
    GuestDoorbell &doorbell_;
    TransportRing &ring_;
    uint64_t       doorbell_submissions_;
    uint64_t       kernel_submissions_;
    bool           publishes_tail_;
    uint64_t       user_submissions_;
    ReleaseRoute  *route_;
    uint32_t       route_ring_;
    uint64_t       release_submissions_;

    friend struct SubmissionPathProbe;
};

}  // namespace av

#endif  // AV_SUBMISSION_PATH_HPP
