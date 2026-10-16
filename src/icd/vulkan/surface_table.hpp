/* The surfaces vkCreateWin32SurfaceKHR made (feature 046): a window and the
 * instance it was made for, as many as an application makes. A surface's
 * handle is a number minted for it and never minted again, so a destroyed
 * surface's handle names nothing. */
#ifndef AV_SURFACE_TABLE_HPP
#define AV_SURFACE_TABLE_HPP

#include "core/support/keyed_slots.hpp"

#include <stdint.h>

#include <vulkan/vulkan_core.h>

namespace av {
class BlockSource;
}  // namespace av

namespace av::icd {

struct SurfaceTableProbe;

/* One surface: the instance it was made for and the window it is on. A type of
 * its own so the table's slots are a complete type (tools/header_scan.py reads
 * a class's members only up to its first nested type). */
struct SurfaceSlot {
    VkInstance instance_;
    void      *window_;
};

class SurfaceTable {
public:
    explicit SurfaceTable(BlockSource &memory);
    SurfaceTable(const SurfaceTable &) = delete;
    SurfaceTable &operator=(const SurfaceTable &) = delete;

    /* VK_ERROR_INITIALIZATION_FAILED for no window;
     * VK_ERROR_OUT_OF_HOST_MEMORY when there is no memory to keep it. */
    VkResult     create(VkInstance instance, void *window, VkSurfaceKHR &surface);
    /* A handle this table did not make, or one destroyed, is ignored. */
    void         destroy(VkInstance instance, VkSurfaceKHR surface);
    /* Every surface of `instance`, as its destruction does. */
    void         forget(VkInstance instance);
    /* The window of a live surface; null otherwise. */
    void        *window_of(VkSurfaceKHR surface) const;
    uint32_t     live() const;

    bool invariants_hold() const;

private:
    /* Each surface by its handle's bits. */
    KeyedSlots<uint64_t, SurfaceSlot> slots_;
    uint64_t                          next_;

    friend struct SurfaceTableProbe;
};

}  // namespace av::icd

#endif  // AV_SURFACE_TABLE_HPP
