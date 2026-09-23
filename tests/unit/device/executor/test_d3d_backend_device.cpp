/* The Direct3D backend on this machine's GPU, through DXVK: a triangle drawn
 * the way the guest's user-mode driver will draw one - shaders as the DDI hands
 * them (tokens and register signatures, tests/fixtures/d3d/), an input layout
 * naming registers, a dynamic constant buffer written with UpdateSubresource -
 * and read back, judged pixel by pixel where the answer is certain.
 *
 * DXVK is loaded from RVK_D3D_PATH, or from build-d3d/lib in this checkout
 * (tools/build_d3d_runtime.sh). A host without it returns without judging, as
 * the other device tests do on a host without their API. */
#include "fixtures/d3d/triangle_ps.hpp"
#include "fixtures/d3d/triangle_vs.hpp"
#include "device/executor/d3d_backend.hpp"
#include "device/shim/d3d11_host_device.hpp"
#include "device/shim/loader_host_apis.hpp"
#include "harness/executor/d3d_loopback.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libvulkan\n"
           "leak:libdxvk\n"
           "leak:libvulkan_radeon\n"
           "leak:libvkd3d\n"
           "leak:libOpenCL\n"
           "leak:libEGL\n"
           "leak:libGLX\n"
           "leak:libSDL3\n"
           "leak:libdbus-1\n"
           "leak:libglapi\n"
           "leak:libz\n";
}

namespace {

using namespace av::d3dproto;

constexpr std::uint32_t format_b8g8r8a8 = 87u;
constexpr std::uint32_t format_r32g32 = 16u;
constexpr std::uint32_t format_r32g32b32a32 = 2u;
constexpr std::uint32_t bind_vertex = 0x1u;
constexpr std::uint32_t bind_constant = 0x4u;
constexpr std::uint32_t bind_render_target = 0x20u;
constexpr std::uint32_t rtv_texture2d = 4u;
constexpr std::uint32_t topology_triangles = 4u;
constexpr std::uint32_t size = 64u;

struct Vertex {
    float x, y;
    float r, g, b, a;
};

/* One device of the host's, on its first GPU that runs Direct3D; null when
 * none does. */
av::D3d11HostDevice *host_device(av::LoaderHostApis &apis)
{
    if (std::getenv("RVK_D3D_PATH") == nullptr) setenv("RVK_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
    apis.open();
    for (std::uint32_t index = 0u; index < apis.gpu_count(); index++)
        if (apis.d3d11_device(index) != nullptr) return apis.d3d11_device(index);
    std::fprintf(stderr, "no GPU here runs DXVK: nothing judged\n");
    return nullptr;
}

std::uint32_t pixel(const std::uint8_t *pixels, std::uint32_t x, std::uint32_t y)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, pixels + (y * size + x) * 4u, sizeof(value));
    return value;
}

/* The render target (1) and its view (2), a triangle's vertices (3), a tint
 * (4), its shaders (5, 6) and input layout (7): one triangle over the left
 * half and down the middle, red halved by the tint. */
void make_triangle(av::D3dLoopback &wire)
{
    ResourceDesc target = {};
    target.dimension = 3u;
    target.width = size;
    target.height = size;
    target.depth_or_array = 1u;
    target.mip_levels = 1u;
    target.format = format_b8g8r8a8;
    target.sample_count = 1u;
    target.bind_flags = bind_render_target;
    CHECK(rvk_d3d_CreateResource(wire, 1u, target, 0u));
    ViewDesc rtv = {};
    rtv.resource = 1u;
    rtv.format = format_b8g8r8a8;
    rtv.dimension = rtv_texture2d;
    CHECK(rvk_d3d_CreateRenderTargetView(wire, 2u, rtv));

    const Vertex vertices[3] = {{-1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
                                {-1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
                                {0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f}};
    ResourceDesc buffer = {};
    buffer.dimension = 1u;
    buffer.width = sizeof(vertices);
    buffer.bind_flags = bind_vertex;
    CHECK(rvk_d3d_CreateResource(wire, 3u, buffer, 0u));
    CHECK(rvk_d3d_UpdateSubresource(wire, 3u, 0u, 0u, Box{}, 0u, 0u, 0u, vertices, sizeof(vertices)));
    const float tint[4] = {0.5f, 0.5f, 0.5f, 1.0f};
    buffer.width = 16u;
    buffer.bind_flags = bind_constant;
    CHECK(rvk_d3d_CreateResource(wire, 4u, buffer, 0u));
    CHECK(rvk_d3d_UpdateSubresource(wire, 4u, 0u, 0u, Box{}, 0u, 0u, 0u, tint, sizeof(tint)));

    CHECK(rvk_d3d_CreateShader(wire, 5u, 0u, triangle_vs::inputs_count, triangle_vs::inputs, triangle_vs::outputs_count,
                               triangle_vs::outputs, 0u, nullptr, 0u, nullptr, 0u, nullptr, 0xffffffffu,
                               triangle_vs::tokens, sizeof(triangle_vs::tokens)));
    CHECK(rvk_d3d_CreateShader(wire, 6u, 4u, triangle_ps::inputs_count, triangle_ps::inputs, triangle_ps::outputs_count,
                               triangle_ps::outputs, 0u, nullptr, 0u, nullptr, 0u, nullptr, 0xffffffffu,
                               triangle_ps::tokens, sizeof(triangle_ps::tokens)));
    const InputElement elements[2] = {{0u, 0u, format_r32g32, 0u, 0u, 0u}, {0u, 8u, format_r32g32b32a32, 0u, 0u, 1u}};
    CHECK(rvk_d3d_CreateInputLayout(wire, 7u, 2u, elements));
}

/* The triangle's pipeline bound on the selected device, the target cleared blue. */
void bind_triangle(av::D3dLoopback &wire)
{
    const std::uint32_t targets[1] = {2u};
    CHECK(rvk_d3d_SetRenderTargets(wire, 1u, targets, 0u, 1u, 0u, nullptr));
    CHECK(rvk_d3d_ClearRenderTargetView(wire, 2u, Float4{{0.0f, 0.0f, 1.0f, 1.0f}}));
    const Viewport viewport = {0.0f, 0.0f, float(size), float(size), 0.0f, 1.0f};
    CHECK(rvk_d3d_SetViewports(wire, 1u, &viewport));
    CHECK(rvk_d3d_SetInputLayout(wire, 7u));
    const VertexBufferBinding binding = {3u, sizeof(Vertex), 0u};
    CHECK(rvk_d3d_SetVertexBuffers(wire, 0u, 1u, &binding));
    CHECK(rvk_d3d_SetTopology(wire, topology_triangles));
    CHECK(rvk_d3d_SetShader(wire, 0u, 5u));
    CHECK(rvk_d3d_SetShader(wire, 4u, 6u));
    const ConstantBufferBinding constants = {4u, 0u, 0u};
    CHECK(rvk_d3d_SetConstantBuffers(wire, 4u, 0u, 1u, &constants));
}

}  // namespace

TEST(d3d_backend_device, a_triangle_drawn_from_ddi_shaders_reads_back_where_it_was_drawn)
{
    av::LoaderHostApis apis;
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    av::D3dBackend backend;
    CHECK(backend.initialize(device));
    av::D3dLoopback wire(backend);

    std::uint32_t hresult = 1u, level = 0u;
    CHECK(rvk_d3d_CreateDevice11(wire, 1u, 0u, 0u, &hresult, &level));
    CHECK_EQ(hresult, 0u);
    CHECK(level >= 0xb000u);

    make_triangle(wire);
    CHECK_EQ(wire.refused(), 0u);
    bind_triangle(wire);
    CHECK(rvk_d3d_Draw(wire, 3u, 0u));

    static std::uint8_t pixels[size * size * 4u];
    std::uint32_t row_pitch = 0u, depth_pitch = 0u;
    std::uint64_t read = 0u;
    CHECK(rvk_d3d_ReadSubresource(wire, 1u, 0u, &hresult, &row_pitch, &depth_pitch, pixels, sizeof(pixels), &read));
    CHECK_EQ(hresult, 0u);
    CHECK_EQ(row_pitch, size * 4u);
    CHECK_EQ(read, static_cast<std::uint64_t>(sizeof(pixels)));
    /* BGRA in memory: 0xAARRGGBB as a little-endian word. Half of red is 0x80
     * after rounding on every driver; the clear is exact. */
    const std::uint32_t tinted = pixel(pixels, 4u, size / 2u);
    CHECK_EQ(tinted & 0xff00ffffu, 0xff000000u);
    CHECK((tinted >> 16u & 0xffu) >= 0x7fu && (tinted >> 16u & 0xffu) <= 0x80u);
    CHECK_EQ(pixel(pixels, size - 4u, size / 2u), 0xff0000ffu);
    CHECK_EQ(wire.refused(), 0u);

    /* An id this session never made is a refused command, not a pointer. */
    CHECK(!rvk_d3d_SetShader(wire, 4u, 99u));
    CHECK(!rvk_d3d_SetShader(wire, 4u, 5u));
    CHECK_EQ(wire.refused(), 2u);
    CHECK(backend.invariants_hold());
    backend.shut_down();
}

/* A process's devices share one session: objects are the session's, pipeline
 * state is each device's own. A second device starts with nothing bound, so
 * its draw leaves the first device's target as it was, and the first draws the
 * triangle when it is selected again. A device destroyed is no longer one to
 * select, and destroying the selected one leaves nothing to draw with. */
TEST(d3d_backend_device, each_device_of_a_session_draws_with_its_own_pipeline_state)
{
    av::LoaderHostApis apis;
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    av::D3dBackend backend;
    CHECK(backend.initialize(device));
    av::D3dLoopback wire(backend);

    std::uint32_t hresult = 1u, level = 0u;
    CHECK(rvk_d3d_CreateDevice11(wire, 1u, 0u, 0u, &hresult, &level));
    CHECK_EQ(hresult, 0u);
    make_triangle(wire);
    bind_triangle(wire);
    CHECK(rvk_d3d_CreateDevice11(wire, 2u, 0u, 0u, &hresult, &level));
    CHECK_EQ(hresult, 0u);
    CHECK(!rvk_d3d_CreateDevice11(wire, 2u, 0u, 0u, &hresult, &level));
    CHECK(rvk_d3d_Draw(wire, 3u, 0u));

    static std::uint8_t pixels[size * size * 4u];
    std::uint32_t row_pitch = 0u, depth_pitch = 0u;
    std::uint64_t read = 0u;
    CHECK(rvk_d3d_ReadSubresource(wire, 1u, 0u, &hresult, &row_pitch, &depth_pitch, pixels, sizeof(pixels), &read));
    CHECK_EQ(hresult, 0u);
    CHECK_EQ(pixel(pixels, 4u, size / 2u), 0xff0000ffu);

    CHECK(rvk_d3d_SelectDevice11(wire, 1u));
    CHECK(rvk_d3d_Draw(wire, 3u, 0u));
    CHECK(rvk_d3d_ReadSubresource(wire, 1u, 0u, &hresult, &row_pitch, &depth_pitch, pixels, sizeof(pixels), &read));
    const std::uint32_t tinted = pixel(pixels, 4u, size / 2u);
    CHECK_EQ(tinted & 0xff00ffffu, 0xff000000u);
    CHECK((tinted >> 16u & 0xffu) >= 0x7fu && (tinted >> 16u & 0xffu) <= 0x80u);
    CHECK_EQ(wire.refused(), 1u);

    CHECK(rvk_d3d_DestroyDevice11(wire, 2u));
    CHECK(!rvk_d3d_SelectDevice11(wire, 2u));
    CHECK(!rvk_d3d_DestroyDevice11(wire, 2u));
    CHECK(rvk_d3d_DestroyDevice11(wire, 1u));
    CHECK(!rvk_d3d_Draw(wire, 3u, 0u));
    CHECK_EQ(wire.refused(), 4u);
    /* With no device selected nothing touches pipeline state, but the
     * session still finishes, and a device it still has makes objects. */
    std::uint32_t removed = 1u;
    CHECK(rvk_d3d_CreateDevice11(wire, 3u, 0u, 0u, &hresult, &level));
    CHECK(rvk_d3d_DestroyDevice11(wire, 3u));
    CHECK(rvk_d3d_CreateDevice11(wire, 4u, 0u, 0u, &hresult, &level));
    CHECK(rvk_d3d_CreateDevice11(wire, 5u, 0u, 0u, &hresult, &level));
    CHECK(rvk_d3d_DestroyDevice11(wire, 5u));
    CHECK(!rvk_d3d_Draw(wire, 3u, 0u));
    CHECK(rvk_d3d_Finish11(wire, &removed));
    CHECK_EQ(removed, 0u);
    ResourceDesc buffer = {};
    buffer.dimension = 1u;
    buffer.width = 64u;
    buffer.bind_flags = bind_vertex;
    CHECK(rvk_d3d_CreateResource(wire, 20u, buffer, 0u));
    CHECK_EQ(wire.refused(), 5u);
    CHECK(backend.invariants_hold());
    backend.shut_down();
    CHECK(backend.invariants_hold());
}
