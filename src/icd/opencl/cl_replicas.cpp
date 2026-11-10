#include "icd/opencl/cl_replicas.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClReplicas::ClReplicas() : entries_(), count_(0u)
{
    AV_SCOPE("ClReplicas::ClReplicas");
}

bool ClReplicas::add(GuestClContext &session, uint64_t wire)
{
    AV_SCOPE("ClReplicas::add");
    if (wire == 0u || count_ == most || in(&session) != 0u) return false;
    entries_[count_++] = Entry{&session, wire};
    return true;
}

uint64_t ClReplicas::in(const GuestClContext *session) const
{
    AV_SCOPE("ClReplicas::in");
    for (uint32_t index = 0u; index < count_; ++index)
        if (entries_[index].session == session) return entries_[index].wire;
    return 0u;
}

uint32_t ClReplicas::count() const
{
    AV_SCOPE("ClReplicas::count");
    return count_;
}

const ClReplicas::Entry &ClReplicas::at(uint32_t index) const
{
    AV_SCOPE("ClReplicas::at");
    return entries_[index];
}

bool ClReplicas::invariants_hold() const
{
    AV_SCOPE("ClReplicas::invariants_hold");
    if (count_ > most) return false;
    for (uint32_t index = 0u; index < count_; ++index) {
        if (entries_[index].session == nullptr || entries_[index].wire == 0u) return false;
        for (uint32_t other = 0u; other < index; ++other)
            if (entries_[other].session == entries_[index].session) return false;
    }
    return true;
}

}
