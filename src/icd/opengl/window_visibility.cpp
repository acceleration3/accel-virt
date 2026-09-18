#include "icd/opengl/window_visibility.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/common/window_surface.hpp"

namespace av::glicd {

WindowVisibility::WindowVisibility(icd::WindowSurface &surface, void *window) : surface_(surface), window_(window)
{
    AV_SCOPE("WindowVisibility::WindowVisibility");
}

bool WindowVisibility::showing() const
{
    AV_SCOPE("WindowVisibility::showing");
    if (window_ == nullptr) return false;
    uint32_t width = 0u, height = 0u;
    if (!surface_.client_size(window_, width, height)) return false;
    return width != 0u && height != 0u;
}

}  // namespace av::glicd
