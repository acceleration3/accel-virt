/* Reaches into a ClHandleRegistry so invariant tests can corrupt it. Declared a
 * friend by ClHandleRegistry; nothing outside the tests uses it. */
#ifndef AV_CL_HANDLE_REGISTRY_PROBE_HPP
#define AV_CL_HANDLE_REGISTRY_PROBE_HPP

#include "executor/opencl/cl_handle_registry.hpp"

#include <stdint.h>

namespace av {

struct ClHandleRegistryProbe {
    explicit ClHandleRegistryProbe(ClHandleRegistry &target);

    void set_references(const void *object, uint32_t references);
    /* The null object held, as no remember would hold it. */
    void hold_null();

    ClHandleRegistry &registry;
};

}  // namespace av

#endif  // AV_CL_HANDLE_REGISTRY_PROBE_HPP
