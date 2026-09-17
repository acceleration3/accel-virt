/* The pixel format each window was given (data-model.md, research.md §7): set
 * once per window, as Windows allows, and asked for by every context create
 * and make current on a DC of that window. At most max_windows windows. */
#ifndef AV_GLICD_WINDOW_TABLE_HPP
#define AV_GLICD_WINDOW_TABLE_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class StateWriter;
}

namespace av::glicd {

struct WindowTableProbe;

class WindowTable {
public:
    static constexpr uint32_t max_windows = 256u;

    WindowTable();
    WindowTable(const WindowTable &) = delete;
    WindowTable &operator=(const WindowTable &) = delete;

    /* invalid_parameter for no window or a format below 1; conflict for a
     * window that has one, which it keeps; no_space with max_windows held. */
    [[nodiscard]] Status set_format(void *window, int32_t format);
    /* 0 for a window never given one. */
    int32_t              format_of(void *window) const;
    uint32_t             held() const;
    void                 report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    void    *windows_[max_windows];
    int32_t  formats_[max_windows];
    uint32_t count_;

    friend struct WindowTableProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_WINDOW_TABLE_HPP
