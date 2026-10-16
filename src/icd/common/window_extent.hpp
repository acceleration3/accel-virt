/* Judges one window's client area against the extent a presenter holds:
 * whether the window is gone, has no area (minimized, or being destroyed), is
 * another size than the one held, or the same. What both ICDs ask before a
 * present and when they forget a window - Vulkan to answer SURFACE_LOST or
 * OUT_OF_DATE, OpenGL to skip a frame nobody can see and to drop what it made
 * for a window that is gone.
 *
 * A window merely covered by another is not detectable from an ICD - there is
 * no occlusion to ask about without DWM - so it is not judged. */
#ifndef AV_ICD_WINDOW_EXTENT_HPP
#define AV_ICD_WINDOW_EXTENT_HPP

#include <stdint.h>

namespace av::icd {

class WindowSurface;

class WindowExtent {
public:
    enum class Look {
        gone,
        /* No area in one dimension or both. */
        empty,
        resized,
        same,
    };

    /* A window no presenter holds an extent for yet: any area is resized. */
    WindowExtent(WindowSurface &surface, void *window);
    /* `window`, held at `width` x `height`. */
    WindowExtent(WindowSurface &surface, void *window, uint32_t width, uint32_t height);
    WindowExtent(const WindowExtent &) = delete;
    WindowExtent &operator=(const WindowExtent &) = delete;

    /* The window as it is now; a null window is gone. */
    Look     look();
    /* Its client area as look() last found it; 0 when gone. */
    uint32_t width() const;
    uint32_t height() const;

private:
    WindowSurface &surface_;
    void          *window_;
    uint32_t       held_width_;
    uint32_t       held_height_;
    uint32_t       width_;
    uint32_t       height_;
};

}  // namespace av::icd

#endif  // AV_ICD_WINDOW_EXTENT_HPP
