#include "harness/executor/handle_registry_probe.hpp"

namespace av {

HandleRegistryProbe::HandleRegistryProbe(HandleRegistry &target) : registry(target) {}

uint32_t HandleRegistryProbe::slot_of(uint64_t handle) const { return registry.slots_.slot_of(handle); }

void HandleRegistryProbe::set_parent(uint64_t handle, uint32_t parent) { registry.slots_.find(handle)->parent_ = parent; }

void HandleRegistryProbe::set_children(uint64_t handle, uint32_t children)
{ registry.slots_.find(handle)->children_ = children; }

void HandleRegistryProbe::hold_null()
{ uint32_t slot = 0u;
  static_cast<void>(registry.slots_.insert(0u, HandleSlot{HandleRegistry::no_slot, 0u, HandleKind::other}, slot)); }

}  // namespace av
