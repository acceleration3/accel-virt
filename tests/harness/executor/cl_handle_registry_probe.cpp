#include "harness/executor/cl_handle_registry_probe.hpp"

namespace av {

ClHandleRegistryProbe::ClHandleRegistryProbe(ClHandleRegistry &target) : registry(target) {}

void ClHandleRegistryProbe::set_references(const void *object, uint32_t references)
{ registry.handles_.find(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(object)))->references_ = references; }

void ClHandleRegistryProbe::hold_null()
{ uint32_t slot = 0u;
  static_cast<void>(registry.handles_.insert(0u, ClHandle{1u, clproto::HandleKind::mem}, slot)); }

}  // namespace av
