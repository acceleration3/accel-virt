#ifndef AV_VBLANK_PHASE_HPP
#define AV_VBLANK_PHASE_HPP

#include <stdint.h>

namespace av {

class VblankPhase {
public:
    static constexpr uint32_t window = 64u;

    VblankPhase();

    void     observe(uint64_t sequence, uint64_t now, uint32_t period_ns);
    bool     known() const;
    uint64_t since_blank(uint64_t now) const;
    uint64_t period() const;
    void     forget();

    bool invariants_hold() const;

private:
    static constexpr uint64_t ns_per_tick = 100u;

    uint32_t period_ns_;
    int64_t  anchor_;
    int64_t  candidate_;
    uint32_t seen_;
    bool     known_;
};

}

#endif
