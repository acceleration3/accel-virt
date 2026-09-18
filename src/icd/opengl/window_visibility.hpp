/* Whether a frame put on this window now would be seen (feature 051, US2.3).
 *
 * A minimized window's client area is empty, and so is one of a window being
 * destroyed; either way the read-back and the blit buy nothing, and skipping
 * them is what keeps a swapping application off the host GPU while nobody is
 * looking. A window merely covered by another is not visible either, and is
 * not detectable from an ICD - there is no occlusion to ask about without
 * DWM - so this does not pretend to know. */
#ifndef AV_GLICD_WINDOW_VISIBILITY_HPP
#define AV_GLICD_WINDOW_VISIBILITY_HPP

#include <stdint.h>

namespace av::icd {
class WindowSurface;
}

namespace av::glicd {

class WindowVisibility {
public:
    WindowVisibility(icd::WindowSurface &surface, void *window);
    WindowVisibility(const WindowVisibility &) = delete;
    WindowVisibility &operator=(const WindowVisibility &) = delete;

    /* False for a window that is gone, minimized, or empty in either
     * dimension. */
    bool showing() const;

private:
    icd::WindowSurface &surface_;
    void               *window_;
};

}  // namespace av::glicd

#endif  // AV_GLICD_WINDOW_VISIBILITY_HPP
