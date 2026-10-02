/* The probe's window on the full-stack rig (feature 046): the ScriptedWindowSurface
 * the ICD blits onto, read back the way a desktop is read back in the guest.
 * What the probe judges is still what the driver put on the window - here the
 * last blit it made, since no desktop holds it. */
#ifndef AV_SURFACE_WINDOW_FRAME_HPP
#define AV_SURFACE_WINDOW_FRAME_HPP

#include "guest/probe/window_frame.hpp"

#include <cstdint>

namespace av {

class ScriptedWindowSurface;

class SurfaceWindowFrame final : public WindowFrame {
public:
    /* `windows` is the ICD's window surface, and `window` the window the probe
     * was given, of `width` x `height`. */
    SurfaceWindowFrame(ScriptedWindowSurface &windows, void *window, std::uint32_t width, std::uint32_t height);

    bool size(void *window, std::uint32_t &width, std::uint32_t &height) override;
    bool read(void *window, std::uint32_t width, std::uint32_t height, std::uint8_t *bgra) override;
    bool resize(void *window, std::uint32_t width, std::uint32_t height) override;

private:
    ScriptedWindowSurface &windows_;
    void                  *window_;
    std::uint32_t          width_;
    std::uint32_t          height_;
};

}  // namespace av

#endif  // AV_SURFACE_WINDOW_FRAME_HPP
