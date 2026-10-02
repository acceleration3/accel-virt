/* A Direct3D 11 texture a guest shares, opened by OpenGL on the same GPU
 * (feature 053, docs/shared-surfaces.md step 5): the texture is made over
 * memory the D3D backend exports, the GL backend imports that memory as a
 * memory object, a pattern is written into a texture over it through GL, and
 * Direct3D reads back every texel where GL put it. This is how an OpenGL window's frames reach a DXGI chain.
 * A host without DXVK or without OpenGL on one GPU returns without judging. */
#include "executor/d3d/d3d_backend.hpp"
#include "executor/opengl/opengl_backend.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "harness/executor/d3d_loopback.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
           "leak:libz\n";
}

namespace av {

/* The host's own GL of a backend, called directly with its context current. */
struct OpenGlBackendProbe {
    static const glproto::RealGl &gl(OpenGlBackend &backend)
    {
        CHECK(backend.context_.make_current());
        return backend.gl_;
    }
};

}  // namespace av

namespace {

using namespace av::d3dproto;

constexpr std::uint32_t size = 64u;
constexpr std::uint32_t format_r8g8b8a8 = 28u;
constexpr std::uint32_t bind_shader_resource = 0x8u;
constexpr std::uint32_t bind_render_target = 0x20u;
constexpr std::uint32_t misc_shared = 0x2u;
constexpr std::uint64_t key = 0x5100000000000031ull;
constexpr std::uint64_t bytes = size * size * 4u;
constexpr unsigned gl_texture_2d = 0x0DE1;
constexpr unsigned gl_rgba8 = 0x8058;
constexpr unsigned gl_rgba = 0x1908;
constexpr unsigned gl_unsigned_byte = 0x1401;

}  // namespace

TEST(d3d_gl_interop_device, a_shared_texture_written_through_opengl_reads_back_through_direct3d)
{
    if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
    av::LoaderHostApis apis;
    apis.open();
    std::uint32_t gpu = apis.gpu_count();
    for (std::uint32_t index = 0u; index < apis.gpu_count() && gpu == apis.gpu_count(); index++)
        if (apis.d3d11_device(index) != nullptr && apis.gl_display(index) != nullptr) gpu = index;
    if (gpu == apis.gpu_count()) {
        std::fprintf(stderr, "no GPU here runs both DXVK and OpenGL: nothing judged\n");
        return;
    }
    av::D3dBackend direct3d;
    CHECK(direct3d.initialize(apis.d3d11_device(gpu)));
    av::D3dLoopback wire(direct3d);
    std::uint32_t hresult = 1u, level = 0u;
    CHECK(av_d3d_CreateDevice11(wire, 1u, 0u, 0u, &hresult, &level));
    ResourceDesc shared = {};
    shared.dimension = 3u;
    shared.width = size;
    shared.height = size;
    shared.depth_or_array = 1u;
    shared.mip_levels = 1u;
    shared.format = format_r8g8b8a8;
    shared.sample_count = 1u;
    shared.bind_flags = bind_shader_resource | bind_render_target;
    shared.misc_flags = misc_shared;
    CHECK(av_d3d_CreateResource(wire, 1u, shared, key));
    std::uint32_t handle_kind = 0u;
    int           fd = -1;
    CHECK(direct3d.export_object(AV_SHARED_MEMORY, 1u, handle_kind, fd));
    CHECK_EQ(handle_kind, std::uint32_t{AV_SHARED_HANDLE_DMA_BUF});
    CHECK(fd >= 0);

    av::OpenGlBackend opengl;
    CHECK(opengl.initialize(apis.gl_display(gpu), nullptr, nullptr));
    std::uint64_t memory = 0u;
    CHECK(opengl.import_object(AV_SHARED_MEMORY, AV_SHARED_HANDLE_DMA_BUF, fd, bytes, memory));
    close(fd);
    {
        const av::glproto::RealGl &gl = av::OpenGlBackendProbe::gl(opengl);
        unsigned texture = 0u;
        gl.glCreateTextures(gl_texture_2d, 1, &texture);
        gl.glTextureStorageMem2DEXT(texture, 1, gl_rgba8, size, size, static_cast<unsigned>(memory), 0u);
        /* Every texel its own value, so a layout the two drivers disagree on
         * moves texels where a clear would not show it. */
        static std::uint8_t pattern[size * size * 4u];
        for (std::uint32_t at = 0u; at < size * size; at++) {
            pattern[at * 4u] = static_cast<std::uint8_t>(at % size);
            pattern[at * 4u + 1u] = static_cast<std::uint8_t>(at / size);
            pattern[at * 4u + 2u] = static_cast<std::uint8_t>(at * 7u);
            pattern[at * 4u + 3u] = 0xffu;
        }
        gl.glTextureSubImage2D(texture, 0, 0, 0, size, size, gl_rgba, gl_unsigned_byte, pattern);
        gl.glFinish();
        CHECK_EQ(gl.glGetError(), 0u);
        gl.glDeleteTextures(1, &texture);
    }

    static std::uint8_t pixels[size * size * 4u];
    std::uint32_t row_pitch = 0u, depth_pitch = 0u;
    std::uint64_t read = 0u;
    CHECK(av_d3d_ReadSubresource(wire, 1u, 0u, &hresult, &row_pitch, &depth_pitch, pixels, sizeof(pixels), &read));
    CHECK_EQ(hresult, 0u);
    /* R8G8B8A8 in memory: red, green, blue, alpha - each texel where GL put it. */
    CHECK_EQ(row_pitch, size * 4u);
    const std::uint32_t probes[] = {0u, 1u, size - 1u, size, size * 3u + 17u, size * size / 2u + 9u, size * size - 1u};
    for (const std::uint32_t at : probes) {
        CHECK_EQ(pixels[at * 4u], static_cast<std::uint8_t>(at % size));
        CHECK_EQ(pixels[at * 4u + 1u], static_cast<std::uint8_t>(at / size));
        CHECK_EQ(pixels[at * 4u + 2u], static_cast<std::uint8_t>(at * 7u));
    }
    opengl.release_object(AV_SHARED_MEMORY, memory);
    opengl.shut_down();
    direct3d.shut_down();
}
