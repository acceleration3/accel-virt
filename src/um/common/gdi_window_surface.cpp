#include "um/common/gdi_window_surface.hpp"

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
    return draw(dc, width, static_cast<int32_t>(height), bgra_bottom_up);
}

bool GdiWindowSurface::blit_window(void *window, uint32_t width, uint32_t height, const uint8_t *bgra_bottom_up)
{
    AV_SCOPE("GdiWindowSurface::blit_window");
    return draw_on_window(window, width, static_cast<int32_t>(height), bgra_bottom_up);
}

bool GdiWindowSurface::blit_window_top_down(void *window, uint32_t width, uint32_t height,
                                            const uint8_t *bgra_top_down)
{
    AV_SCOPE("GdiWindowSurface::blit_window_top_down");
    return draw_on_window(window, width, -static_cast<int32_t>(height), bgra_top_down);
}

bool GdiWindowSurface::draw_on_window(void *window, uint32_t width, int32_t dib_height, const uint8_t *rows)
{
    AV_SCOPE("GdiWindowSurface::draw_on_window");
    const HDC dc = GetDC(static_cast<HWND>(window));
    if (dc == nullptr) return false;
    const bool drawn = draw(dc, width, dib_height, rows);
    static_cast<void>(ReleaseDC(static_cast<HWND>(window), dc));
    return drawn;
}

bool GdiWindowSurface::draw(void *dc, uint32_t width, int32_t dib_height, const uint8_t *rows)
{
    AV_SCOPE("GdiWindowSurface::draw");
    const auto lines = static_cast<DWORD>(dib_height < 0 ? -dib_height : dib_height);
    BITMAPINFO format = {};
    format.bmiHeader.biSize = sizeof format.bmiHeader;
    format.bmiHeader.biWidth = static_cast<LONG>(width);
    format.bmiHeader.biHeight = static_cast<LONG>(dib_height);
    format.bmiHeader.biPlanes = 1u;
    format.bmiHeader.biBitCount = bits_;
    format.bmiHeader.biCompression = BI_RGB;
    return SetDIBitsToDevice(static_cast<HDC>(dc), 0, 0, width, lines, 0, 0, 0u, lines, rows, &format,
                             DIB_RGB_COLORS) != 0;
}

}
