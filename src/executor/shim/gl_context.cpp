#include "executor/shim/gl_context.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/gl_display.hpp"

#include <EGL/egl.h>

namespace av {

GlContext::GlContext() : display_(nullptr), context_(nullptr)
{
    AV_SCOPE("GlContext::GlContext");
}

GlContext::~GlContext()
{
    AV_SCOPE("GlContext::~GlContext");
    close();
}

bool GlContext::open() const
{
    AV_SCOPE("GlContext::open");
    return context_ != nullptr;
}

bool GlContext::open(const GlDisplay &display, const GlContextAttributes &attributes, const GlContext *share)
{
    AV_SCOPE("GlContext::open");
    if (context_ != nullptr) return false;
    if (share != nullptr && (share->context_ == nullptr || share->display_ != &display)) return false;
    void *const context = display.create_context(attributes, share != nullptr ? share->context_ : nullptr);
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
    AV_SCOPE("GlContext::make_current");
    if (context_ == nullptr) return false;
    if (eglGetCurrentContext() == static_cast<EGLContext>(context_)) return true;
    return display_->make_current(context_);
}

void *GlContext::handle() const
{
    AV_SCOPE("GlContext::handle");
    return context_;
}

void *GlContext::display() const
{
    AV_SCOPE("GlContext::display");
    return display_ != nullptr ? display_->handle() : nullptr;
}

void *GlContext::resolve(const char *name) const
{
    AV_SCOPE("GlContext::resolve");
    if (context_ == nullptr) return nullptr;
    return reinterpret_cast<void *>(eglGetProcAddress(name));
}

void GlContext::close()
{
    AV_SCOPE("GlContext::close");
    if (context_ == nullptr) return;
    display_->destroy_context(context_);
    display_ = nullptr;
    context_ = nullptr;
}

}
