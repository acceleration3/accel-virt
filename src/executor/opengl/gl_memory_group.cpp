#include "executor/opengl/gl_memory_group.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

GlMemoryGroup::GlMemoryGroup() : lock_(), members_{}, count_(0u)
{
    AV_SCOPE("GlMemoryGroup::GlMemoryGroup");
}

void GlMemoryGroup::lock()
{
    AV_SCOPE("GlMemoryGroup::lock");
    lock_.acquire();
}

void GlMemoryGroup::unlock()
{
    AV_SCOPE("GlMemoryGroup::unlock");
    lock_.release();
}

bool GlMemoryGroup::join(GlMemoryObjects &member)
{
    AV_SCOPE("GlMemoryGroup::join");
    lock_.acquire();
    bool joined = false;
    for (uint32_t index = 0u; index < count_ && !joined; index++) joined = members_[index] == &member;
    if (!joined && count_ < most_members) {
        members_[count_++] = &member;
        joined = true;
    }
    lock_.release();
    return joined;
}

void GlMemoryGroup::part(GlMemoryObjects &member)
{
    AV_SCOPE("GlMemoryGroup::part");
    lock_.acquire();
    for (uint32_t index = 0u; index < count_; index++) {
        if (members_[index] != &member) continue;
        members_[index] = members_[--count_];
        members_[count_] = nullptr;
        break;
    }
    lock_.release();
}

uint32_t GlMemoryGroup::members() const
{
    AV_SCOPE("GlMemoryGroup::members");
    return count_;
}

GlMemoryObjects *GlMemoryGroup::at(uint32_t index) const
{
    AV_SCOPE("GlMemoryGroup::at");
    return index < count_ ? members_[index] : nullptr;
}

}
