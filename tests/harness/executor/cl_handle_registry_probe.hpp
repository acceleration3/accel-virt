/* Reaches into a ClHandleRegistry so invariant tests can corrupt it. Declared a
 * friend by ClHandleRegistry; nothing outside the tests uses it. */
#ifndef AV_CL_HANDLE_REGISTRY_PROBE_HPP
#define AV_CL_HANDLE_REGISTRY_PROBE_HPP

#include "device/executor/cl_handle_registry.hpp"

#include <stdint.h>

namespace av {

struct ClHandleRegistryProbe {
    explicit ClHandleRegistryProbe(ClHandleRegistry &target);

    void set_count(uint32_t count);
    void set_object(uint32_t index, uint64_t object);
    void set_references(uint32_t index, uint32_t references);

    ClHandleRegistry &registry;
};

}  // namespace av

#endif  // AV_CL_HANDLE_REGISTRY_PROBE_HPP
