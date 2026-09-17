/* The surfaces vkCreateWin32SurfaceKHR made (feature 046): a window and the
 * instance it was made for; a surface's handle is its slot's address. */
#ifndef AV_SURFACE_TABLE_HPP
#define AV_SURFACE_TABLE_HPP

#include <stdint.h>

#include <vulkan/vulkan_core.h>

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
    static constexpr uint32_t max_surfaces = 32u;

    SurfaceTable();
    SurfaceTable(const SurfaceTable &) = delete;
    SurfaceTable &operator=(const SurfaceTable &) = delete;

    /* VK_ERROR_INITIALIZATION_FAILED for no window;
     * VK_ERROR_OUT_OF_HOST_MEMORY when every slot is taken. */
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
    const SurfaceSlot *slot_of(VkSurfaceKHR surface) const;

    SurfaceSlot slots_[max_surfaces];

    friend struct SurfaceTableProbe;
};

}  // namespace av::icd

#endif  // AV_SURFACE_TABLE_HPP
