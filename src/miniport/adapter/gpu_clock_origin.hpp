#ifndef AV_GPU_CLOCK_ORIGIN_HPP
#define AV_GPU_CLOCK_ORIGIN_HPP

#include "core/platform/spin_lock.hpp"

#include <stdint.h>

namespace av {

class ClockOriginRecord;
class Platform;

class GpuClockOrigin {
public:
    explicit GpuClockOrigin(Platform &platform);
    GpuClockOrigin(const GpuClockOrigin &) = delete;
    GpuClockOrigin &operator=(const GpuClockOrigin &) = delete;

    void adopt(const ClockOriginRecord &origin);
    [[nodiscard]] bool project(uint64_t counter, uint64_t counter_frequency, uint64_t &ticks, uint64_t &frequency);

private:
    SpinLock lock_;
    bool     known_;
    uint64_t counter_;
    uint64_t ticks_;
    uint64_t frequency_;
};

}

#endif
