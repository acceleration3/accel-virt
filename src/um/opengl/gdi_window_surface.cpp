#include "um/opengl/gdi_window_surface.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av {

void *GdiWindowSurface::window_of(void *dc)
{
    AV_SCOPE("GdiWindowSurface::window_of");
    return WindowFromDC(static_cast<HDC>(dc));
}

bool GdiWindowSurface::client_size(void *window, uint32_t &width, uint32_t &height)
{
    AV_SCOPE("GdiWindowSurface::client_size");
    RECT area = {};
    if (!GetClientRect(static_cast<HWND>(window), &area)) return false;
    width = static_cast<uint32_t>(area.right - area.left);
    height = static_cast<uint32_t>(area.bottom - area.top);
    return true;
}

bool GdiWindowSurface::blit(void *dc, uint32_t width, uint32_t height, const uint8_t *bgra_bottom_up)
{
    AV_SCOPE("GdiWindowSurface::blit");
    BITMAPINFO format = {};
    format.bmiHeader.biSize = sizeof format.bmiHeader;
    format.bmiHeader.biWidth = static_cast<LONG>(width);
    format.bmiHeader.biHeight = static_cast<LONG>(height);
    format.bmiHeader.biPlanes = 1u;
    format.bmiHeader.biBitCount = bits_;
    format.bmiHeader.biCompression = BI_RGB;
    return SetDIBitsToDevice(static_cast<HDC>(dc), 0, 0, width, height, 0, 0, 0u, height, bgra_bottom_up, &format,
                             DIB_RGB_COLORS) != 0;
}

}  // namespace av
