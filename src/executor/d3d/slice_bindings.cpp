#include "executor/d3d/slice_bindings.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

SliceBindings::SliceBindings(BlockSource &blocks) : bound_(blocks, Pool::paged)
{
    AV_SCOPE("SliceBindings::SliceBindings");
}

Status SliceBindings::bind(const Binding &binding)
{
    AV_SCOPE("SliceBindings::bind");
    if (binding.store_ == nullptr || binding.slice_ == nullptr) return Status::invalid_parameter;
    Binding *const held = bound_.find(binding.resource_);
    if (held != nullptr) {
        *held = binding;
        return Status::ok;
    }
    uint32_t slot = KeyedSlots<uint32_t, Binding>::no_slot;
    return bound_.insert(binding.resource_, binding, slot);
}

bool SliceBindings::take(uint32_t resource, Binding &was)
{
    AV_SCOPE("SliceBindings::take");
    return bound_.take(resource, was);
}

bool SliceBindings::find(uint32_t resource, Binding &was) const
{
    AV_SCOPE("SliceBindings::find");
    const Binding *const held = bound_.find(resource);
    if (held == nullptr) return false;
    was = *held;
    return true;
}

void SliceBindings::clear()
{
    AV_SCOPE("SliceBindings::clear");
    bound_.clear();
}

uint32_t SliceBindings::count() const
{
    AV_SCOPE("SliceBindings::count");
    return bound_.count();
}

bool SliceBindings::invariants_hold() const
{
    AV_SCOPE("SliceBindings::invariants_hold");
    bool sound = true;
    bound_.for_each([&sound](uint32_t key, const Binding &binding) {
        sound = sound && binding.store_ != nullptr && binding.slice_ != nullptr && binding.resource_ == key;
    });
    return sound && bound_.invariants_hold();
}

}
