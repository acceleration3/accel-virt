/* The writes presents make into one allocation's pages, counted as a seqlock
 * and the latest ticket that moves them: the present engine moves a present's
 * pixels on a host thread after DxgkDdiPresent returned, and a reader of the
 * pages (docs/stale-blt-frames.md) has to know whether it read while one was
 * being written. The sequence goes odd when a present is about to be queued
 * and even again once it is; the ticket says which present of the engine has
 * the pages still to move. Atomics only, no lock, so a present, the DPC and a
 * reader's escape can all ask at any IRQL. */
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
     * sequence goes odd. */
    void     opening();
    /* Any IRQL. The present was queued under `ticket`, or moved in place
     * (0, which leaves the ticket as it was): the sequence goes even again.
     * The ticket is stored first, so a reader that sees the even sequence
     * sees the ticket that belongs to it. */
    void     written(uint64_t ticket);
    uint64_t sequence();
    /* Whether a present was being written when `sequence` was read. */
    bool     in_progress(uint64_t sequence) const;
    /* The latest ticket queued into the pages; 0 before any. */
    uint64_t ticket();

    bool invariants_hold();

private:
    Platform &platform_;
    uint64_t  sequence_;
    uint64_t  ticket_;
};

}  // namespace av

#endif  // AV_PRESENT_WRITES_HPP
