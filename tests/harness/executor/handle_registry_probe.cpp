#include "harness/executor/handle_registry_probe.hpp"

namespace av {

HandleRegistryProbe::HandleRegistryProbe(HandleRegistry &target) : registry(target) {}

void HandleRegistryProbe::set_count(uint32_t count) { registry.count_ = count; }

void HandleRegistryProbe::set_handle(uint32_t index, uint64_t handle)
{ registry.handles_[index] = handle; }

}  // namespace av
