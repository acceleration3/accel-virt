#include "um/probe/gdi_window_frame.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av {

bool GdiWindowFrame::size(void *window, uint32_t &width, uint32_t &height)
{
    AV_SCOPE("GdiWindowFrame::size");
    RECT client = {};
    if (GetClientRect(static_cast<HWND>(window), &client) == 0) return false;
    width = static_cast<uint32_t>(client.right - client.left);
    height = static_cast<uint32_t>(client.bottom - client.top);
    return true;
}

bool GdiWindowFrame::read(void *window, uint32_t width, uint32_t height, uint8_t *bgra)
{
    AV_SCOPE("GdiWindowFrame::read");
    const HDC memory_dc = CreateCompatibleDC(nullptr);
    if (memory_dc == nullptr) return false;
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof info.bmiHeader;
    info.bmiHeader.biWidth = static_cast<LONG>(width);
    info.bmiHeader.biHeight = static_cast<LONG>(height);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = static_cast<WORD>(bits_);
    info.bmiHeader.biCompression = BI_RGB;
    void         *pixels = nullptr;
    const HBITMAP bitmap = CreateDIBSection(memory_dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0u);
    bool          read_back = false;
    if (bitmap != nullptr) {
        const HGDIOBJ was = SelectObject(memory_dc, bitmap);
        read_back = PrintWindow(static_cast<HWND>(window), memory_dc, PW_CLIENTONLY | PW_RENDERFULLCONTENT) != 0;
        if (read_back) __builtin_memcpy(bgra, pixels, static_cast<size_t>(width) * height * 4u);
        static_cast<void>(SelectObject(memory_dc, was));
        static_cast<void>(DeleteObject(bitmap));
    }
    static_cast<void>(DeleteDC(memory_dc));
    return read_back;
}

bool GdiWindowFrame::resize(void *window, uint32_t width, uint32_t height)
{
    AV_SCOPE("GdiWindowFrame::resize");
    return SetWindowPos(static_cast<HWND>(window), nullptr, 0, 0, static_cast<int>(width), static_cast<int>(height),
                        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != 0;
}

}
