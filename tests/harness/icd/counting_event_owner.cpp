#include "harness/icd/counting_event_owner.hpp"

namespace av {

CountingEventOwner::CountingEventOwner()
    : last_(nullptr), releases_(0u), deliveries_(0u), out_of_order_(false)
{}

void CountingEventOwner::deliver_watched(clicd::ClObject &)
{
    ++deliveries_;
}

void CountingEventOwner::release_watched(clicd::ClObject &event)
{
    if (deliveries_ != releases_ + 1u) out_of_order_ = true;
    last_ = &event;
    ++releases_;
}

std::uint32_t          CountingEventOwner::releases() const { return releases_; }
const clicd::ClObject *CountingEventOwner::last() const { return last_; }
std::uint32_t          CountingEventOwner::deliveries() const { return deliveries_; }
bool CountingEventOwner::delivered_before_release() const { return !out_of_order_; }

}
