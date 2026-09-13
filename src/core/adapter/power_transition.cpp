#include "core/adapter/power_transition.hpp"

namespace av {

PowerTransition::PowerTransition() : sources_(0u), started_(false), held_(false) {}

Status PowerTransition::hold(bool started, uint32_t sources)
{
    if (held_) return Status::invalid_state;
    started_ = started;
    sources_ = sources;
    held_ = true;
    return Status::ok;
}

void PowerTransition::release()
{
    started_ = false;
    sources_ = 0u;
    held_ = false;
}

bool PowerTransition::held() const
{
    return held_;
}

bool PowerTransition::started() const
{
    return started_;
}

bool PowerTransition::scanning_out(uint32_t source) const
{
    return source < sizeof(sources_) * 8u && (sources_ & (1u << source)) != 0u;
}

}  // namespace av
