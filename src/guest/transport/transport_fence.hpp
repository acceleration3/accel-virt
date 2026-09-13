/* The fences one context has in flight (docs/protocol.md). A producer gives
 * each batch the next fence id; the device completes them in ring order, so the
 * outstanding set is exactly the range (completed, next) and needs no table. The
 * transport asks whether a fence still needs waiting on, how many are in
 * flight, and the oldest outstanding fence - the one whose completion frees ring
 * room when the ring is full. At most `max_outstanding` are in flight at once. */
#ifndef AV_TRANSPORT_FENCE_HPP
#define AV_TRANSPORT_FENCE_HPP

#include <stdint.h>

namespace av {

struct TransportFenceProbe;

class TransportFence {
public:
    static constexpr uint32_t max_outstanding = 64u;

    TransportFence();

    /* The next fence id, now outstanding; 0 when `max_outstanding` are already
     * in flight - the producer must wait for one to complete first. */
    uint64_t open();
    /* The id open() would return next, without committing it: 0 when full. */
    uint64_t peek() const;
    /* The device completed up to `fence_id`; retires it and every earlier one.
     * A completion not greater than what is done, or past what was opened, is
     * ignored. */
    void     close(uint64_t fence_id);
    /* Whether `fence_id` was opened and is not yet completed. */
    bool     pending(uint64_t fence_id) const;
    uint64_t completed() const;
    uint32_t outstanding() const;
    /* The oldest outstanding fence, 0 when none: which to wait on for room. */
    uint64_t oldest() const;
    bool     full() const;

    bool invariants_hold() const;

private:
    uint64_t next_;        /* the next id to hand out; ids are 1.. */
    uint64_t completed_;   /* the highest fence the device has finished */

    friend struct TransportFenceProbe;
};

}  // namespace av

#endif  // AV_TRANSPORT_FENCE_HPP
