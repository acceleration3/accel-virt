#ifndef AV_SURFACE_WINDOW_FRAME_HPP
#define AV_SURFACE_WINDOW_FRAME_HPP

#include "guest/probe/window_frame.hpp"

#include <cstdint>

namespace av {

class ScriptedWindowSurface;

class SurfaceWindowFrame final : public WindowFrame {
public:
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

}

#endif
