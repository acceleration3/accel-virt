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

constexpr size_t   name_chars = 128u;
constexpr uint32_t hex_digits = 16u;

}  // namespace

GlDiscoveryProbe::GlDiscoveryProbe()
    : bytes_{}, report_(bytes_, sizeof bytes_), window_(), format_(0)
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
    line("avgl_loaded", GetModuleHandleW(L"avgl.dll") != nullptr ? "yes" : "no", 0u, 0u);
    report_new_modules(before, before_count);
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
    const bool registered = window_.register_class();
    line("register_class", registered ? "ok" : "failed", window_.atom(), window_.last_error());
    if (!registered) return false;
    const bool created = window_.create();
    line("create_window", created ? "ok" : "failed",
         static_cast<uint64_t>(reinterpret_cast<uintptr_t>(window_.window())), window_.last_error());
    if (!created) return false;
    const bool got = window_.get_dc();
    line("get_dc", got ? "ok" : "failed", static_cast<uint64_t>(reinterpret_cast<uintptr_t>(window_.dc())),
         window_.last_error());
    return got;
}

void GlDiscoveryProbe::open_adapter()
{
    AV_SCOPE("GlDiscoveryProbe::open_adapter");
    D3DKMT_OPENADAPTERFROMHDC open = {};
    open.hDc = static_cast<HDC>(window_.dc());
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
    const int count = DescribePixelFormat(static_cast<HDC>(window_.dc()), 1, sizeof pfd, &pfd);
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
    const BOOL set = SetPixelFormat(static_cast<HDC>(window_.dc()), format_ != 0 ? format_ : 1, &pfd);
    line("set_pixel_format", set ? "ok" : "failed", static_cast<uint64_t>(set), GetLastError());
}

void GlDiscoveryProbe::create_context()
{
    AV_SCOPE("GlDiscoveryProbe::create_context");
    SetLastError(0u);
    const HGLRC context = wglCreateContext(static_cast<HDC>(window_.dc()));
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
