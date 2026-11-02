/* The writes presents make into one allocation's pages, counted as a seqlock
 * and the latest ticket that moves them: the present engine moves a present's
 * pixels on a host thread after DxgkDdiPresent returned, and a reader of the
 * pages (docs/stale-blt-frames.md) has to know whether it read while one was
 * being written. The sequence moves at every opening and every write, so a
 * reader that saw the same sequence before and after its read saw no present
 * begin or end meanwhile; the writers still open say whether one is being
 * written now, however many are. The ticket says which present of which
 * engine epoch has the pages still to move. Atomics only, no lock, so a
 * present, the DPC and a reader's escape can all ask at any IRQL. */
#ifndef AV_PRESENT_WRITES_HPP
#define AV_PRESENT_WRITES_HPP

#include "core/platform/platform.hpp"

#include <stdint.h>

namespace av {

class PresentWrites {
public:
    explicit PresentWrites(Platform &platform);
    PresentWrites(const PresentWrites &) = delete;
    PresentWrites &operator=(const PresentWrites &) = delete;

    /* Any IRQL. A present into the pages is about to be queued or moved: the
     * sequence goes up, and so do the writers open. */
    void     opening();
    /* Any IRQL. The present was queued under `ticket` of the engine's `epoch`,
     * or moved in place (0, which leaves both as they were): the sequence
     * goes up again and the writer is closed. The ticket is stored first, so
     * a reader that sees the sequence move sees the ticket that belongs to
     * it. */
    void     written(uint64_t ticket, uint32_t epoch);
    uint64_t sequence();
    /* Whether a present was being written when `sequence` was read, or is
     * being written now. */
    bool     in_progress(uint64_t sequence);
    /* The latest ticket queued into the pages, and the epoch of the engine
     * that numbered it; 0 for both before any. A ticket of an epoch the
     * engine is no longer in names a present that went with it. */
    uint64_t ticket();
    uint32_t epoch();

    bool invariants_hold();

private:
    Platform &platform_;
    uint64_t  sequence_;
    uint64_t  writers_;
    uint64_t  ticket_;
    uint64_t  epoch_;
};

}  // namespace av

#endif  // AV_PRESENT_WRITES_HPP
