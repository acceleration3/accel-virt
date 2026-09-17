#include "um/opengl/gl_discovery.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

#include <windows.h>

namespace av {

namespace {

constexpr const char *entry_names[] = {
    "DrvValidateVersion",   "DrvSetCallbackProcs", "DrvDescribePixelFormat", "DrvSetPixelFormat",
    "DrvCreateContext",     "DrvCreateLayerContext", "DrvDeleteContext",     "DrvSetContext",
    "DrvReleaseContext",    "DrvCopyContext",      "DrvShareLists",          "DrvGetProcAddress",
    "DrvDescribeLayerPlane", "DrvSwapLayerBuffers", "DrvSwapBuffers",
};
constexpr uint32_t line_bytes = 192u;
constexpr uint32_t argument_digits = 16u;
/* PFD_SUPPORT_COMPOSITION: a format without it is refused a DWM window. */
constexpr DWORD supports_composition = 0x00008000u;

}  // namespace

GlDiscovery::Seams::Seams() : log(L"rvkgl"), trace()
{
    AV_SCOPE("GlDiscovery::Seams::Seams");
    log.open();
}

GlDiscovery::Seams *GlDiscovery::seams()
{
    AV_SCOPE("GlDiscovery::seams");
    if (seams_.constructed()) return &seams_.object();
    if (seams_.construct() != Status::ok) return nullptr;
    Seams &made = seams_.object();
    if (made.trace.start("rvkgl", *this, crashed_) != Status::ok)
        made.log.write("rvkgl: tracing is off: its section, pin or service failed");
    return &made;
}

void GlDiscovery::called(Entry entry, uint64_t argument)
{
    AV_SCOPE("GlDiscovery::called");
    const auto index = static_cast<uint32_t>(entry);
    calls_[index]++;
    Seams *const opened = seams();
    if (opened == nullptr) return;
    char       bytes[line_bytes];
    TextBuffer line(bytes, sizeof bytes);
    line.append(entry_names[index]);
    line.append(" 0x");
    line.append_hex(argument, argument_digits);
    opened->log.write(line.text());
}

int32_t GlDiscovery::validate(uint32_t version)
{
    AV_SCOPE("GlDiscovery::validate");
    called(Entry::validate_version, version);
    return version == driver_version ? TRUE : FALSE;
}

int32_t GlDiscovery::answer(Entry entry, uint64_t argument)
{
    AV_SCOPE("GlDiscovery::answer");
    called(entry, argument);
    return entry == Entry::set_pixel_format ? TRUE : FALSE;
}

void *GlDiscovery::decline(Entry entry, uint64_t argument)
{
    AV_SCOPE("GlDiscovery::decline");
    called(entry, argument);
    return nullptr;
}

void *GlDiscovery::look_up(const char *name)
{
    AV_SCOPE("GlDiscovery::look_up");
    calls_[static_cast<uint32_t>(Entry::get_proc_address)]++;
    Seams *const opened = seams();
    if (opened == nullptr) return nullptr;
    char       bytes[line_bytes];
    TextBuffer line(bytes, sizeof bytes);
    line.append("DrvGetProcAddress ");
    line.append(name != nullptr ? name : "(null)");
    opened->log.write(line.text());
    return nullptr;
}

int32_t GlDiscovery::describe(uint64_t dc, int32_t format, uint32_t bytes, void *descriptor)
{
    AV_SCOPE("GlDiscovery::describe");
    called(Entry::describe_pixel_format, dc);
    if (format != formats || descriptor == nullptr || bytes < sizeof(PIXELFORMATDESCRIPTOR)) return formats;
    auto *const pfd = static_cast<PIXELFORMATDESCRIPTOR *>(descriptor);
    *pfd = PIXELFORMATDESCRIPTOR{};
    pfd->nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd->nVersion = 1u;
    pfd->dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | supports_composition;
    pfd->iPixelType = PFD_TYPE_RGBA;
    pfd->cColorBits = 32u;
    pfd->cRedBits = 8u;
    pfd->cRedShift = 16u;
    pfd->cGreenBits = 8u;
    pfd->cGreenShift = 8u;
    pfd->cBlueBits = 8u;
    pfd->cBlueShift = 0u;
    pfd->cAlphaBits = 8u;
    pfd->cAlphaShift = 24u;
    pfd->cDepthBits = 24u;
    pfd->cStencilBits = 8u;
    pfd->iLayerType = PFD_MAIN_PLANE;
    return formats;
}

long GlDiscovery::crashed(void *pointers)
{
    AV_SCOPE("GlDiscovery::crashed");
    return seams_.constructed() ? seams_.object().trace.crashed(pointers) : 0;
}

void GlDiscovery::report(StateWriter &writer)
{
    AV_SCOPE("GlDiscovery::report");
    writer.open("discovery", 0u);
    for (uint32_t index = 0u; index < static_cast<uint32_t>(Entry::count); index++)
        writer.number(entry_names[index], calls_[index]);
    writer.close();
}

}  // namespace av
