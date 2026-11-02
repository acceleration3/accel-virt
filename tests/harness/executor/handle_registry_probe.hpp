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
    void     hold_null();

    HandleRegistry &registry;
};

}

#endif
