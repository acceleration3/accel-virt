#include "miniport/memory/resource.hpp"

#include "core/platform/platform.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

Resource::Resource(Platform &platform) : platform_(platform), allocations_(0u)
{
    AV_SCOPE("Resource::Resource");
}

void Resource::took_one()
{
    AV_SCOPE("Resource::took_one");
    platform_.require_irql_at_most(Irql::apc, "Resource::took_one");
    allocations_++;
}

Status Resource::released_one()
{
    AV_SCOPE("Resource::released_one");
    platform_.require_irql_at_most(Irql::apc, "Resource::released_one");
    if (allocations_ == 0u) return Status::invalid_state;
    allocations_--;
    return Status::ok;
}

uint32_t Resource::allocations() const
{
    AV_SCOPE("Resource::allocations");
    return allocations_;
}

}  // namespace av
