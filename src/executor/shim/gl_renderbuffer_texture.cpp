#include "executor/shim/gl_renderbuffer_texture.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/gl_context.hpp"
#include "executor/shim/gl_display.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace av {

namespace {

using CreateImage = void *(*)(void *, void *, unsigned, void *, const int32_t *);
using DestroyImage = unsigned (*)(void *, void *);

}

GlRenderbufferTexture::GlRenderbufferTexture()
    : create_image_(nullptr), destroy_image_(nullptr), image_target_texture_(nullptr), gen_textures_(nullptr),
      bind_texture_(nullptr), delete_textures_(nullptr), errors_(), context_(nullptr), display_(nullptr)
{
    AV_SCOPE("GlRenderbufferTexture::GlRenderbufferTexture");
}

void GlRenderbufferTexture::configure(GlContext &context, const GlDisplay &display)
{
    AV_SCOPE("GlRenderbufferTexture::configure");
    context_ = &context;
    display_ = &display;
    if (!context.make_current()) return;
    create_image_ = reinterpret_cast<CreateImage>(context.resolve("eglCreateImageKHR"));
    destroy_image_ = reinterpret_cast<DestroyImage>(context.resolve("eglDestroyImageKHR"));
    image_target_texture_ =
        reinterpret_cast<void(APIENTRY *)(GLenum, void *)>(context.resolve("glEGLImageTargetTexture2DOES"));
    gen_textures_ = reinterpret_cast<void(APIENTRY *)(GLsizei, GLuint *)>(context.resolve("glGenTextures"));
    bind_texture_ = reinterpret_cast<void(APIENTRY *)(GLenum, GLuint)>(context.resolve("glBindTexture"));
    delete_textures_ =
        reinterpret_cast<void(APIENTRY *)(GLsizei, const GLuint *)>(context.resolve("glDeleteTextures"));
    errors_.resolve(context);
}

void GlRenderbufferTexture::give_back()
{
    AV_SCOPE("GlRenderbufferTexture::give_back");
    context_ = nullptr;
    display_ = nullptr;
}

bool GlRenderbufferTexture::makes_textures() const
{
    AV_SCOPE("GlRenderbufferTexture::makes_textures");
    return create_image_ != nullptr && destroy_image_ != nullptr && image_target_texture_ != nullptr &&
           gen_textures_ != nullptr && bind_texture_ != nullptr && delete_textures_ != nullptr &&
           errors_.resolved() && display_ != nullptr;
}

GlContext *GlRenderbufferTexture::current()
{
    AV_SCOPE("GlRenderbufferTexture::current");
    if (context_ == nullptr || !context_->make_current()) return nullptr;
    return context_;
}

bool GlRenderbufferTexture::make(uint32_t renderbuffer, uint32_t &texture, void *&image)
{
    AV_SCOPE("GlRenderbufferTexture::make");
    if (!makes_textures() || renderbuffer == 0u || current() == nullptr) return false;
    void *const made = create_image_(display_->handle(), context_->handle(), EGL_GL_RENDERBUFFER_KHR,
                                     reinterpret_cast<void *>(static_cast<uintptr_t>(renderbuffer)), nullptr);
    if (made == nullptr) return false;
    GLuint over = 0u;
    gen_textures_(1, &over);
    static_cast<void>(errors_.clear());
    bind_texture_(texture_2d, over);
    image_target_texture_(texture_2d, made);
    const bool taken = errors_.clear();
    bind_texture_(texture_2d, 0u);
    if (!taken) {
        delete_textures_(1, &over);
        static_cast<void>(destroy_image_(display_->handle(), made));
        static_cast<void>(errors_.clear());
        return false;
    }
    texture = over;
    image = made;
    return true;
}

void GlRenderbufferTexture::destroy(uint32_t texture, void *image)
{
    AV_SCOPE("GlRenderbufferTexture::destroy");
    if (!makes_textures() || current() == nullptr) return;
    const GLuint named = texture;
    if (named != 0u) delete_textures_(1, &named);
    static_cast<void>(errors_.clear());
    if (image != nullptr) static_cast<void>(destroy_image_(display_->handle(), image));
}

}
