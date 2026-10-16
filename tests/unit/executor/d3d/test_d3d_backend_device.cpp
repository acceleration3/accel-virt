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
#include "executor/host_memory_source.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/sharing/host_shares.hpp"
#include "executor/sharing/session_shares.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "harness/executor/d3d_loopback.hpp"
#include "executor/shim/host_heap.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/d3d/passthrough_geometry_shader.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "proto/resources/surface_point.hpp"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

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

namespace av {

struct D3dBackendProbe {
    static D3dObjectTable &objects(D3dBackend &backend) { return backend.objects_; }
    static WatchedBlocks  &blocks(D3dBackend &backend) { return backend.blocks_; }
};

struct WatchedBlocksProbe {
    static void draw_from(WatchedBlocks &blocks, BlockSource &source) { blocks.blocks_ = &source; }
};

}  // namespace av

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

/* The shares of the GPU `device` is on. */
av::HostShares &shares_of(av::LoaderHostApis &apis, const av::D3d11HostDevice *device)
{
    std::uint32_t index = 0u;
    while (apis.d3d11_device(index) != device) index++;
    return *apis.shares(index);
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
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, device));
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

/* A batch's mark is watched by DXVK, which writes the session's eventfd once
 * the GPU reaches it: the worker is woken for it rather than polling. A
 * native DXVK whose SetEvent did nothing (tools/dxvk-patches/0009) left the
 * worker spinning on the fence for up to 100 ms after every batch. */
namespace {

std::atomic<std::uint32_t> batch_wakes{0u};

void count_batch_wake(void *count)
{
    static_cast<std::atomic<std::uint32_t> *>(count)->fetch_add(1u);
}

}  // namespace

TEST(d3d_backend_device, a_batch_the_gpu_finishes_wakes_the_worker_instead_of_being_polled_for)
{
    av::LoaderHostApis apis;
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    HeldBackend     held(first_backend);
    av::D3dBackend &backend = held.backend();
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, device));
    batch_wakes = 0u;
    backend.bind_signal(&count_batch_wake, &batch_wakes);
    av::D3dLoopback wire(backend);
    std::uint32_t hresult = 1u, level = 0u;
    CHECK(av_d3d_CreateDevice11(wire, 1u, 0u, 0u, &hresult, &level));
    make_triangle(wire);
    bind_triangle(wire);
    CHECK(av_d3d_Draw(wire, 3u, 0u));
    CHECK_EQ(wire.refused(), 0u);
    /* Every batch was marked and watched: nothing is left to poll for. */
    CHECK(!backend.poll());
    std::uint64_t marked = 0u, reached = 0u;
    for (std::uint32_t tries = 0u; tries < 2000u; tries++) {
        backend.gpu_progress(marked, reached);
        if (reached == marked && batch_wakes.load() != 0u) break;
        const timespec pause = {0, 1000000};
        nanosleep(&pause, nullptr);
    }
    CHECK(marked >= 1u);
    CHECK(reached == marked);
    CHECK(batch_wakes.load() >= 1u);
    backend.shut_down();
}

/* A surface one process shares opens in another by the id the driver minted,
 * for as long as the guest holds it: past the creator's own session, as on
 * Windows DWM keeps a window's surface its creator already let go of, and
 * until the driver's release. A key already shared keeps naming what it
 * named. */
/* A stream-output declaration with no shader streams the vertex shader's
 * output: the guest's driver writes the pass-through geometry shader it stands
 * for (PassthroughGeometryShader), and the host makes it, position declared as
 * a system value and the streamed register named by the same signatures. */
TEST(d3d_backend_device, a_stream_output_with_no_shader_is_a_pass_through_geometry_shader)
{
    av::LoaderHostApis apis;
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    HeldBackend     held(first_backend);
    av::D3dBackend &backend = held.backend();
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, device));
    av::D3dLoopback wire(backend);
    std::uint32_t hresult = 1u, level = 0u;
    CHECK(av_d3d_CreateDevice11(wire, 1u, 0u, 0u, &hresult, &level));
    constexpr std::uint32_t position = 1u;
    constexpr std::uint32_t float_type = 3u;
    av::d3d::PassthroughGeometryShader shader;
    CHECK(shader.add(0u, 0xFu, position));
    CHECK(shader.add(1u, 0xFu, 0u));
    const std::uint32_t *const tokens = shader.tokens();
    CHECK(tokens != nullptr);
    const SignatureEntry signature[2] = {{position, 0u, 0xFu, float_type, 0u, 0u}, {0u, 1u, 0xFu, float_type, 0u, 0u}};
    const StreamOutEntry streamed[1] = {{0u, 0u, 1u, 0xFu}};
    const std::uint32_t  stride = 16u;
    constexpr std::uint32_t stage_geometry = 3u, made = 50u, no_rasterized_stream = 0xFFFFFFFFu;
    CHECK(av_d3d_CreateShader(wire, made, stage_geometry, 2u, signature, 2u, signature, 0u, nullptr, 1u, streamed, 1u,
                              &stride, no_rasterized_stream, tokens, shader.bytes()));
    CHECK(av_d3d_Finish11(wire, &hresult));
    CHECK(av::D3dBackendProbe::objects(backend).find(made, av::D3dObjectKind::geometry_shader) != nullptr);
    CHECK_EQ(wire.refused(), 0u);
}

/* What the pass-through geometry shader streams is what the vertex shader
 * wrote, vertex for vertex: the fixture's position (x, y, 0, 1) and its colour
 * (1, 0, 0, 1), eight floats a vertex in the declaration's order - the data a
 * game's skinning streams out and then draws from. */
TEST(d3d_backend_device, a_stream_output_with_no_shader_streams_what_the_vertex_shader_wrote)
{
    av::LoaderHostApis apis;
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    HeldBackend     held(first_backend);
    av::D3dBackend &backend = held.backend();
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, device));
    av::D3dLoopback wire(backend);
    std::uint32_t hresult = 1u, level = 0u;
    CHECK(av_d3d_CreateDevice11(wire, 1u, 0u, 0u, &hresult, &level));
    make_triangle(wire);
    av::d3d::PassthroughGeometryShader shader;
    for (std::uint32_t index = 0u; index < triangle_vs::outputs_count; index++)
        CHECK(shader.add(triangle_vs::outputs[index].register_index, triangle_vs::outputs[index].mask,
                         triangle_vs::outputs[index].system_value));
    const std::uint32_t *const tokens = shader.tokens();
    const StreamOutEntry streamed[2] = {{0u, 0u, 0u, 0xFu}, {0u, 0u, 1u, 0xFu}};
    const std::uint32_t  stride = 32u;
    constexpr std::uint32_t stage_geometry = 3u, gs = 50u, out = 60u, no_rasterized_stream = 0xFFFFFFFFu;
    CHECK(av_d3d_CreateShader(wire, gs, stage_geometry, triangle_vs::outputs_count, triangle_vs::outputs,
                              triangle_vs::outputs_count, triangle_vs::outputs, 0u, nullptr, 2u, streamed, 1u, &stride,
                              no_rasterized_stream, tokens, shader.bytes()));
    ResourceDesc buffer = {};
    buffer.dimension = 1u;
    buffer.width = 3u * stride;
    buffer.bind_flags = 0x10u;   /* D3D11_BIND_STREAM_OUTPUT */
    CHECK(av_d3d_CreateResource(wire, out, buffer, 0u));
    bind_triangle(wire);
    CHECK(av_d3d_SetTopology(wire, 1u));   /* points */
    CHECK(av_d3d_SetShader(wire, stage_geometry, gs));
    const StreamOutTarget target = {out, 0u};
    CHECK(av_d3d_SetStreamOutTargets(wire, 1u, &target));
    CHECK(av_d3d_Draw(wire, 3u, 0u));
    static float streamed_out[3u * 8u];
    std::uint32_t row_pitch = 0u, depth_pitch = 0u;
    std::uint64_t read = 0u;
    CHECK(av_d3d_ReadSubresource(wire, out, 0u, &hresult, &row_pitch, &depth_pitch, streamed_out, sizeof streamed_out,
                                 &read));
    CHECK_EQ(hresult, 0u);
    const float expected[3u * 8u] = {-1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
                                     -1.0f, 1.0f,  0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
                                     0.0f,  1.0f,  0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f};
    std::uint32_t wrong = 0u;
    for (std::uint32_t index = 0u; index < 3u * 8u; index++) {
        if (streamed_out[index] != expected[index]) {
            if (wrong < 8u) std::fprintf(stderr, "float %u: %f, not %f\n", index, streamed_out[index], expected[index]);
            wrong++;
        }
    }
    CHECK_EQ(wrong, 0u);
    CHECK_EQ(wire.refused(), 0u);
}

TEST(d3d_backend_device, a_shared_surface_outlives_its_creator_until_the_guest_releases_it)
{
    av::LoaderHostApis apis;
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    constexpr std::uint64_t key = 0x5100000000000007ull;
    HeldBackend     held_creator(first_backend), held_opener(second_backend);
    av::D3dBackend &creator = held_creator.backend(), &opener = held_opener.backend();
    av::HostShares   &shares = shares_of(apis, device);
    av::SessionShares creator_shares, opener_shares;
    creator_shares.bind(shares, 1u, creator);
    opener_shares.bind(shares, 1u, opener);
    CHECK(creator.initialize(creator_shares, device));
    CHECK(opener.initialize(opener_shares, device));
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
    const std::uint32_t held = shares.live();

    ResourceDesc other = surface;
    other.width = size / 2u;
    CHECK(av_d3d_CreateResource(opening, 9u, other, key));
    CHECK_EQ(shares.live(), held);
    std::uint32_t found = 0u;
    ResourceDesc opened = {};
    CHECK(av_d3d_OpenResource(opening, 2u, key, &found, &opened));
    CHECK_EQ(found, 1u);
    CHECK_EQ(opened.width, size);

    CHECK(av_d3d_DestroyObject(made, 1u));
    creator_shares.end();
    creator.shut_down();
    CHECK(av_d3d_OpenResource(opening, 3u, key, &found, &opened));
    CHECK_EQ(found, 1u);
    CHECK_EQ(opened.width, size);

    shares.release(1u, key);
    device->retire_shares();
    CHECK_EQ(shares.live(), held - 1u);
    CHECK(av_d3d_OpenResource(opening, 4u, key, &found, &opened));
    CHECK_EQ(found, 0u);
    CHECK_EQ(opening.refused(), 0u);
    CHECK(opener.invariants_hold());
    opener_shares.end();
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
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, device));
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
    av::HostHeap            surfaces_heap;
    av::SharedSurfaces      surfaces(surfaces_heap);
    HeldBackend             held(first_backend);
    av::D3dBackend         &backend = held.backend();
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, device, nullptr, &surfaces));
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
    /* A flip names no frame. This session shares nothing, so its target has
     * no copy to show - said once, not per flip - and a flip of a buffer or
     * of a texture never shared has nothing to show at all. */
    CHECK(av_d3d_PresentPoint(wire, 1u, 0u, 0u));
    CHECK(av_d3d_PresentPoint(wire, 1u, 0u, 0u));
    CHECK(av_d3d_PresentPoint(wire, 3u, 0u, 0u));
    ResourceDesc unshared_texture = {};
    unshared_texture.dimension = 3u;
    unshared_texture.width = size;
    unshared_texture.height = size;
    unshared_texture.depth_or_array = 1u;
    unshared_texture.mip_levels = 1u;
    unshared_texture.format = format_b8g8r8a8;
    unshared_texture.sample_count = 1u;
    unshared_texture.bind_flags = bind_render_target;
    CHECK(av_d3d_CreateResource(wire, 9u, unshared_texture, 0u));
    CHECK(av_d3d_PresentPoint(wire, 9u, 0u, 0u));
    CHECK(av_d3d_PresentPoint(wire, 3u, 0u, 2u));   /* the vertex buffer, never shared */
    CHECK(!surfaces.map(av::SurfacePoint(shown, 2u), 0u, view));
    /* Nor does it open anything: no share answers it. */
    std::uint32_t found = 1u;
    ResourceDesc  opened = {};
    CHECK(av_d3d_OpenResource(wire, 10u, shown, &found, &opened));
    CHECK_EQ(found, 0u);
    CHECK(av_d3d_Finish11(wire, &removed));
    CHECK_EQ(removed, 0u);
    CHECK_EQ(wire.refused(), 0u);
    CHECK(surfaces.invariants_hold());
}

/* A flip of a target its session shares is copied to the share's own flip
 * copy, which is what a compositor opening the id reads: the copy is made at
 * the first flip and written again at each. */
TEST(d3d_backend_device, a_flip_of_a_shared_target_is_shown_through_its_copy)
{
    av::LoaderHostApis apis;
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    constexpr std::uint64_t key = 0x5100000000000009ull;
    HeldBackend       held(first_backend);
    av::D3dBackend   &backend = held.backend();
    av::HostShares   &shares = shares_of(apis, device);
    av::SessionShares session;
    session.bind(shares, 1u, backend);
    CHECK(backend.initialize(session, device));
    av::D3dLoopback wire(backend);
    std::uint32_t hresult = 1u, level = 0u, removed = 1u;
    CHECK(av_d3d_CreateDevice11(wire, 1u, 0u, 0u, &hresult, &level));
    ResourceDesc target = {};
    target.dimension = 3u;
    target.width = size;
    target.height = size;
    target.depth_or_array = 1u;
    target.mip_levels = 1u;
    target.format = format_b8g8r8a8;
    target.sample_count = 1u;
    target.bind_flags = bind_render_target;
    CHECK(av_d3d_CreateResource(wire, 1u, target, key));
    IUnknown *copy = shares.shown(1u, key);
    CHECK(copy == nullptr);
    CHECK(av_d3d_PresentPoint(wire, 1u, 0u, 0u));
    copy = shares.shown(1u, key);
    CHECK(copy != nullptr);
    CHECK(av_d3d_PresentPoint(wire, 1u, 0u, 0u));
    IUnknown *const again = shares.shown(1u, key);
    CHECK(again == copy);
    if (again != nullptr) again->Release();
    if (copy != nullptr) copy->Release();
    CHECK(av_d3d_Finish11(wire, &removed));
    CHECK_EQ(removed, 0u);
    CHECK_EQ(wire.refused(), 0u);
    session.end();
    backend.shut_down();
    shares.release(1u, key);
    device->retire_shares();
}

/* The session's tables grow in a source that runs out: what the host has no
 * memory for is told to the guest as E_OUTOFMEMORY - a device create
 * answers it, and a command refused for it removes its device for it, not
 * for an internal error - while the next command, with memory, is refused
 * for what it did, as before. Each table the backend grows is met at its
 * first growth: the object table's, a device's constant bindings, a
 * rotation's remade views and the pending queries. */
TEST(d3d_backend_device, what_the_host_has_no_memory_for_is_out_of_memory_to_the_guest)
{
    av::LoaderHostApis apis;
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    av::HostMemorySource *source = nullptr;
    for (std::uint32_t index = 0u; index < apis.gpu_count(); index++)
        if (apis.d3d11_device(index) == device) source = apis.memory_source(index);
    constexpr std::uint32_t out_of_memory = 0x8007000Eu;
    av::MockPlatform platform;
    {
        HeldBackend     held(first_backend);
        av::D3dBackend &backend = held.backend();
        av::WatchedBlocksProbe::draw_from(av::D3dBackendProbe::blocks(backend), platform);
        av::SessionShares unshared;
        CHECK(backend.initialize(unshared, device));
        constexpr std::size_t region_bytes = 16u * 4096u;
        av::D3dLoopback wire(backend, source, source != nullptr ? region_bytes : 0u);

        std::uint32_t hresult = 0u, level = 0u, removed = 0u;
        platform.fail_allocation_at(1u);
        CHECK(av_d3d_CreateDevice11(wire, 1u, 0u, 0u, &hresult, &level));
        CHECK_EQ(hresult, out_of_memory);
        CHECK(av_d3d_CreateDevice11(wire, 1u, 0u, 0u, &hresult, &level));
        CHECK_EQ(hresult, 0u);
        make_triangle(wire);
        CHECK_EQ(wire.refused(), 0u);

        /* An object the table cannot grow for. */
        CHECK(av_d3d_CreateDevice11(wire, 2u, 0u, 0u, &hresult, &level));
        platform.fail_allocation_at(1u);
        ResourceDesc buffer = {};
        buffer.dimension = 1u;
        buffer.width = 64u;
        buffer.bind_flags = bind_vertex;
        for (std::uint32_t id = 100u; id < 200u && removed == 0u; id++) {
            CHECK(av_d3d_CreateResource(wire, id, buffer, 0u));
            CHECK(av_d3d_Finish11(wire, &removed));
        }
        CHECK_EQ(removed, out_of_memory);

        /* A device's first constant binding, with nowhere to keep it. */
        CHECK(av_d3d_CreateDevice11(wire, 3u, 0u, 0u, &hresult, &level));
        platform.fail_allocation_at(1u);
        const ConstantBufferBinding constants = {4u, 0u, 0u};
        CHECK(av_d3d_SetConstantBuffers(wire, 4u, 0u, 1u, &constants));
        CHECK(av_d3d_Finish11(wire, &removed));
        CHECK_EQ(removed, out_of_memory);

        /* A rotation whose remade view has nowhere to wait to be rebound. */
        CHECK(av_d3d_CreateDevice11(wire, 4u, 0u, 0u, &hresult, &level));
        ResourceDesc target = {};
        target.dimension = 3u;
        target.width = size;
        target.height = size;
        target.depth_or_array = 1u;
        target.mip_levels = 1u;
        target.format = format_b8g8r8a8;
        target.sample_count = 1u;
        target.bind_flags = bind_render_target;
        CHECK(av_d3d_CreateResource(wire, 30u, target, 0u));
        CHECK(av_d3d_Finish11(wire, &removed));
        CHECK_EQ(removed, 0u);
        platform.fail_allocation_at(1u);
        const std::uint32_t rotated[2] = {1u, 30u};
        CHECK(av_d3d_RotateResources11(wire, 2u, rotated));
        CHECK(av_d3d_Finish11(wire, &removed));
        CHECK_EQ(removed, out_of_memory);

        /* With memory, a command refused for what it named is an internal
         * error, as ever: the flag is the command's alone. */
        CHECK(av_d3d_CreateDevice11(wire, 5u, 0u, 0u, &hresult, &level));
        CHECK(av_d3d_SetShader(wire, 4u, 99u));
        CHECK(av_d3d_Finish11(wire, &removed));
        CHECK_EQ(removed, driver_internal_error);

        /* A query ended with its results bound, and nowhere to keep it pending. */
        if (source != nullptr) {
            CHECK(av_d3d_CreateDevice11(wire, 6u, 0u, 0u, &hresult, &level));
            constexpr std::uint32_t query_event = 0u;
            CHECK(av_d3d_CreateQuery(wire, 40u, query_event, 0u));
            CHECK(av_d3d_BindQueryResults(wire, 0u, 4u));
            CHECK(av_d3d_Finish11(wire, &removed));
            CHECK_EQ(removed, 0u);
            platform.fail_allocation_at(1u);
            CHECK(av_d3d_EndQuery(wire, 40u));
            CHECK(av_d3d_Finish11(wire, &removed));
            CHECK_EQ(removed, out_of_memory);
        }
        CHECK_EQ(wire.refused(), 0u);
        CHECK(backend.invariants_hold());
        backend.shut_down();
    }
}
