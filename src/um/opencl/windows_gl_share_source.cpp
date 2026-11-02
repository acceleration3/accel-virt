#include "um/opencl/windows_gl_share_source.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

WindowsGlShareSource::WindowsGlShareSource() : current_(nullptr), proc_address_(nullptr)
{
    AV_SCOPE("WindowsGlShareSource::WindowsGlShareSource");
}

uint32_t WindowsGlShareSource::share_name(void *gl_context)
{
    AV_SCOPE("WindowsGlShareSource::share_name");
    if (gl_context == nullptr || !resolve() || current_() != static_cast<HGLRC>(gl_context)) return 0u;
    const auto named = reinterpret_cast<ShareName>(proc_address_("avGetShareName"));
    return named != nullptr ? named() : 0u;
}

void WindowsGlShareSource::run_held_calls()
{
    AV_SCOPE("WindowsGlShareSource::run_held_calls");
    if (!resolve() || current_() == nullptr) return;
    const auto run = reinterpret_cast<RunHeldCalls>(proc_address_("avRunHeldCalls"));
    if (run != nullptr) static_cast<void>(run());
}

bool WindowsGlShareSource::resolve()
{
    AV_SCOPE("WindowsGlShareSource::resolve");
    if (current_ != nullptr && proc_address_ != nullptr) return true;
    HMODULE const opengl32 = GetModuleHandleW(L"opengl32.dll");
    if (opengl32 == nullptr) return false;
    current_ = reinterpret_cast<CurrentContext>(GetProcAddress(opengl32, "wglGetCurrentContext"));
    proc_address_ = reinterpret_cast<ProcAddress>(GetProcAddress(opengl32, "wglGetProcAddress"));
    return current_ != nullptr && proc_address_ != nullptr;
}

}
