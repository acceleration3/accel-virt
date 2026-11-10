#include "icd/opencl/cl_context_group.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClContextGroup::ClContextGroup() : owner_(nullptr), members_(), count_(0u)
{
    AV_SCOPE("ClContextGroup::ClContextGroup");
}

bool ClContextGroup::add(GuestClContext &session, ClDevice &device, uint64_t host_context)
{
    AV_SCOPE("ClContextGroup::add");
    if (count_ == most || host_context == 0u || of(&session) != nullptr || of(&device) != nullptr) return false;
    members_[count_++] = Member{&session, &device, host_context, 0u, 0u};
    return true;
}

void ClContextGroup::bind(ClObject &owner)
{
    AV_SCOPE("ClContextGroup::bind");
    owner_ = &owner;
}

ClObject &ClContextGroup::owner() const
{
    AV_SCOPE("ClContextGroup::owner");
    return *owner_;
}

uint32_t ClContextGroup::count() const
{
    AV_SCOPE("ClContextGroup::count");
    return count_;
}

ClContextGroup::Member &ClContextGroup::at(uint32_t index)
{
    AV_SCOPE("ClContextGroup::at");
    return members_[index];
}

const ClContextGroup::Member &ClContextGroup::at(uint32_t index) const
{
    AV_SCOPE("ClContextGroup::at");
    return members_[index];
}

ClContextGroup::Member *ClContextGroup::of(const GuestClContext *session)
{
    AV_SCOPE("ClContextGroup::of");
    for (uint32_t index = 0u; index < count_; ++index)
        if (members_[index].session == session) return &members_[index];
    return nullptr;
}

ClContextGroup::Member *ClContextGroup::of(const ClDevice *device)
{
    AV_SCOPE("ClContextGroup::of");
    for (uint32_t index = 0u; index < count_; ++index)
        if (members_[index].device == device) return &members_[index];
    return nullptr;
}

bool ClContextGroup::has(const GuestClContext *session) const
{
    AV_SCOPE("ClContextGroup::has");
    for (uint32_t index = 0u; index < count_; ++index)
        if (members_[index].session == session) return true;
    return false;
}

bool ClContextGroup::invariants_hold() const
{
    AV_SCOPE("ClContextGroup::invariants_hold");
    if (owner_ == nullptr || count_ > most) return false;
    for (uint32_t index = 0u; index < count_; ++index) {
        if (members_[index].session == nullptr || members_[index].device == nullptr || members_[index].host_context == 0u)
            return false;
        for (uint32_t other = 0u; other < index; ++other)
            if (members_[other].session == members_[index].session || members_[other].device == members_[index].device)
                return false;
    }
    return true;
}

}
