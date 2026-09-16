#include "core/adapter/power_transition.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

PowerTransition::PowerTransition() : sources_(0u), started_(false), held_(false)
{
    AV_SCOPE("PowerTransition::PowerTransition");
}

Status PowerTransition::hold(bool started, uint32_t sources)
{
    AV_SCOPE("PowerTransition::hold");
    if (held_) return Status::invalid_state;
    started_ = started;
    sources_ = sources;
    held_ = true;
    return Status::ok;
}

void PowerTransition::release()
{
    AV_SCOPE("PowerTransition::release");
    started_ = false;
    sources_ = 0u;
    held_ = false;
}

bool PowerTransition::held() const
{
    AV_SCOPE("PowerTransition::held");
    return held_;
}

bool PowerTransition::started() const
{
    AV_SCOPE("PowerTransition::started");
    return started_;
}

bool PowerTransition::scanning_out(uint32_t source) const
{
    AV_SCOPE("PowerTransition::scanning_out");
    return source < sizeof(sources_) * 8u && (sources_ & (1u << source)) != 0u;
}

}  // namespace av
