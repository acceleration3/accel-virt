/* Reaches into a HandleRegistry so invariant tests can corrupt it. Declared a
 * friend by HandleRegistry; nothing outside the tests uses it. */
#ifndef AV_HANDLE_REGISTRY_PROBE_HPP
#define AV_HANDLE_REGISTRY_PROBE_HPP

#include "executor/vulkan/handle_registry.hpp"

#include <stdint.h>

namespace av {

struct HandleRegistryProbe {
    explicit HandleRegistryProbe(HandleRegistry &target);

    uint32_t slot_of(uint64_t handle) const;
    void     set_parent(uint64_t handle, uint32_t parent);
    void     set_children(uint64_t handle, uint32_t children);
    /* The null handle held, as no remember would hold it. */
    void     hold_null();

    HandleRegistry &registry;
};

}  // namespace av

#endif  // AV_HANDLE_REGISTRY_PROBE_HPP
