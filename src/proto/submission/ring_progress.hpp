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

}

#endif
