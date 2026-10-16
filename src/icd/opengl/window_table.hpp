/* The pixel format each window was given (data-model.md, research.md §7): set
 * once per window, as Windows allows, and asked for by every context create
 * and make current on a DC of that window, for as many windows as there are. A row
 * lasts as long as its window: an application may delete its last context and
 * make another on the same window, which keeps its format. */
#ifndef AV_GLICD_WINDOW_TABLE_HPP
#define AV_GLICD_WINDOW_TABLE_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class StateWriter;
}

namespace av::icd {
class WindowSurface;
}  // namespace av::icd

namespace av::glicd {

struct WindowTableProbe;

class WindowTable {
public:
    explicit WindowTable(BlockSource &memory);
    WindowTable(const WindowTable &) = delete;
    WindowTable &operator=(const WindowTable &) = delete;

    /* invalid_parameter for no window or a format below 1; conflict for a
     * window that has one, which it keeps; no_memory when there is no room. */
    [[nodiscard]] Status set_format(void *window, int32_t format);
    /* 0 for a window never given one. */
    int32_t              format_of(void *window) const;
    uint32_t             held() const;
    /* Forgets every window `surface` says no longer exists. */
    void                 drop_gone(icd::WindowSurface &surface);
    void                 report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    uint64_t key_of(void *window) const;

    /* Each window's format, by its HWND. */
    KeyedSlots<uint64_t, int32_t> formats_;

    friend struct WindowTableProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_WINDOW_TABLE_HPP
