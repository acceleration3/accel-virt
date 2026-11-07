#include "miniport/adapter/gpu_clock_origin.hpp"

#include "core/platform/platform.hpp"
#include "core/platform/spin_lock_guard.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/escape/clock_origin_record.hpp"

namespace av {

GpuClockOrigin::GpuClockOrigin(Platform &platform)
    : lock_(platform, LockRank::gpu_clock_origin), known_(false), counter_(0u), ticks_(0u), frequency_(0u)
{
    AV_SCOPE("GpuClockOrigin::GpuClockOrigin");
}

void GpuClockOrigin::adopt(const ClockOriginRecord &origin)
{
    AV_SCOPE("GpuClockOrigin::adopt");
    SpinLockGuard held(lock_);
    counter_ = origin.counter();
    ticks_ = origin.ticks();
    frequency_ = origin.frequency();
    known_ = true;
}

bool GpuClockOrigin::project(uint64_t counter, uint64_t counter_frequency, uint64_t &ticks, uint64_t &frequency)
{
    AV_SCOPE("GpuClockOrigin::project");
    SpinLockGuard held(lock_);
    if (!known_ || counter_frequency == 0u || counter_frequency > ClockOriginRecord::most_frequency) return false;
    const bool     after = counter >= counter_;
    const uint64_t apart = after ? counter - counter_ : counter_ - counter;
    const uint64_t moved = (apart / counter_frequency) * frequency_ + (apart % counter_frequency) * frequency_ / counter_frequency;
    frequency = frequency_;
    if (after) ticks = ticks_ + moved;
    else ticks = ticks_ > moved ? ticks_ - moved : 0u;
    return true;
}

}
