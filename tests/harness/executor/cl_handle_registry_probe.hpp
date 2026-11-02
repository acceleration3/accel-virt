#ifndef AV_CL_HANDLE_REGISTRY_PROBE_HPP
#define AV_CL_HANDLE_REGISTRY_PROBE_HPP

#include "executor/opencl/cl_handle_registry.hpp"

#include <stdint.h>

namespace av {

struct ClHandleRegistryProbe {
    explicit ClHandleRegistryProbe(ClHandleRegistry &target);

    void set_references(const void *object, uint32_t references);
    void hold_null();

    ClHandleRegistry &registry;
};

}

#endif
