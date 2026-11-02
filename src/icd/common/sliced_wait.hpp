#ifndef AV_ICD_SLICED_WAIT_HPP
#define AV_ICD_SLICED_WAIT_HPP

#include <stdint.h>

namespace av::icd {

struct SlicedWaitProbe;

class SlicedWait {
public:
    static constexpr uint64_t slice_ns = 500000000u;
    static constexpr uint64_t forever = ~uint64_t{0};

    SlicedWait(uint64_t timeout, uint64_t now, uint64_t frequency);

    [[nodiscard]] bool next(uint64_t now, uint64_t &slice);

    bool invariants_hold() const;

private:
    uint64_t elapsed_ns(uint64_t now) const;

    uint64_t timeout_;
    uint64_t start_;
    uint64_t frequency_;
    uint64_t slice_;
    bool     asked_;

    friend struct SlicedWaitProbe;
};

}

#endif
