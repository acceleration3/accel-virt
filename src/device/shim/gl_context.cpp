#include "device/shim/gl_context.hpp"

#include "device/shim/gl_display.hpp"

#include <EGL/egl.h>

namespace av {

GlContext::GlContext() : display_(nullptr), context_(nullptr) {}

GlContext::~GlContext() { close(); }

bool GlContext::open() const { return context_ != nullptr; }

bool GlContext::open(const GlDisplay &display)
{
    if (context_ != nullptr) return false;
    void *const context = display.create_context();
    if (context == nullptr) return false;
    if (!display.make_current(context)) {
        display.destroy_context(context);
        return false;
    }
    display_ = &display;
    context_ = context;
    return true;
}

bool GlContext::make_current()
{
    if (context_ == nullptr) return false;
    return display_->make_current(context_);
}

void *GlContext::resolve(const char *name) const
{
    if (context_ == nullptr) return nullptr;
    return reinterpret_cast<void *>(eglGetProcAddress(name));
}

void GlContext::close()
{
    if (context_ == nullptr) return;
    display_->destroy_context(context_);
    display_ = nullptr;
    context_ = nullptr;
}

}  // namespace av
