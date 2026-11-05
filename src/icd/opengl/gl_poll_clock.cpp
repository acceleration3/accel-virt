#include "icd/opengl/gl_poll_clock.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"

namespace av::glicd {

namespace {
constexpr uint64_t nanoseconds_per_second = 1000000000u;
}

GlPollClock::GlPollClock() : kmt_(nullptr)
{
    AV_SCOPE("GlPollClock::GlPollClock");
}

void GlPollClock::bind(Kmt &kmt)
{
    AV_SCOPE("GlPollClock::bind");
    kmt_ = &kmt;
}

uint64_t GlPollClock::now_nanoseconds() const
{
    AV_SCOPE("GlPollClock::now_nanoseconds");
    if (kmt_ == nullptr) return 0u;
    uint64_t       frequency = 0u;
    const uint64_t ticks = kmt_->performance_counter(frequency);
    if (frequency == 0u) return 0u;
    return ticks / frequency * nanoseconds_per_second + ticks % frequency * nanoseconds_per_second / frequency;
}

void GlPollClock::pause(uint64_t)
{
    AV_SCOPE("GlPollClock::pause");
    if (kmt_ != nullptr) kmt_->sleep_briefly();
}

}
