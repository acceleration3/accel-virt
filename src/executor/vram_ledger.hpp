#ifndef AV_VRAM_LEDGER_HPP
#define AV_VRAM_LEDGER_HPP

#include "executor/byte_ledger.hpp"
#include "proto/context/vram_policy.hpp"

#include <stdint.h>

namespace av {

class VramLedger final : public ByteLedger {
public:
    VramLedger();

    void learn(uint64_t total);
    void reserve_vram(uint32_t mib);

    uint64_t total() const;

    bool invariants_hold() const;

private:
    void recompute();

    uint64_t   total_;
    VramPolicy policy_;
};

}

#endif
