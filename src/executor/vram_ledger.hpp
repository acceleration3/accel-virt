#ifndef AV_VRAM_LEDGER_HPP
#define AV_VRAM_LEDGER_HPP

#include "executor/byte_ledger.hpp"
#include "proto/context/vram_policy.hpp"

#include <stdint.h>

namespace av {

class VramBudgetSource;

class VramLedger final : public ByteLedger {
public:
    VramLedger();

    [[nodiscard]] bool take(uint64_t bytes) override;
    [[nodiscard]] bool take_session(uint64_t bytes) override;

    void learn(uint64_t total);
    void follow(VramBudgetSource *source);
    void reserve_vram(uint32_t mib);

    uint64_t total() const;

    bool invariants_hold() const;

private:
    void recompute();
    void refresh();

    uint64_t   total_;
    VramPolicy policy_;
    VramBudgetSource *source_;
    uint64_t   stamp_;
};

}

#endif
