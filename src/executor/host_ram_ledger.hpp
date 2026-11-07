#ifndef AV_HOST_RAM_LEDGER_HPP
#define AV_HOST_RAM_LEDGER_HPP

#include "executor/byte_ledger.hpp"

#include <stdint.h>

namespace av {

class HostRamLedger final : public ByteLedger {
public:
    HostRamLedger();

    void     budget(uint64_t bytes);
    uint64_t budget() const;

private:
    uint64_t budget_;
};

}

#endif
