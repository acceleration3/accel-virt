#ifndef AV_HOST_RAM_LEDGER_HPP
#define AV_HOST_RAM_LEDGER_HPP

#include "executor/byte_ledger.hpp"

#include <stdint.h>

namespace av {

class HostMemoryGauge;

class HostRamLedger final : public ByteLedger {
public:
    static constexpr uint64_t refresh_ms = 100u;
    static constexpr uint64_t relieve_ms = 1000u;

    HostRamLedger();

    [[nodiscard]] bool take(uint64_t bytes) override;

    void     budget(uint64_t bytes);
    uint64_t budget() const;
    void     watch(HostMemoryGauge *gauge);
    void     floor(uint64_t bytes);
    uint64_t floor() const;
    uint64_t available() const;
    uint64_t tight_refusals() const;

private:
    static constexpr uint64_t never = UINT64_MAX;

    bool admit(uint64_t bytes);
    void refresh(uint64_t now);
    void restore(uint64_t bytes);

    uint64_t         budget_;
    HostMemoryGauge *gauge_;
    uint64_t         floor_;
    uint64_t         stamp_;
    uint64_t         available_;
    uint64_t         relieved_;
    uint64_t         tight_refusals_;
    uint32_t         tight_;
};

}

#endif
