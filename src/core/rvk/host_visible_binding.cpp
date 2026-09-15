#include "core/rvk/host_visible_binding.hpp"

namespace av {

HostVisibleBinding::HostVisibleBinding() : allocation_(nullptr), resource_id_(0u) {}

void HostVisibleBinding::take(const Allocation &allocation, uint32_t resource_id)
{
    allocation_ = &allocation;
    resource_id_ = resource_id;
}

void HostVisibleBinding::clear()
{
    allocation_ = nullptr;
    resource_id_ = 0u;
}

bool              HostVisibleBinding::bound() const { return allocation_ != nullptr; }
const Allocation *HostVisibleBinding::allocation() const { return allocation_; }
uint32_t          HostVisibleBinding::resource_id() const { return resource_id_; }

/* A slot holds an allocation exactly when it names a resource. */
bool HostVisibleBinding::invariants_hold() const { return (allocation_ == nullptr) == (resource_id_ == 0u); }

}  // namespace av
