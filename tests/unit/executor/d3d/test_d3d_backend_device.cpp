/* The Direct3D backend on this machine's GPU, through DXVK: a triangle drawn
 * the way the guest's user-mode driver will draw one - shaders as the DDI hands
 * them (tokens and register signatures, tests/fixtures/d3d/), an input layout
 * naming registers, a dynamic constant buffer written with UpdateSubresource -
 * and read back, judged pixel by pixel where the answer is certain.
 *
 * DXVK is loaded from AV_D3D_PATH, or from build-d3d/lib in this checkout
 * (tools/build_d3d_runtime.sh). A host without it returns without judging, as
 * the other device tests do on a host without their API. */
#include "executor/d3d/shared_surfaces.hpp"
#include "fixtures/d3d/triangle_ps.hpp"
#include "fixtures/d3d/triangle_vs.hpp"
#include "core/support/object_storage.hpp"
#include "executor/d3d/d3d_backend.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "harness/executor/d3d_loopback.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "proto/resources/surface_point.hpp"

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
/* DXGI_ERROR_DRIVER_INTERNAL_ERROR: the reason a refused command removes its device for. */
constexpr std::uint32_t driver_internal_error = 0x887A0020u;

struct Vertex {
    float x, y;
    float r, g, b, a;
};

/* One device of the host's, on its first GPU that runs Direct3D; null when
 * none does. */
av::D3d11HostDevice *host_device(av::LoaderHostApis &apis)
{
    if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
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
void make_triangle(av::D3dLoopback &wire, std::uint64_t shared_key = 0u)
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
    CHECK(av_d3d_CreateResource(wire, 1u, target, shared_key));
    ViewDesc rtv = {};
    rtv.resource = 1u;
    rtv.format = format_b8g8r8a8;
    rtv.dimension = rtv_texture2d;
    CHECK(av_d3d_CreateRenderTargetView(wire, 2u, rtv));

    const Vertex vertices[3] = {{-1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
                                {-1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
                                {0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f}};
    ResourceDesc buffer = {};
    buffer.dimension = 1u;
    buffer.width = sizeof(vertices);
    buffer.bind_flags = bind_vertex;
    CHECK(av_d3d_CreateResource(wire, 3u, buffer, 0u));
    CHECK(av_d3d_UpdateSubresource(wire, 3u, 0u, 0u, Box{}, 0u, 0u, 0u, vertices, sizeof(vertices)));
    const float tint[4] = {0.5f, 0.5f, 0.5f, 1.0f};
    buffer.width = 16u;
    buffer.bind_flags = bind_constant;
    CHECK(av_d3d_CreateResource(wire, 4u, buffer, 0u));
    CHECK(av_d3d_UpdateSubresource(wire, 4u, 0u, 0u, Box{}, 0u, 0u, 0u, tint, sizeof(tint)));

    CHECK(av_d3d_CreateShader(wire, 5u, 0u, triangle_vs::inputs_count, triangle_vs::inputs, triangle_vs::outputs_count,
                               triangle_vs::outputs, 0u, nullptr, 0u, nullptr, 0u, nullptr, 0xffffffffu,
                               triangle_vs::tokens, sizeof(triangle_vs::tokens)));
    CHECK(av_d3d_CreateShader(wire, 6u, 4u, triangle_ps::inputs_count, triangle_ps::inputs, triangle_ps::outputs_count,
                               triangle_ps::outputs, 0u, nullptr, 0u, nullptr, 0u, nullptr, 0xffffffffu,
                               triangle_ps::tokens, sizeof(triangle_ps::tokens)));
    const InputElement elements[2] = {{0u, 0u, format_r32g32, 0u, 0u, 0u}, {0u, 8u, format_r32g32b32a32, 0u, 0u, 1u}};
    CHECK(av_d3d_CreateInputLayout(wire, 7u, 2u, elements));
}

/* The triangle's pipeline bound on the selected device, the target cleared blue. */
void bind_triangle(av::D3dLoopback &wire)
{
    const std::uint32_t targets[1] = {2u};
    CHECK(av_d3d_SetRenderTargets(wire, 1u, targets, 0u, 1u, 0u, nullptr));
    CHECK(av_d3d_ClearRenderTargetView(wire, 2u, Float4{{0.0f, 0.0f, 1.0f, 1.0f}}));
    const Viewport viewport = {0.0f, 0.0f, float(size), float(size), 0.0f, 1.0f};
    CHECK(av_d3d_SetViewports(wire, 1u, &viewport));
    CHECK(av_d3d_SetInputLayout(wire, 7u));
    const VertexBufferBinding binding = {3u, sizeof(Vertex), 0u};
    CHECK(av_d3d_SetVertexBuffers(wire, 0u, 1u, &binding));
    CHECK(av_d3d_SetTopology(wire, topology_triangles));
    CHECK(av_d3d_SetShader(wire, 0u, 5u));
    CHECK(av_d3d_SetShader(wire, 4u, 6u));
    const ConstantBufferBinding constants = {4u, 0u, 0u};
    CHECK(av_d3d_SetConstantBuffers(wire, 4u, 0u, 1u, &constants));
}

/* A backend is several megabytes - two object tables and Direct3D 12's
 * descriptor state - so a test holding two would overflow its stack: they
 * live in static storage, constructed and destroyed around each test. */
av::ObjectStorage<av::D3dBackend> first_backend, second_backend;

class HeldBackend {
public:
    explicit HeldBackend(av::ObjectStorage<av::D3dBackend> &storage) : storage_(storage)
    {
        CHECK_STATUS(storage_.construct(), av::Status::ok);
    }
    ~HeldBackend() { storage_.destroy(); }
    HeldBackend(const HeldBackend &) = delete;
    HeldBackend &operator=(const HeldBackend &) = delete;

    av::D3dBackend &backend() { return storage_.object(); }

private:
    av::ObjectStorage<av::D3dBackend> &storage_;
};

}  // namespace

TEST(d3d_backend_device, a_triangle_drawn_from_ddi_shaders_reads_back_where_it_was_drawn)
{
    av::LoaderHostApis apis;
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    HeldBackend     held(first_backend);
    av::D3dBackend &backend = held.backend();
    CHECK(backend.initialize(device));
    av::D3dLoopback wire(backend);

    std::uint32_t hresult = 1u, level = 0u;
    CHECK(av_d3d_CreateDevice11(wire, 1u, 0u, 0u, &hresult, &level));
    CHECK_EQ(hresult, 0u);
    CHECK(level >= 0xb000u);

    make_triangle(wire);
    CHECK_EQ(wire.refused(), 0u);
    bind_triangle(wire);
    CHECK(av_d3d_Draw(wire, 3u, 0u));

    static std::uint8_t pixels[size * size * 4u];
    std::uint32_t row_pitch = 0u, depth_pitch = 0u;
    std::uint64_t read = 0u;
    CHECK(av_d3d_ReadSubresource(wire, 1u, 0u, &hresult, &row_pitch, &depth_pitch, pixels, sizeof(pixels), &read));
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

    /* An id this session never made is refused, never followed as a pointer:
     * the device that named it is removed, not the session, and the next
     * Finish11 says why. */
    std::uint32_t removed = 1u;
    CHECK(av_d3d_Finish11(wire, &removed));
    CHECK_EQ(removed, 0u);
    CHECK(av_d3d_SetShader(wire, 4u, 99u));
    CHECK(av_d3d_SetShader(wire, 4u, 5u));
    CHECK(av_d3d_Finish11(wire, &removed));
    CHECK_EQ(removed, driver_internal_error);
    CHECK_EQ(wire.refused(), 0u);
    CHECK(backend.invariants_hold());
    backend.shut_down();
}

/* A surface one process shares opens in another by the id the driver minted,
 * for as long as the guest holds it: past the creator's own session, as on
 * Windows DWM keeps a window's surface its creator already let go of, and
 * until the driver's release. A key already shared keeps naming what it
 * named. */
TEST(d3d_backend_device, a_shared_surface_outlives_its_creator_until_the_guest_releases_it)
{
    av::LoaderHostApis apis;
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    constexpr std::uint64_t key = 0x5100000000000007ull;
    HeldBackend     held_creator(first_backend), held_opener(second_backend);
    av::D3dBackend &creator = held_creator.backend(), &opener = held_opener.backend();
    CHECK(creator.initialize(device));
    CHECK(opener.initialize(device));
    av::D3dLoopback made(creator), opening(opener);
    std::uint32_t hresult = 1u, level = 0u;
    CHECK(av_d3d_CreateDevice11(made, 1u, 0u, 0u, &hresult, &level));
    CHECK(av_d3d_CreateDevice11(opening, 1u, 0u, 0u, &hresult, &level));
    ResourceDesc surface = {};
    surface.dimension = 3u;
    surface.width = size;
    surface.height = size;
    surface.depth_or_array = 1u;
    surface.mip_levels = 1u;
    surface.format = format_b8g8r8a8;
    surface.sample_count = 1u;
    surface.bind_flags = bind_render_target;
    CHECK(av_d3d_CreateResource(made, 1u, surface, key));
    const std::uint32_t held = device->shared().count();

    ResourceDesc other = surface;
    other.width = size / 2u;
    CHECK(av_d3d_CreateResource(opening, 9u, other, key));
    CHECK_EQ(device->shared().count(), held);
    std::uint32_t found = 0u;
    ResourceDesc opened = {};
    CHECK(av_d3d_OpenResource(opening, 2u, key, &found, &opened));
    CHECK_EQ(found, 1u);
    CHECK_EQ(opened.width, size);

    CHECK(av_d3d_DestroyObject(made, 1u));
    creator.shut_down();
    CHECK(av_d3d_OpenResource(opening, 3u, key, &found, &opened));
    CHECK_EQ(found, 1u);
    CHECK_EQ(opened.width, size);

    device->release_shared(key);
    CHECK_EQ(device->shared().count(), held - 1u);
    CHECK(av_d3d_OpenResource(opening, 4u, key, &found, &opened));
    CHECK_EQ(found, 0u);
    CHECK_EQ(opening.refused(), 0u);
    CHECK(opener.invariants_hold());
    opener.shut_down();
}

/* A process's devices share one session: objects are the session's, pipeline
 * state is each device's own. A second device starts with nothing bound, so
 * its draw leaves the first device's target as it was, and the first draws the
 * triangle when it is selected again. A command a device gets refused removes
 * that device alone: its later commands are dropped and Finish11 names the
 * reason, while the other device goes on drawing. Selecting an id the host
 * has no device for selects nothing and drops what follows, rather than
 * removing the device selected before it. Creating objects and finishing
 * need no device selected. */
TEST(d3d_backend_device, each_device_of_a_session_draws_with_its_own_pipeline_state)
{
    av::LoaderHostApis apis;
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    HeldBackend     held(first_backend);
    av::D3dBackend &backend = held.backend();
    CHECK(backend.initialize(device));
    av::D3dLoopback wire(backend);

    std::uint32_t hresult = 1u, level = 0u, removed = 1u;
    CHECK(av_d3d_CreateDevice11(wire, 1u, 0u, 0u, &hresult, &level));
    CHECK_EQ(hresult, 0u);
    make_triangle(wire);
    bind_triangle(wire);
    CHECK(av_d3d_CreateDevice11(wire, 2u, 0u, 0u, &hresult, &level));
    CHECK_EQ(hresult, 0u);
    CHECK(av_d3d_Draw(wire, 3u, 0u));

    static std::uint8_t pixels[size * size * 4u];
    std::uint32_t row_pitch = 0u, depth_pitch = 0u;
    std::uint64_t read = 0u;
    CHECK(av_d3d_ReadSubresource(wire, 1u, 0u, &hresult, &row_pitch, &depth_pitch, pixels, sizeof(pixels), &read));
    CHECK_EQ(hresult, 0u);
    CHECK_EQ(pixel(pixels, 4u, size / 2u), 0xff0000ffu);

    /* Device 2 names a shader nobody made: it is removed, and its draw of
     * device 1's triangle - rebound on it - is dropped. */
    CHECK(av_d3d_SetShader(wire, 4u, 77u));
    CHECK(av_d3d_Finish11(wire, &removed));
    CHECK_EQ(removed, driver_internal_error);
    bind_triangle(wire);
    CHECK(av_d3d_Draw(wire, 3u, 0u));
    CHECK(av_d3d_SelectDevice11(wire, 1u));
    CHECK(av_d3d_ReadSubresource(wire, 1u, 0u, &hresult, &row_pitch, &depth_pitch, pixels, sizeof(pixels), &read));
    CHECK_EQ(pixel(pixels, 4u, size / 2u), 0xff0000ffu);
    CHECK(av_d3d_Finish11(wire, &removed));
    CHECK_EQ(removed, 0u);

    CHECK(av_d3d_Draw(wire, 3u, 0u));
    CHECK(av_d3d_ReadSubresource(wire, 1u, 0u, &hresult, &row_pitch, &depth_pitch, pixels, sizeof(pixels), &read));
    const std::uint32_t tinted = pixel(pixels, 4u, size / 2u);
    CHECK_EQ(tinted & 0xff00ffffu, 0xff000000u);
    CHECK((tinted >> 16u & 0xffu) >= 0x7fu && (tinted >> 16u & 0xffu) <= 0x80u);

    /* A device destroyed is gone: selecting it selects nothing, and what
     * follows is dropped without touching device 1. */
    CHECK(av_d3d_DestroyDevice11(wire, 2u));
    CHECK(av_d3d_SelectDevice11(wire, 2u));
    CHECK(av_d3d_ClearRenderTargetView(wire, 2u, Float4{{0.0f, 1.0f, 0.0f, 1.0f}}));
    CHECK(av_d3d_SelectDevice11(wire, 1u));
    CHECK(av_d3d_Finish11(wire, &removed));
    CHECK_EQ(removed, 0u);
    CHECK(av_d3d_ReadSubresource(wire, 1u, 0u, &hresult, &row_pitch, &depth_pitch, pixels, sizeof(pixels), &read));
    CHECK_EQ(pixel(pixels, size - 4u, size / 2u), 0xff0000ffu);

    /* With no device selected nothing touches pipeline state, but the
     * session still finishes, and a device it still has makes objects. */
    CHECK(av_d3d_CreateDevice11(wire, 3u, 0u, 0u, &hresult, &level));
    CHECK(av_d3d_DestroyDevice11(wire, 3u));
    CHECK(av_d3d_Finish11(wire, &removed));
    CHECK_EQ(removed, 0u);
    ResourceDesc buffer = {};
    buffer.dimension = 1u;
    buffer.width = 64u;
    buffer.bind_flags = bind_vertex;
    CHECK(av_d3d_CreateResource(wire, 20u, buffer, 0u));
    CHECK_EQ(wire.refused(), 0u);
    /* A refusal with no device selected is the session's: nobody else to blame. */
    CHECK(!av_d3d_Draw(wire, 3u, 0u));
    CHECK_EQ(wire.refused(), 1u);
    CHECK(backend.invariants_hold());
    backend.shut_down();
    CHECK(backend.invariants_hold());
}

/* A present's point on a shared target stages the target as the commands
 * before it left it: a clear after it never reaches the frame. The frame is
 * mapped once the GPU has copied it and gone once let go; a point of 0, or a
 * resource never shared, stages nothing and removes no device. */
TEST(d3d_backend_device, a_present_point_stages_the_frame_the_commands_before_it_drew)
{
    av::LoaderHostApis apis;
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    constexpr std::uint64_t shown = 0x5150u;
    av::SharedSurfaces      surfaces;
    HeldBackend             held(first_backend);
    av::D3dBackend         &backend = held.backend();
    CHECK(backend.initialize(device, nullptr, &surfaces));
    av::D3dLoopback wire(backend);
    std::uint32_t hresult = 1u, level = 0u, removed = 1u;
    CHECK(av_d3d_CreateDevice11(wire, 1u, 0u, 0u, &hresult, &level));
    make_triangle(wire, shown);
    bind_triangle(wire);
    CHECK(av_d3d_Draw(wire, 3u, 0u));
    CHECK(av_d3d_PresentPoint(wire, 1u, 0u, 1u));
    CHECK(av_d3d_ClearRenderTargetView(wire, 2u, Float4{{1.0f, 0.0f, 0.0f, 1.0f}}));
    AvFrameView view = {};
    CHECK(surfaces.map(av::SurfacePoint(shown, 1u), 1000u, view));
    CHECK_EQ(view.width, size);
    CHECK_EQ(view.height, size);
    CHECK(view.row_pitch >= size * 4u);
    std::uint32_t edge = 0u;
    std::memcpy(&edge, view.pixels + (size / 2u) * view.row_pitch + (size - 4u) * 4u, sizeof edge);
    CHECK_EQ(edge, 0xff0000ffu);   /* the blue the draw left, not the red cleared after */
    surfaces.unmap(av::SurfacePoint(shown, 1u));
    CHECK(!surfaces.map(av::SurfacePoint(shown, 1u), 0u, view));
    CHECK(av_d3d_PresentPoint(wire, 1u, 0u, 0u));
    CHECK(av_d3d_PresentPoint(wire, 3u, 0u, 2u));   /* the vertex buffer, never shared */
    CHECK(!surfaces.map(av::SurfacePoint(shown, 2u), 0u, view));
    CHECK(av_d3d_Finish11(wire, &removed));
    CHECK_EQ(removed, 0u);
    CHECK_EQ(wire.refused(), 0u);
    CHECK(surfaces.invariants_hold());
}
