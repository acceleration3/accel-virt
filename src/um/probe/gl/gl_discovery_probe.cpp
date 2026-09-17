#include "um/probe/gl/gl_discovery_probe.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>
typedef _Return_type_success_(return >= 0) LONG NTSTATUS;
#include <d3dkmthk.h>

extern "C" {
BOOL WINAPI K32EnumProcessModules(HANDLE process, HMODULE *modules, DWORD bytes, LPDWORD needed);
DWORD WINAPI K32GetModuleBaseNameW(HANDLE process, HMODULE module, LPWSTR name, DWORD size);
}

namespace av {

namespace {

constexpr wchar_t window_class[] = L"rvkglprobe";
constexpr int     window_side = 64;
constexpr size_t  name_chars = 128u;
constexpr uint32_t hex_digits = 16u;

LRESULT CALLBACK discovery_window(HWND window, UINT message, WPARAM wide, LPARAM lengthy)
{
    AV_SCOPE("discovery_window");
    return DefWindowProcW(window, message, wide, lengthy);
}

}  // namespace

GlDiscoveryProbe::GlDiscoveryProbe()
    : bytes_{}, report_(bytes_, sizeof bytes_), window_(nullptr), dc_(nullptr), format_(0)
{
    AV_SCOPE("GlDiscoveryProbe::GlDiscoveryProbe");
}

uint32_t GlDiscoveryProbe::run()
{
    AV_SCOPE("GlDiscoveryProbe::run");
    void    *before[max_modules] = {};
    uint32_t before_count = 0u;
    modules(before, before_count);
    if (!make_window()) {
        write();
        return 1u;
    }
    open_adapter();
    describe();
    set_format();
    create_context();
    line("rvkgl_loaded", GetModuleHandleW(L"rvkgl.dll") != nullptr ? "yes" : "no", 0u, 0u);
    report_new_modules(before, before_count);
    ReleaseDC(static_cast<HWND>(window_), static_cast<HDC>(dc_));
    DestroyWindow(static_cast<HWND>(window_));
    write();
    return 0u;
}

void GlDiscoveryProbe::line(const char *key, const char *text, uint64_t value, uint64_t error)
{
    AV_SCOPE("GlDiscoveryProbe::line");
    report_.append("discovery ");
    report_.append(key);
    report_.append(" ");
    report_.append(text);
    report_.append(" value=0x");
    report_.append_hex(value, hex_digits);
    report_.append(" error=");
    report_.append_decimal(error);
    report_.append("\r\n");
}

void GlDiscoveryProbe::modules(void **list, uint32_t &count) const
{
    AV_SCOPE("GlDiscoveryProbe::modules");
    DWORD needed = 0u;
    count = 0u;
    if (!K32EnumProcessModules(GetCurrentProcess(), static_cast<HMODULE *>(static_cast<void *>(list)),
                               static_cast<DWORD>(max_modules * sizeof(HMODULE)), &needed))
        return;
    count = static_cast<uint32_t>(needed / sizeof(HMODULE));
    if (count > max_modules) count = max_modules;
}

/* Every module the pixel-format and context calls brought in: the ICD
 * opengl32 chose, whatever it is called. */
void GlDiscoveryProbe::report_new_modules(void *const *before, uint32_t before_count)
{
    AV_SCOPE("GlDiscoveryProbe::report_new_modules");
    void    *after[max_modules] = {};
    uint32_t after_count = 0u;
    modules(after, after_count);
    for (uint32_t index = 0u; index < after_count; index++) {
        bool known = false;
        for (uint32_t earlier = 0u; earlier < before_count && !known; earlier++) known = before[earlier] == after[index];
        if (known) continue;
        wchar_t name[name_chars] = {};
        static_cast<void>(K32GetModuleBaseNameW(GetCurrentProcess(), static_cast<HMODULE>(after[index]), name,
                                                static_cast<DWORD>(name_chars - 1u)));
        char narrow[name_chars] = {};
        for (size_t at = 0u; at + 1u < name_chars && name[at] != L'\0'; at++)
            narrow[at] = name[at] < 0x80 ? static_cast<char>(name[at]) : '?';
        line("loaded", narrow, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(after[index])), 0u);
    }
}

bool GlDiscoveryProbe::make_window()
{
    AV_SCOPE("GlDiscoveryProbe::make_window");
    WNDCLASSEXW description = {};
    description.cbSize = sizeof description;
    description.style = CS_OWNDC;
    description.lpfnWndProc = &discovery_window;
    description.hInstance = GetModuleHandleW(nullptr);
    description.lpszClassName = window_class;
    SetLastError(0u);
    const ATOM registered = RegisterClassExW(&description);
    line("register_class", registered != 0u ? "ok" : "failed", registered, GetLastError());
    if (registered == 0u) return false;
    SetLastError(0u);
    window_ = CreateWindowExW(0u, window_class, L"rvkglprobe", WS_OVERLAPPEDWINDOW, 0, 0, window_side, window_side,
                              nullptr, nullptr, description.hInstance, nullptr);
    line("create_window", window_ != nullptr ? "ok" : "failed",
         static_cast<uint64_t>(reinterpret_cast<uintptr_t>(window_)), GetLastError());
    if (window_ == nullptr) return false;
    dc_ = GetDC(static_cast<HWND>(window_));
    line("get_dc", dc_ != nullptr ? "ok" : "failed", static_cast<uint64_t>(reinterpret_cast<uintptr_t>(dc_)),
         GetLastError());
    return dc_ != nullptr;
}

void GlDiscoveryProbe::open_adapter()
{
    AV_SCOPE("GlDiscoveryProbe::open_adapter");
    D3DKMT_OPENADAPTERFROMHDC open = {};
    open.hDc = static_cast<HDC>(dc_);
    const NTSTATUS status = D3DKMTOpenAdapterFromHdc(&open);
    const uint64_t luid = (static_cast<uint64_t>(static_cast<uint32_t>(open.AdapterLuid.HighPart)) << 32u) |
                          open.AdapterLuid.LowPart;
    line("open_adapter_from_hdc", status >= 0 ? "ok" : "failed", luid, static_cast<uint32_t>(status));
    if (status < 0) return;
    D3DKMT_CLOSEADAPTER close = {};
    close.hAdapter = open.hAdapter;
    static_cast<void>(D3DKMTCloseAdapter(&close));
}

void GlDiscoveryProbe::describe()
{
    AV_SCOPE("GlDiscoveryProbe::describe");
    PIXELFORMATDESCRIPTOR pfd = {};
    SetLastError(0u);
    const int count = DescribePixelFormat(static_cast<HDC>(dc_), 1, sizeof pfd, &pfd);
    line("describe_pixel_format", count > 0 ? "ok" : "failed", static_cast<uint64_t>(count), GetLastError());
    line("format_1_flags", "pfd", pfd.dwFlags, pfd.cDepthBits);
    format_ = count > 0 ? 1 : 0;
}

void GlDiscoveryProbe::set_format()
{
    AV_SCOPE("GlDiscoveryProbe::set_format");
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof pfd;
    pfd.nVersion = 1u;
    SetLastError(0u);
    const BOOL set = SetPixelFormat(static_cast<HDC>(dc_), format_ != 0 ? format_ : 1, &pfd);
    line("set_pixel_format", set ? "ok" : "failed", static_cast<uint64_t>(set), GetLastError());
}

void GlDiscoveryProbe::create_context()
{
    AV_SCOPE("GlDiscoveryProbe::create_context");
    SetLastError(0u);
    const HGLRC context = wglCreateContext(static_cast<HDC>(dc_));
    line("create_context", context != nullptr ? "ok" : "failed",
         static_cast<uint64_t>(reinterpret_cast<uintptr_t>(context)), GetLastError());
    if (context != nullptr) static_cast<void>(wglDeleteContext(context));
}

/* A report the console does not take is still the exit code. */
void GlDiscoveryProbe::write() const
{
    AV_SCOPE("GlDiscoveryProbe::write");
    DWORD written = 0u;
    static_cast<void>(WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), report_.text(), static_cast<DWORD>(report_.length()),
                                &written, nullptr));
}

}  // namespace av
