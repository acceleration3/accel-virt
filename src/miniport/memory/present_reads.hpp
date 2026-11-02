#ifndef AV_PRESENT_READS_HPP
#define AV_PRESENT_READS_HPP

#include "core/platform/platform.hpp"
#include "core/platform/spin_lock.hpp"

#include <stdint.h>

namespace av {

class PresentWrites;
struct PresentReadsProbe;

class PresentReads {
public:
    static constexpr uint64_t expiry_ticks = 1000000u;

    explicit PresentReads(Platform &platform);
    PresentReads(const PresentReads &) = delete;
    PresentReads &operator=(const PresentReads &) = delete;

    [[nodiscard]] bool open();
    void               close();
    [[nodiscard]] bool admit(PresentWrites &writes);

    bool invariants_hold();

private:
    bool holding(uint64_t now) const;

    Platform &platform_;
    SpinLock  lock_;
    uint64_t  open_;
    uint64_t  expires_;

    friend struct PresentReadsProbe;
};

}

#endif
