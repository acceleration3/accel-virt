#include "device/renderer/doorbell_offers.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

DoorbellOffers::DoorbellOffers()
{
    AV_SCOPE("DoorbellOffers::DoorbellOffers");
    for (int &descriptor : descriptors_) descriptor = none;
}

Status DoorbellOffers::offer(uint32_t index, int descriptor)
{
    AV_SCOPE("DoorbellOffers::offer");
    if (index >= pages || descriptor < 0) return Status::invalid_parameter;
    if (descriptors_[index] != none) return Status::conflict;
    for (const int offered : descriptors_)
        if (offered == descriptor) return Status::conflict;
    descriptors_[index] = descriptor;
    return Status::ok;
}

Status DoorbellOffers::take(uint32_t index, int &descriptor)
{
    AV_SCOPE("DoorbellOffers::take");
    if (index >= pages || descriptors_[index] == none) return Status::not_found;
    descriptor = descriptors_[index];
    descriptors_[index] = none;
    return Status::ok;
}

Status DoorbellOffers::withdraw(int &descriptor)
{
    AV_SCOPE("DoorbellOffers::withdraw");
    for (uint32_t index = 0u; index < pages; index++)
        if (descriptors_[index] != none) return take(index, descriptor);
    return Status::not_found;
}

bool DoorbellOffers::invariants_hold() const
{
    AV_SCOPE("DoorbellOffers::invariants_hold");
    for (uint32_t first = 0u; first < pages; first++) {
        if (descriptors_[first] == none) continue;
        if (descriptors_[first] < 0) return false;
        for (uint32_t second = first + 1u; second < pages; second++)
            if (descriptors_[second] == descriptors_[first]) return false;
    }
    return true;
}

}
