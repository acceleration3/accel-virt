#ifndef AV_ICD_LOSS_LATCH_HPP
#define AV_ICD_LOSS_LATCH_HPP

#include "proto/submission/fault_code.hpp"

#include <stdint.h>

namespace av {
class KeyedLog;
class StateWriter;
}

namespace av::icd {

struct LossLatchProbe;

class LossLatch {
public:
    explicit LossLatch(KeyedLog &lines);
    LossLatch(const LossLatch &) = delete;
    LossLatch &operator=(const LossLatch &) = delete;

    void      lose(FaultCode cause);
    bool      lost() const;
    FaultCode cause() const;
    bool      guilty() const;
    [[nodiscard]] bool refuses(const char *entry);
    void      forget();

    void report(StateWriter &writer) const;
    bool invariants_hold() const;

private:
    static constexpr uint32_t lost_bit = 0x80000000u;

    uint32_t word() const;

    KeyedLog &lines_;
    uint32_t  word_;

    friend struct LossLatchProbe;
};

}

#endif
