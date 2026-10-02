/* Reaches into a HandleRegistry so invariant tests can corrupt it. Declared a
 * friend by HandleRegistry; nothing outside the tests uses it. */
#ifndef AV_HANDLE_REGISTRY_PROBE_HPP
#define AV_HANDLE_REGISTRY_PROBE_HPP

#include "executor/vulkan/handle_registry.hpp"

#include <stdint.h>

namespace av {

struct HandleRegistryProbe {
    explicit HandleRegistryProbe(HandleRegistry &target);

    void set_count(uint32_t count);
    void set_end(uint32_t end);
    void set_handle(uint32_t index, uint64_t handle);
    void set_parent(uint32_t index, uint16_t parent);

    HandleRegistry &registry;
};

}  // namespace av

#endif  // AV_HANDLE_REGISTRY_PROBE_HPP
