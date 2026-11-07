#include "executor/shim/egl_extensions.hpp"

#include "core/trace/function_scope.hpp"

#include <EGL/egl.h>
#include <string.h>

namespace av {

EglExtensions::EglExtensions() : extensions_(nullptr)
{
    AV_SCOPE("EglExtensions::EglExtensions");
}

void EglExtensions::configure(void *display)
{
    AV_SCOPE("EglExtensions::configure");
    extensions_ = display != nullptr ? eglQueryString(static_cast<EGLDisplay>(display), EGL_EXTENSIONS) : nullptr;
}

void EglExtensions::forget()
{
    AV_SCOPE("EglExtensions::forget");
    extensions_ = nullptr;
}

bool EglExtensions::lists(const char *wanted) const
{
    AV_SCOPE("EglExtensions::lists");
    if (extensions_ == nullptr) return false;
    const size_t length = strlen(wanted);
    for (const char *at = strstr(extensions_, wanted); at != nullptr; at = strstr(at + length, wanted))
        if ((at == extensions_ || at[-1] == ' ') && (at[length] == ' ' || at[length] == '\0')) return true;
    return false;
}

}
