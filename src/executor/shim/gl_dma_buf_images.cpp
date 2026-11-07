#include "executor/shim/gl_dma_buf_images.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/sharing/shared_image_shape.hpp"
#include "executor/shim/gl_context.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace av {

namespace {

constexpr uint32_t dxgi_r8g8b8a8_unorm = 28u;
constexpr uint32_t dxgi_b8g8r8a8_unorm = 87u;
constexpr uint32_t drm_format_abgr8888 = 0x34324241u;
constexpr uint32_t drm_format_argb8888 = 0x34325241u;
constexpr uint32_t gl_rgba8 = 0x8058u;
constexpr uint32_t drm_format_mod_linear_low = 0u;
constexpr uint32_t drm_format_mod_linear_high = 0u;
constexpr uint32_t texel_bytes = 4u;

uint32_t fourcc_of(uint32_t dxgi)
{
    AV_SCOPE("fourcc_of");
    if (dxgi == dxgi_r8g8b8a8_unorm) return drm_format_abgr8888;
    if (dxgi == dxgi_b8g8r8a8_unorm) return drm_format_argb8888;
    return 0u;
}

}

GlDmaBufImages::GlDmaBufImages(BlockSource &blocks)
    : create_image_(nullptr), destroy_image_(nullptr), target_storage_(nullptr), egl_(), gl_(),
      images_(blocks, Pool::paged), display_(nullptr)
{
    AV_SCOPE("GlDmaBufImages::GlDmaBufImages");
}

void GlDmaBufImages::configure(GlContext &context)
{
    AV_SCOPE("GlDmaBufImages::configure");
    give_back();
    void *const display = context.display();
    if (display == nullptr || !context.make_current()) return;
    egl_.configure(display);
    gl_.configure(context);
    if (!egl_.lists("EGL_KHR_image_base") || !egl_.lists("EGL_EXT_image_dma_buf_import") ||
        !egl_.lists("EGL_EXT_image_dma_buf_import_modifiers") || !gl_.lists("GL_EXT_EGL_image_storage")) {
        egl_.forget();
        return;
    }
    create_image_ = reinterpret_cast<void *(*)(void *, void *, unsigned, void *, const int *)>(
        eglGetProcAddress("eglCreateImageKHR"));
    destroy_image_ = reinterpret_cast<unsigned (*)(void *, void *)>(eglGetProcAddress("eglDestroyImageKHR"));
    target_storage_ = reinterpret_cast<void (*)(unsigned, void *, const int *)>(
        context.resolve("glEGLImageTargetTextureStorageEXT"));
    if (create_image_ != nullptr && destroy_image_ != nullptr && target_storage_ != nullptr) {
        display_ = display;
        return;
    }
    give_back();
}

void GlDmaBufImages::give_back()
{
    AV_SCOPE("GlDmaBufImages::give_back");
    if (display_ != nullptr)
        images_.erase_if([this](uint32_t, const Image &held) {
            destroy_image_(display_, held.image_);
            return true;
        });
    images_.clear();
    egl_.forget();
    display_ = nullptr;
    create_image_ = nullptr;
    destroy_image_ = nullptr;
    target_storage_ = nullptr;
}

bool GlDmaBufImages::imports_images() const
{
    AV_SCOPE("GlDmaBufImages::imports_images");
    return display_ != nullptr;
}

bool GlDmaBufImages::import(uint32_t memory, int fd, const SharedImageShape &shape)
{
    AV_SCOPE("GlDmaBufImages::import");
    const uint32_t fourcc = fourcc_of(shape.format());
    if (display_ == nullptr || fd < 0 || memory == 0u || !shape.described() || fourcc == 0u ||
        shape.pitch() < shape.width() * texel_bytes || images_.find(memory) != nullptr)
        return false;
    const int attributes[] = {
        EGL_WIDTH,                         static_cast<int>(shape.width()),
        EGL_HEIGHT,                        static_cast<int>(shape.height()),
        EGL_LINUX_DRM_FOURCC_EXT,          static_cast<int>(fourcc),
        EGL_DMA_BUF_PLANE0_FD_EXT,         fd,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT,     0,
        EGL_DMA_BUF_PLANE0_PITCH_EXT,      static_cast<int>(shape.pitch()),
        EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, static_cast<int>(drm_format_mod_linear_low),
        EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, static_cast<int>(drm_format_mod_linear_high),
        EGL_NONE,
    };
    void *const image = create_image_(display_, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes);
    if (image == nullptr) return false;
    uint32_t     slot = 0u;
    const Status held = images_.insert(memory, Image{image, shape.width(), shape.height(), gl_rgba8}, slot);
    if (held == Status::ok) return true;
    destroy_image_(display_, image);
    return false;
}

bool GlDmaBufImages::covers(uint32_t memory, uint32_t width, uint32_t height, uint32_t internal_format) const
{
    AV_SCOPE("GlDmaBufImages::covers");
    const Image *const held = images_.find(memory);
    return held != nullptr && held->width_ == width && held->height_ == height &&
           held->internal_format_ == internal_format;
}

bool GlDmaBufImages::bind(uint32_t texture, uint32_t memory)
{
    AV_SCOPE("GlDmaBufImages::bind");
    const Image *const held = images_.find(memory);
    if (held == nullptr || display_ == nullptr) return false;
    target_storage_(texture, held->image_, nullptr);
    return true;
}

void GlDmaBufImages::forget(uint32_t memory)
{
    AV_SCOPE("GlDmaBufImages::forget");
    Image held{};
    if (!images_.take(memory, held)) return;
    destroy_image_(display_, held.image_);
}

uint32_t GlDmaBufImages::count() const
{
    AV_SCOPE("GlDmaBufImages::count");
    return images_.count();
}

}
