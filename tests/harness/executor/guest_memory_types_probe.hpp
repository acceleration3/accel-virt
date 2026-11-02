#ifndef AV_GUEST_MEMORY_TYPES_PROBE_HPP
#define AV_GUEST_MEMORY_TYPES_PROBE_HPP

#include "executor/vulkan/guest_memory_types.hpp"

#include <stdint.h>

namespace av {

struct GuestMemoryTypesProbe {
    explicit GuestMemoryTypesProbe(GuestMemoryTypes &target);

    void set_type_count(uint32_t count);
    void set_heap_count(uint32_t count);
    void set_host_types(uint32_t count);
    void set_backed(uint32_t mask);
    void set_host(uint32_t guest, uint32_t host);
    void set_flags(uint32_t guest, VkMemoryPropertyFlags flags);

    GuestMemoryTypes &types;
};

}

#endif
