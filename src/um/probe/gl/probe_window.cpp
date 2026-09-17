#include "um/probe/gl/probe_window.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av {

namespace {

constexpr wchar_t window_class[] = L"rvkglprobe";

LRESULT CALLBACK probe_window_procedure(HWND window, UINT message, WPARAM wide, LPARAM lengthy)
{
    AV_SCOPE("probe_window_procedure");
    return DefWindowProcW(window, message, wide, lengthy);
}

}  // namespace

ProbeWindow::ProbeWindow()
    : instance_(GetModuleHandleW(nullptr)), window_(nullptr), dc_(nullptr), atom_(0u), last_error_(0u)
{
    AV_SCOPE("ProbeWindow::ProbeWindow");
}

ProbeWindow::~ProbeWindow()
{
    AV_SCOPE("ProbeWindow::~ProbeWindow");
    if (dc_ != nullptr) ReleaseDC(static_cast<HWND>(window_), static_cast<HDC>(dc_));
    if (window_ != nullptr) DestroyWindow(static_cast<HWND>(window_));
}

bool ProbeWindow::register_class()
{
    AV_SCOPE("ProbeWindow::register_class");
    WNDCLASSEXW description = {};
    description.cbSize = sizeof description;
    description.style = CS_OWNDC;
    description.lpfnWndProc = &probe_window_procedure;
    description.hInstance = static_cast<HINSTANCE>(instance_);
    description.lpszClassName = window_class;
    SetLastError(0u);
    atom_ = RegisterClassExW(&description);
    last_error_ = GetLastError();
    return atom_ != 0u;
}

bool ProbeWindow::create()
{
    AV_SCOPE("ProbeWindow::create");
    SetLastError(0u);
    window_ = CreateWindowExW(0u, window_class, L"rvkglprobe", WS_OVERLAPPEDWINDOW, 0, 0, static_cast<int>(side),
                              static_cast<int>(side), nullptr, nullptr, static_cast<HINSTANCE>(instance_), nullptr);
    last_error_ = GetLastError();
    return window_ != nullptr;
}

bool ProbeWindow::get_dc()
{
    AV_SCOPE("ProbeWindow::get_dc");
    SetLastError(0u);
    dc_ = GetDC(static_cast<HWND>(window_));
    last_error_ = GetLastError();
    return dc_ != nullptr;
}

bool ProbeWindow::open()
{
    AV_SCOPE("ProbeWindow::open");
    return register_class() && create() && get_dc();
}

void *ProbeWindow::window() const
{
    AV_SCOPE("ProbeWindow::window");
    return window_;
}

void *ProbeWindow::dc() const
{
    AV_SCOPE("ProbeWindow::dc");
    return dc_;
}

uint16_t ProbeWindow::atom() const
{
    AV_SCOPE("ProbeWindow::atom");
    return atom_;
}

uint32_t ProbeWindow::last_error() const
{
    AV_SCOPE("ProbeWindow::last_error");
    return last_error_;
}

}  // namespace av
