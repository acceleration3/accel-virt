#include "executor/d3d/slice_bindings.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

SliceBindings::SliceBindings(BlockSource &blocks) : bound_(blocks, Pool::paged)
{
    AV_SCOPE("SliceBindings::SliceBindings");
}

uint32_t SliceBindings::index_of(uint32_t resource) const
{
    AV_SCOPE("SliceBindings::index_of");
    for (uint32_t index = 0u; index < bound_.size(); index++)
        if (bound_.at(index).resource_ == resource) return index;
    return bound_.size();
}

Status SliceBindings::bind(const Binding &binding)
{
    AV_SCOPE("SliceBindings::bind");
    if (binding.store_ == nullptr || binding.slice_ == nullptr) return Status::invalid_parameter;
    const uint32_t index = index_of(binding.resource_);
    if (index < bound_.size()) {
        bound_.at(index) = binding;
        return Status::ok;
    }
    return bound_.append(binding);
}

bool SliceBindings::take(uint32_t resource, Binding &was)
{
    AV_SCOPE("SliceBindings::take");
    const uint32_t index = index_of(resource);
    if (index >= bound_.size()) return false;
    was = bound_.at(index);
    bound_.remove_swap(index);
    return true;
}

bool SliceBindings::take_bound_to(const SourceMemory &store, Binding &was)
{
    AV_SCOPE("SliceBindings::take_bound_to");
    for (uint32_t index = 0u; index < bound_.size(); index++) {
        if (bound_.at(index).store_ != &store) continue;
        was = bound_.at(index);
        bound_.remove_swap(index);
        return true;
    }
    return false;
}

void SliceBindings::clear()
{
    AV_SCOPE("SliceBindings::clear");
    bound_.clear();
}

uint32_t SliceBindings::count() const
{
    AV_SCOPE("SliceBindings::count");
    return bound_.size();
}

bool SliceBindings::invariants_hold() const
{
    AV_SCOPE("SliceBindings::invariants_hold");
    for (uint32_t index = 0u; index < bound_.size(); index++) {
        const Binding &binding = bound_.at(index);
        if (binding.store_ == nullptr || binding.slice_ == nullptr || index_of(binding.resource_) != index) return false;
    }
    return bound_.invariants_hold();
}

}
