#include "core/rvk/host_visible_binding.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

HostVisibleBinding::HostVisibleBinding() : allocation_(nullptr), resource_id_(0u)
{
    AV_SCOPE("HostVisibleBinding::HostVisibleBinding");
}

void HostVisibleBinding::take(const Allocation &allocation, uint32_t resource_id)
{
    AV_SCOPE("HostVisibleBinding::take");
    allocation_ = &allocation;
    resource_id_ = resource_id;
}

void HostVisibleBinding::clear()
{
    AV_SCOPE("HostVisibleBinding::clear");
    allocation_ = nullptr;
    resource_id_ = 0u;
}

bool              HostVisibleBinding::bound() const
{
    AV_SCOPE("HostVisibleBinding::bound");
    return allocation_ != nullptr;
}
const Allocation *HostVisibleBinding::allocation() const
{
    AV_SCOPE("HostVisibleBinding::allocation");
    return allocation_;
}
uint32_t          HostVisibleBinding::resource_id() const
{
    AV_SCOPE("HostVisibleBinding::resource_id");
    return resource_id_;
}

/* A slot holds an allocation exactly when it names a resource. */
bool HostVisibleBinding::invariants_hold() const
{
    AV_SCOPE("HostVisibleBinding::invariants_hold");
    return (allocation_ == nullptr) == (resource_id_ == 0u);
}

}  // namespace av
