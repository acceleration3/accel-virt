/* The order a context's batches reach its ring in, once its process releases
 * them by packet (ReleaseRecord): the process writes batches into the ring
 * without publishing them, and the scheduler hands the miniport the packets
 * that release them in whatever order the waits on each packet's context
 * allow. A batch is published only when every one before it in the ring is,
 * so a packet whose predecessors are still to come is held here until they
 * have, at most `most_held` of them - a packet past that is refused, which
 * fails the context (AvContextTable::lose). A batch already released (a packet
 * dxgkrnl submits again after a preemption) is done, not an error.
 *
 * The order begins where the ring's tail was when the first batch arrived. Nothing here is the
 * ring's: RingSubmitter judges every batch it is handed. Guarded by the
 * AvContextTable's lock. */
#ifndef AV_RELEASE_QUEUE_HPP
#define AV_RELEASE_QUEUE_HPP

#include "proto/submission/submission_descriptor.hpp"

#include <stdint.h>

namespace av {

class ReleaseQueue {
public:
    /* More than a transport has batches in flight (its fence window is 64), so
     * a full queue is a ring that is not the process's doing. */
    static constexpr uint32_t most_held = 64u;

    enum class Turn : uint8_t { release_now, held, released_already, refused };

    ReleaseQueue();

    /* Where `batch` stands, `tail` being where the ring's next batch begins as
     * far as the device is concerned - which fixes the start of the order the
     * first time, so a batch published before releasing began and not yet
     * handed over holds the ones after it: release_now - hand it to the ring,
     * then `released()` and `next_held()` until none; held - kept; refused -
     * it overlaps one released, or the queue is full. */
    Turn     arrive(const SubmissionDescriptor &batch, uint64_t tail);
    /* `batch` went into the ring. */
    void     released(const SubmissionDescriptor &batch);
    /* The held batch that follows what was released last, taken out. */
    bool     next_held(SubmissionDescriptor &batch);
    /* Releasing by packet began: false until the first release. */
    bool     active() const;
    /* The context is gone or the device was reset: nothing held is ever
     * released, and the next release begins an order again. */
    void     clear();
    uint32_t held() const;

private:
    SubmissionDescriptor held_[most_held];
    uint32_t             count_;
    uint64_t             next_;
    bool                 active_;
};

}  // namespace av

#endif  // AV_RELEASE_QUEUE_HPP
