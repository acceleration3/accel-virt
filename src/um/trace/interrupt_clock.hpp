#ifndef AV_UM_INTERRUPT_CLOCK_HPP
#define AV_UM_INTERRUPT_CLOCK_HPP

#include <stdint.h>

namespace av {

class InterruptClock {
public:
    InterruptClock();

    uint64_t now() const;

private:
    const uint32_t *time_;
};

}

#endif
