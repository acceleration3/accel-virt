#ifndef AV_VRAM_LEDGER_HPP
#define AV_VRAM_LEDGER_HPP

#include "proto/context/vram_policy.hpp"

#include <stdint.h>

namespace av {

class VramLedger {
public:
    VramLedger();
    VramLedger(const VramLedger &) = delete;
    VramLedger &operator=(const VramLedger &) = delete;

    void learn(uint64_t total);
    void reserve_vram(uint32_t mib);

    [[nodiscard]] bool take(uint64_t bytes);
    void               give(uint64_t bytes);

    uint64_t total() const;
    uint64_t cap() const;
    uint64_t used() const;
    uint64_t free_bytes() const;
    uint64_t refusals() const;

    bool invariants_hold() const;

private:
    void recompute();

    uint64_t   total_;
    VramPolicy policy_;
    uint64_t   cap_;
    uint64_t   used_;
    uint64_t   refusals_;
};

}

#endif
