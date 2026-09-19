#include "harness/icd/counting_event_owner.hpp"

namespace av {

CountingEventOwner::CountingEventOwner() : last_(nullptr), releases_(0u) {}

void CountingEventOwner::release_watched(clicd::ClObject &event)
{
    last_ = &event;
    ++releases_;
}

std::uint32_t          CountingEventOwner::releases() const { return releases_; }
const clicd::ClObject *CountingEventOwner::last() const { return last_; }

}  // namespace av
