#ifndef AV_SURFACE_TABLE_HPP
#define AV_SURFACE_TABLE_HPP

#include "core/support/keyed_slots.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class BlockSource;
}

namespace av::icd {

struct SurfaceTableProbe;

struct SurfaceSlot {
    VkInstance instance_;
    void      *window_;
};

class SurfaceTable {
public:
    explicit SurfaceTable(BlockSource &memory);
    SurfaceTable(const SurfaceTable &) = delete;
    SurfaceTable &operator=(const SurfaceTable &) = delete;

    VkResult     create(VkInstance instance, void *window, VkSurfaceKHR &surface);
    void         destroy(VkInstance instance, VkSurfaceKHR surface);
    void         forget(VkInstance instance);
    void        *window_of(VkSurfaceKHR surface) const;
    uint32_t     live() const;

    bool invariants_hold() const;

private:
    KeyedSlots<uint64_t, SurfaceSlot> slots_;
    uint64_t                          next_;

    friend struct SurfaceTableProbe;
};

}

#endif
