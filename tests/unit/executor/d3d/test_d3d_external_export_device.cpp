/* A Direct3D 11 texture a guest shares, handed to a process outside QEMU as
 * its own memory (docs/external-export.md): the guest's copy into it is
 * counted, a sync_file covers it, and an EGL consumer on the same GPU imports
 * the dma-buf with the layout the driver kept with the memory, waits on the
 * sync_file on its own GPU queue and reads every texel where Direct3D put it -
 * no copy on the host. The same frame staged into a linear image instead
 * (HostImageExport, the scanout's path) is timed beside it, which is what
 * chose the direct export. A host without DXVK or EGL on one GPU returns
 * without judging. */
#include "device/abi/av_executor.h"
#include "executor/d3d/d3d_backend.hpp"
#include "executor/executor_root.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/gl_display.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "harness/executor/d3d_loopback.hpp"
#include "harness/executor/root_under_test.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
/* The consumer reaches GL through eglGetProcAddress, as libEGL alone is linked. */
#define GL_GLES_PROTOTYPES 0
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <poll.h>
#include <unistd.h>

extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libvulkan\n"
           "leak:libdxvk\n"
           "leak:libvulkan_radeon\n"
           "leak:libvkd3d\n"
           "leak:libEGL\n"
           "leak:libGLX\n"
           "leak:libdbus-1\n"
           "leak:libglapi\n"
           "leak:libgallium\n"
           "leak:libz\n";
}

namespace {

using namespace av::d3dproto;

constexpr std::uint32_t largest_texels = 2560u * 1440u;
constexpr std::uint32_t format_b8g8r8a8 = 87u;
constexpr std::uint32_t dimension_texture2d = 3u;
constexpr std::uint32_t bind_shader_resource = 0x8u;
constexpr std::uint32_t bind_render_target = 0x20u;
constexpr std::uint32_t misc_shared = 0x2u;
constexpr std::uint64_t surface = 0x5100000000000077ull;
constexpr std::uint32_t drm_argb8888 = 0x34325241u;   /* AR24 */
constexpr std::uint32_t uuid_bytes = 16u;

ResourceDesc texture(std::uint32_t across, std::uint32_t down, std::uint32_t misc)
{
    ResourceDesc desc = {};
    desc.dimension = dimension_texture2d;
    desc.width = across;
    desc.height = down;
    desc.depth_or_array = 1u;
    desc.mip_levels = 1u;
    desc.format = format_b8g8r8a8;
    desc.sample_count = 1u;
    desc.bind_flags = bind_shader_resource | bind_render_target;
    desc.misc_flags = misc;
    return desc;
}

/* Every texel its own value, so a layout the two sides disagree on moves
 * texels where a clear would not show it. B8G8R8A8 in memory, texel `texel`
 * of a picture `across` wide. */
std::uint8_t pattern_byte(std::uint32_t texel, std::uint32_t across, std::uint32_t channel)
{
    switch (channel) {
    case 0u: return static_cast<std::uint8_t>(texel * 7u + texel / 251u);   /* blue */
    case 1u: return static_cast<std::uint8_t>(texel / across);              /* green */
    case 2u: return static_cast<std::uint8_t>(texel % across);              /* red */
    default: return 0xffu;
    }
}

double milliseconds_since(const timespec &start)
{
    timespec now = {};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<double>(now.tv_sec - start.tv_sec) * 1e3 + static_cast<double>(now.tv_nsec - start.tv_nsec) / 1e6;
}

/* The consumer: a GLES context of its own on the GPU's EGL display, which
 * imports the dma-buf as an EGLImage and the sync_file as a native fence. */
class EglConsumer {
public:
    explicit EglConsumer(void *display)
        : display_(static_cast<EGLDisplay>(display)), context_(EGL_NO_CONTEXT),
          create_image_(reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"))),
          destroy_image_(reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"))),
          create_sync_(reinterpret_cast<PFNEGLCREATESYNCKHRPROC>(eglGetProcAddress("eglCreateSyncKHR"))),
          wait_sync_(reinterpret_cast<PFNEGLWAITSYNCKHRPROC>(eglGetProcAddress("eglWaitSyncKHR"))),
          destroy_sync_(reinterpret_cast<PFNEGLDESTROYSYNCKHRPROC>(eglGetProcAddress("eglDestroySyncKHR"))),
          target_(reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(
              eglGetProcAddress("glEGLImageTargetTexture2DOES"))),
          gen_textures_(reinterpret_cast<PFNGLGENTEXTURESPROC>(eglGetProcAddress("glGenTextures"))),
          bind_texture_(reinterpret_cast<PFNGLBINDTEXTUREPROC>(eglGetProcAddress("glBindTexture"))),
          delete_textures_(reinterpret_cast<PFNGLDELETETEXTURESPROC>(eglGetProcAddress("glDeleteTextures"))),
          gen_framebuffers_(reinterpret_cast<PFNGLGENFRAMEBUFFERSPROC>(eglGetProcAddress("glGenFramebuffers"))),
          bind_framebuffer_(reinterpret_cast<PFNGLBINDFRAMEBUFFERPROC>(eglGetProcAddress("glBindFramebuffer"))),
          delete_framebuffers_(
              reinterpret_cast<PFNGLDELETEFRAMEBUFFERSPROC>(eglGetProcAddress("glDeleteFramebuffers"))),
          attach_(reinterpret_cast<PFNGLFRAMEBUFFERTEXTURE2DPROC>(eglGetProcAddress("glFramebufferTexture2D"))),
          status_(reinterpret_cast<PFNGLCHECKFRAMEBUFFERSTATUSPROC>(eglGetProcAddress("glCheckFramebufferStatus"))),
          read_pixels_(reinterpret_cast<PFNGLREADPIXELSPROC>(eglGetProcAddress("glReadPixels"))),
          error_(reinterpret_cast<PFNGLGETERRORPROC>(eglGetProcAddress("glGetError")))
    {
        const char *const extensions = eglQueryString(display_, EGL_EXTENSIONS);
        if (extensions == nullptr || std::strstr(extensions, "EGL_EXT_image_dma_buf_import") == nullptr ||
            std::strstr(extensions, "EGL_ANDROID_native_fence_sync") == nullptr || create_image_ == nullptr ||
            target_ == nullptr || create_sync_ == nullptr || wait_sync_ == nullptr || eglBindAPI(EGL_OPENGL_ES_API) == EGL_FALSE)
            return;
        const EGLint attributes[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_NONE};
        context_ = eglCreateContext(display_, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attributes);
        if (context_ != EGL_NO_CONTEXT && eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, context_) == EGL_FALSE) {
            eglDestroyContext(display_, context_);
            context_ = EGL_NO_CONTEXT;
        }
    }
    ~EglConsumer()
    {
        if (context_ == EGL_NO_CONTEXT) return;
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display_, context_);
    }
    EglConsumer(const EglConsumer &) = delete;
    EglConsumer &operator=(const EglConsumer &) = delete;

    bool usable() const { return context_ != EGL_NO_CONTEXT; }

    /* The sync_file waited for on this context's GPU queue; it is EGL's once
     * the sync is made. */
    bool wait(int fd)
    {
        const EGLint attributes[] = {EGL_SYNC_NATIVE_FENCE_FD_ANDROID, fd, EGL_NONE};
        EGLSyncKHR const sync = create_sync_(display_, EGL_SYNC_NATIVE_FENCE_ANDROID, attributes);
        if (sync == EGL_NO_SYNC_KHR) return false;
        const bool waited = wait_sync_(display_, sync, 0) == EGL_TRUE;
        destroy_sync_(display_, sync);
        return waited;
    }

    /* The image as the answer described it, read back as RGBA rows. The
     * modifier is implicit, so no modifier attribute is given: the driver
     * reads the layout it kept with the memory. */
    bool read(const AvExportedImage &image, std::uint8_t *pixels)
    {
        const EGLint attributes[] = {EGL_WIDTH,
                                     static_cast<EGLint>(image.width),
                                     EGL_HEIGHT,
                                     static_cast<EGLint>(image.height),
                                     EGL_LINUX_DRM_FOURCC_EXT,
                                     static_cast<EGLint>(image.fourcc),
                                     EGL_DMA_BUF_PLANE0_FD_EXT,
                                     image.fd,
                                     EGL_DMA_BUF_PLANE0_OFFSET_EXT,
                                     static_cast<EGLint>(image.offset),
                                     EGL_DMA_BUF_PLANE0_PITCH_EXT,
                                     static_cast<EGLint>(image.stride),
                                     EGL_NONE};
        EGLImageKHR const made = create_image_(display_, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes);
        if (made == EGL_NO_IMAGE_KHR) {
            std::fprintf(stderr, "eglCreateImageKHR refused the export: 0x%x\n", eglGetError());
            return false;
        }
        GLuint texture = 0u, framebuffer = 0u;
        gen_textures_(1, &texture);
        bind_texture_(GL_TEXTURE_2D, texture);
        target_(GL_TEXTURE_2D, made);
        gen_framebuffers_(1, &framebuffer);
        bind_framebuffer_(GL_FRAMEBUFFER, framebuffer);
        attach_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        const bool complete = status_(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        if (complete)
            read_pixels_(0, 0, static_cast<GLsizei>(image.width), static_cast<GLsizei>(image.height), GL_RGBA,
                         GL_UNSIGNED_BYTE, pixels);
        const bool read = complete && error_() == GL_NO_ERROR;
        bind_framebuffer_(GL_FRAMEBUFFER, 0u);
        delete_framebuffers_(1, &framebuffer);
        delete_textures_(1, &texture);
        destroy_image_(display_, made);
        return read;
    }

private:
    EGLDisplay                          display_;
    EGLContext                          context_;
    PFNEGLCREATEIMAGEKHRPROC            create_image_;
    PFNEGLDESTROYIMAGEKHRPROC           destroy_image_;
    PFNEGLCREATESYNCKHRPROC             create_sync_;
    PFNEGLWAITSYNCKHRPROC               wait_sync_;
    PFNEGLDESTROYSYNCKHRPROC            destroy_sync_;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC target_;
    PFNGLGENTEXTURESPROC                gen_textures_;
    PFNGLBINDTEXTUREPROC                bind_texture_;
    PFNGLDELETETEXTURESPROC             delete_textures_;
    PFNGLGENFRAMEBUFFERSPROC            gen_framebuffers_;
    PFNGLBINDFRAMEBUFFERPROC            bind_framebuffer_;
    PFNGLDELETEFRAMEBUFFERSPROC         delete_framebuffers_;
    PFNGLFRAMEBUFFERTEXTURE2DPROC       attach_;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC     status_;
    PFNGLREADPIXELSPROC                 read_pixels_;
    PFNGLGETERRORPROC                   error_;
};

/* A sync_file signals by becoming readable. */
bool signalled(int fd, int timeout_ms)
{
    pollfd watched = {fd, POLLIN, 0};
    return poll(&watched, 1u, timeout_ms) == 1 && (watched.revents & POLLIN) != 0;
}

bool find_gpu(av::LoaderHostApis &apis, std::uint32_t &gpu)
{
    for (gpu = 0u; gpu < apis.gpu_count(); gpu++)
        if (apis.d3d11_device(gpu) != nullptr && apis.gl_display(gpu) != nullptr) return true;
    std::fprintf(stderr, "no GPU here runs both DXVK and EGL: nothing judged\n");
    return false;
}

/* One GPU's executor root and a Direct3D 11 session on it, driven over the
 * wire as the guest drives it. */
class ExportRig {
public:
    ExportRig() : apis_(), gpu_(0u), uuid_{}, held_(), direct3d_(), wire_(direct3d_), usable_(false)
    {
        if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
        apis_.open();
        if (!find_gpu(apis_, gpu_)) return;
        apis_.gpu_uuid(gpu_, uuid_);
        CHECK_STATUS(held_->initialize(apis_), av::Status::ok);
        CHECK(direct3d_.initialize(apis_.d3d11_device(gpu_)));
        std::uint32_t hresult = 1u, level = 0u;
        CHECK(av_d3d_CreateDevice11(wire_, 1u, 0u, 0u, &hresult, &level));
        usable_ = true;
    }
    ~ExportRig()
    {
        if (usable_) direct3d_.shut_down();
    }
    ExportRig(const ExportRig &) = delete;
    ExportRig &operator=(const ExportRig &) = delete;

    bool               usable() const { return usable_; }
    av::ExecutorRoot  &root() { return *held_; }
    const std::uint8_t *uuid() const { return uuid_; }
    av::D3dLoopback   &wire() { return wire_; }
    void              *display() { return apis_.gl_display(gpu_)->handle(); }

    /* Texture `id`, `across` by `down`, filled with the pattern a strip of
     * rows at a time: the loopback carries a megabyte a command. */
    void fill(std::uint32_t id, std::uint32_t across, std::uint32_t down)
    {
        static std::uint8_t frame[largest_texels * 4u];
        for (std::uint32_t at = 0u; at < across * down; at++)
            for (std::uint32_t channel = 0u; channel < 4u; channel++)
                frame[at * 4u + channel] = pattern_byte(at, across, channel);
        const std::uint32_t strip = (512u * 1024u) / (across * 4u);
        for (std::uint32_t top = 0u; top < down; top += strip) {
            const std::uint32_t bottom = top + strip < down ? top + strip : down;
            const Box           rows = {0u, top, 0u, across, bottom, 1u};
            CHECK(av_d3d_UpdateSubresource(wire_, id, 0u, 1u, rows, across * 4u, 0u, 0u,
                                           frame + std::size_t{top} * across * 4u,
                                           std::size_t{bottom - top} * across * 4u));
        }
    }

private:
    av::LoaderHostApis apis_;
    std::uint32_t      gpu_;
    std::uint8_t       uuid_[uuid_bytes];
    av::RootUnderTest  held_;
    av::D3dBackend     direct3d_;
    /* Holds only the backend until a command is sent. */
    av::D3dLoopback    wire_;
    bool               usable_;
};

/* The texels of `image` the consumer read back, as RGBA rows, compared with
 * the pattern: the count that differ. */
std::uint32_t texels_wrong(const std::uint8_t *pixels, std::uint32_t across, std::uint32_t down)
{
    std::uint32_t wrong = 0u;
    for (std::uint32_t at = 0u; at < across * down; at++)
        wrong += pixels[at * 4u] != pattern_byte(at, across, 2u) || pixels[at * 4u + 1u] != pattern_byte(at, across, 1u) ||
                         pixels[at * 4u + 2u] != pattern_byte(at, across, 0u) || pixels[at * 4u + 3u] != 0xffu
                     ? 1u
                     : 0u;
    return wrong;
}

}  // namespace

/* What the device asks of the executor, at a small size: copies are counted
 * once they ran and only into the shared surface, a sync_file comes with
 * every answer, and what nobody shares is refused. */
TEST(d3d_external_export_device, copies_into_the_surface_are_counted_and_covered_by_a_sync_file)
{
    constexpr std::uint32_t across = 256u, down = 128u;
    ExportRig rig;
    if (!rig.usable()) return;
    av::ExecutorRoot &root = rig.root();
    CHECK(av_d3d_CreateResource(rig.wire(), 1u, texture(across, down, misc_shared), surface));
    CHECK(av_d3d_CreateResource(rig.wire(), 2u, texture(across, down, 0u), 0u));

    std::uint64_t writes = 99u;
    int           fence = -1;
    CHECK(root.sync_surface(rig.uuid(), surface, writes, fence));
    CHECK_EQ(writes, 0u);
    CHECK(fence >= 0);
    CHECK(signalled(fence, 5000));
    close(fence);
    CHECK(!root.sync_surface(rig.uuid(), surface + 1u, writes, fence));
    CHECK_EQ(fence, -1);
    const std::uint8_t absent[uuid_bytes] = {};
    CHECK(!root.sync_surface(absent, surface, writes, fence));
    CHECK(!root.sync_surface(nullptr, surface, writes, fence));

    CHECK(av_d3d_CopyResource(rig.wire(), 1u, 2u));
    CHECK(av_d3d_CopyResource(rig.wire(), 2u, 1u));   /* into a texture nobody shares */
    const Box corner = {0u, 0u, 0u, 16u, 16u, 1u};
    CHECK(av_d3d_CopySubresourceRegion(rig.wire(), 1u, 0u, 8u, 8u, 0u, 2u, 0u, 1u, corner, 0u));
    CHECK(root.sync_surface(rig.uuid(), surface, writes, fence));
    CHECK_EQ(writes, 2u);
    CHECK(signalled(fence, 5000));
    close(fence);

    AvExportedImage image = {};
    CHECK(!root.export_surface(rig.uuid(), surface + 1u, image));
    CHECK_EQ(image.fd, -1);
    CHECK(!root.export_surface(absent, surface, image));
    CHECK(root.export_surface(rig.uuid(), surface, image));
    CHECK(image.fd >= 0);
    CHECK_EQ(image.slot, 0u);
    CHECK_EQ(image.width, across);
    CHECK_EQ(image.height, down);
    CHECK_EQ(image.fourcc, drm_argb8888);
    CHECK_EQ(image.modifier, std::uint64_t{AV_EXPORT_MODIFIER_IMPLICIT});
    CHECK(image.stride >= across * 4u);
    CHECK(lseek(image.fd, 0, SEEK_END) >= static_cast<off_t>(std::uint64_t{image.stride} * down));
    close(image.fd);
    /* The guest let go of it: nothing is shared under it any more. */
    root.release_shared(rig.uuid(), surface);
    CHECK(!root.export_surface(rig.uuid(), surface, image));
    CHECK(!root.sync_surface(rig.uuid(), surface, writes, fence));
}

/* The consumer's whole path, at a window's odd size and a desktop's: a
 * sync_file waited for on its own GPU queue, the dma-buf imported with the
 * layout the driver kept with the memory, and every texel read back where
 * Direct3D put it - which a pitch the two drivers disagreed on would shear. */
TEST(d3d_external_export_device, every_texel_reaches_an_egl_consumer_through_the_surface_s_own_memory)
{
    struct Size {
        std::uint32_t across;
        std::uint32_t down;
    };
    const Size sizes[] = {{256u, 128u}, {1003u, 701u}, {2560u, 1440u}};
    ExportRig  rig;
    if (!rig.usable()) return;
    EglConsumer consumer(rig.display());
    if (!consumer.usable()) {
        std::fprintf(stderr, "EGL here imports no dma-buf or sync_file: nothing judged\n");
        return;
    }
    static std::uint8_t pixels[largest_texels * 4u];
    std::uint32_t       id = 1u;
    for (const Size &size : sizes) {
        const std::uint32_t shared = id++, source = id++;
        const std::uint64_t key = surface + shared;
        CHECK(av_d3d_CreateResource(rig.wire(), shared, texture(size.across, size.down, misc_shared), key));
        CHECK(av_d3d_CreateResource(rig.wire(), source, texture(size.across, size.down, 0u), 0u));
        rig.fill(source, size.across, size.down);
        CHECK(av_d3d_CopyResource(rig.wire(), shared, source));
        std::uint64_t writes = 0u;
        int           fence = -1;
        CHECK(rig.root().sync_surface(rig.uuid(), key, writes, fence));
        CHECK_EQ(writes, 1u);
        AvExportedImage image = {};
        CHECK(rig.root().export_surface(rig.uuid(), key, image));
        std::fprintf(stderr, "exported %ux%u: stride %u, offset %u, %ld bytes\n", image.width, image.height,
                     image.stride, image.offset, static_cast<long>(lseek(image.fd, 0, SEEK_END)));
        CHECK(consumer.wait(fence));
        std::memset(pixels, 0, std::size_t{size.across} * size.down * 4u);
        CHECK(consumer.read(image, pixels));
        close(image.fd);
        CHECK_EQ(texels_wrong(pixels, size.across, size.down), 0u);
    }
}

/* What a mirror pays per frame at 2560x1440, the guest's copy included: the
 * direct export's sync_file waited for, against the same frame staged into a
 * linear exportable image and waited for (HostImageExport, the copy the
 * direct export saves). Printed, not judged - the machine's load decides the
 * numbers - beyond each path answering. */
TEST(d3d_external_export_device, the_direct_export_and_a_linear_copy_are_timed_at_a_desktop_s_size)
{
    constexpr std::uint32_t across = 2560u, down = 1440u, frames = 240u;
    ExportRig rig;
    if (!rig.usable()) return;
    av::ExecutorRoot &root = rig.root();
    CHECK(av_d3d_CreateResource(rig.wire(), 1u, texture(across, down, misc_shared), surface));
    CHECK(av_d3d_CreateResource(rig.wire(), 2u, texture(across, down, 0u), 0u));
    timespec start = {};
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (std::uint32_t frame = 0u; frame < frames; frame++) {
        CHECK(av_d3d_CopyResource(rig.wire(), 1u, 2u));
        std::uint64_t writes = 0u;
        int           fence = -1;
        CHECK(root.sync_surface(rig.uuid(), surface, writes, fence));
        CHECK_EQ(writes, std::uint64_t{frame} + 1u);
        CHECK(signalled(fence, 5000));
        close(fence);
    }
    const double direct = milliseconds_since(start) / frames;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (std::uint32_t frame = 0u; frame < frames; frame++) {
        CHECK(av_d3d_CopyResource(rig.wire(), 1u, 2u));
        AvExportedImage linear = {};
        CHECK(root.export_image(rig.uuid(), surface, linear));
    }
    const double copied = milliseconds_since(start) / frames;
    std::fprintf(stderr,
                 "%ux%u per frame: guest copy + sync_file signalled %.3f ms; guest copy + linear copy waited %.3f ms\n",
                 across, down, direct, copied);
}
