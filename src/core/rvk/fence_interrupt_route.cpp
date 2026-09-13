#include "core/rvk/fence_interrupt_route.hpp"

#include "core/rvk/rvk_context_binding.hpp"

namespace av {

FenceInterruptRoute::FenceInterruptRoute(Platform &platform) : platform_(platform), bindings_{} {}

Status FenceInterruptRoute::attach(uint32_t vector, RvkContextBinding &binding)
{
    platform_.require_irql_at_most(Irql::dispatch, "FenceInterruptRoute::attach");
    if (vector >= fence_queues) return Status::invalid_parameter;
    if (bindings_[vector] != nullptr) return Status::invalid_state;
    bindings_[vector] = &binding;
    return Status::ok;
}

void FenceInterruptRoute::detach(uint32_t vector)
{
    platform_.require_irql_at_most(Irql::dispatch, "FenceInterruptRoute::detach");
    if (vector < fence_queues) bindings_[vector] = nullptr;
}

void FenceInterruptRoute::serve(uint32_t vector)
{
    platform_.require_irql_at_most(Irql::dispatch, "FenceInterruptRoute::serve");
    if (vector < fence_queues && bindings_[vector] != nullptr) static_cast<void>(bindings_[vector]->serve());
}

bool FenceInterruptRoute::routed(uint32_t vector) const
{
    return vector < fence_queues && bindings_[vector] != nullptr;
}

bool FenceInterruptRoute::invariants_hold() const { return true; }

}  // namespace av
