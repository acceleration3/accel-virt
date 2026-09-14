#include "harness/executor/cl_handle_registry_probe.hpp"

namespace av {

ClHandleRegistryProbe::ClHandleRegistryProbe(ClHandleRegistry &target) : registry(target) {}

void ClHandleRegistryProbe::set_count(uint32_t count) { registry.count_ = count; }

void ClHandleRegistryProbe::set_object(uint32_t index, uint64_t object)
{ registry.objects_[index] = object; }

void ClHandleRegistryProbe::set_references(uint32_t index, uint32_t references)
{ registry.references_[index] = references; }

}  // namespace av
