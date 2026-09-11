/* What one side has seen of a shared ring. The other side can write the header
 * at any time, so every snapshot is checked against the last one accepted:
 * counters only move forward, head never passes tail, and no more than the
 * ring's capacity is outstanding. */
#ifndef AV_RING_PROGRESS_HPP
#define AV_RING_PROGRESS_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class RingHeader;
struct RingProgressProbe;

class RingProgress {
public:
    RingProgress();

    /* invalid_state, keeping the last good snapshot, for anything inconsistent. */
    [[nodiscard]] Status observe(const RingHeader &snapshot);

    bool     started() const;
    uint8_t  order() const;
    uint64_t tail() const;
    uint64_t head() const;

    bool invariants_hold() const;

private:
    bool consistent(uint8_t order, uint64_t tail, uint64_t head) const;

    bool     started_;
    uint8_t  order_;
    uint64_t tail_;
    uint64_t head_;

    friend struct RingProgressProbe;
};

}  // namespace av

#endif  // AV_RING_PROGRESS_HPP
