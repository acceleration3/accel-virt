#include "um/opengl/loaded_opengl32.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av {

void *LoadedOpengl32::create_context(void *dc)
{
    AV_SCOPE("LoadedOpengl32::create_context");
    using CreateContext = HGLRC(WINAPI *)(HDC);
    const HMODULE opengl32 = GetModuleHandleW(L"opengl32.dll");
    const FARPROC create = opengl32 != nullptr ? GetProcAddress(opengl32, export_) : nullptr;
    return create != nullptr ? reinterpret_cast<CreateContext>(create)(static_cast<HDC>(dc)) : nullptr;
}

}
