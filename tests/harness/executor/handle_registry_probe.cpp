#include "harness/executor/handle_registry_probe.hpp"

namespace av {

HandleRegistryProbe::HandleRegistryProbe(HandleRegistry &target) : registry(target) {}

void HandleRegistryProbe::set_count(uint32_t count) { registry.count_ = count; }

void HandleRegistryProbe::set_end(uint32_t end) { registry.end_ = end; }

void HandleRegistryProbe::set_handle(uint32_t index, uint64_t handle)
{ registry.handles_[index] = handle; }

void HandleRegistryProbe::set_parent(uint32_t index, uint16_t parent)
{ registry.parents_[index] = parent; }

}  // namespace av
