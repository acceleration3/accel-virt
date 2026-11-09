#include "executor/d3d/shared_surfaces.hpp"
#include "fixtures/d3d/triangle_ps.hpp"
#include "fixtures/d3d/triangle_vs.hpp"
#include "core/support/object_storage.hpp"
#include "executor/d3d/d3d_backend.hpp"
#include "executor/host_memory_source.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/sharing/d3d_shares.hpp"
#include "executor/sharing/host_shares.hpp"
#include "harness/executor/executor_log_rig.hpp"
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
constexpr std::uint32_t driver_internal_error = 0x887A0020u;

struct Vertex {
    float x, y;
    float r, g, b, a;
};

av::D3d11HostDevice *host_device(av::LoaderHostApis &apis)
{
    if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
    apis.open();
    for (std::uint32_t index = 0u; index < apis.gpu_count(); index++)
        if (apis.d3d11_device(index) != nullptr) return apis.d3d11_device(index);
    std::fprintf(stderr, "no GPU here runs DXVK: nothing judged\n");
    return nullptr;
}

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
    CHECK(av_d3d_CreateResource(wire, 1u, target, shared_key, 0u, 0u));
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
    CHECK(av_d3d_CreateResource(wire, 3u, buffer, 0u, 0u, 0u));
    CHECK(av_d3d_UpdateSubresource(wire, 3u, 0u, 0u, Box{}, 0u, 0u, 0u, vertices, sizeof(vertices)));
    const float tint[4] = {0.5f, 0.5f, 0.5f, 1.0f};
    buffer.width = 16u;
    buffer.bind_flags = bind_constant;
    CHECK(av_d3d_CreateResource(wire, 4u, buffer, 0u, 0u, 0u));
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

av::ObjectStorage<av::D3dBackend> first_backend, second_backend;

class HeldBackend {
public:
    HeldBackend(av::ObjectStorage<av::D3dBackend> &storage, av::ExecutorLog &log) : storage_(storage)
    {
        CHECK_STATUS(storage_.construct(log), av::Status::ok);
    }
    ~HeldBackend() { storage_.destroy(); }
    HeldBackend(const HeldBackend &) = delete;
    HeldBackend &operator=(const HeldBackend &) = delete;

    av::D3dBackend &backend() { return storage_.object(); }

private:
    av::ObjectStorage<av::D3dBackend> &storage_;
};

}

TEST(d3d_backend_device, a_triangle_drawn_from_ddi_shaders_reads_back_where_it_was_drawn)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    HeldBackend     held(first_backend, rig.log);
    av::D3dBackend &backend = held.backend();
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, rig.imports, device));
    av::D3dLoopback wire(backend, rig.imports);

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
    const std::uint32_t tinted = pixel(pixels, 4u, size / 2u);
    CHECK_EQ(tinted & 0xff00ffffu, 0xff000000u);
    CHECK((tinted >> 16u & 0xffu) >= 0x7fu && (tinted >> 16u & 0xffu) <= 0x80u);
    CHECK_EQ(pixel(pixels, size - 4u, size / 2u), 0xff0000ffu);
    CHECK_EQ(wire.refused(), 0u);

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

namespace {

std::atomic<std::uint32_t> batch_wakes{0u};

void count_batch_wake(void *count)
{
    static_cast<std::atomic<std::uint32_t> *>(count)->fetch_add(1u);
}

}

TEST(d3d_backend_device, a_batch_the_gpu_finishes_wakes_the_worker_instead_of_being_polled_for)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    HeldBackend     held(first_backend, rig.log);
    av::D3dBackend &backend = held.backend();
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, rig.imports, device));
    batch_wakes = 0u;
    backend.bind_signal(&count_batch_wake, &batch_wakes);
    av::D3dLoopback wire(backend, rig.imports);
    std::uint32_t hresult = 1u, level = 0u;
    CHECK(av_d3d_CreateDevice11(wire, 1u, 0u, 0u, &hresult, &level));
    make_triangle(wire);
    bind_triangle(wire);
    CHECK(av_d3d_Draw(wire, 3u, 0u));
    CHECK_EQ(wire.refused(), 0u);
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

TEST(d3d_backend_device, a_stream_output_with_no_shader_is_a_pass_through_geometry_shader)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    HeldBackend     held(first_backend, rig.log);
    av::D3dBackend &backend = held.backend();
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, rig.imports, device));
    av::D3dLoopback wire(backend, rig.imports);
    std::uint32_t hresult = 1u, level = 0u;
    CHECK(av_d3d_CreateDevice11(wire, 1u, 0u, 0u, &hresult, &level));
    constexpr std::uint32_t position = 1u;
    constexpr std::uint32_t float_type = 3u;
    av::d3d::PassthroughGeometryShader shader(av::d3d::PassthroughPrimitive::point);
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

TEST(d3d_backend_device, a_stream_output_with_no_shader_streams_what_the_vertex_shader_wrote)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    HeldBackend     held(first_backend, rig.log);
    av::D3dBackend &backend = held.backend();
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, rig.imports, device));
    av::D3dLoopback wire(backend, rig.imports);
    std::uint32_t hresult = 1u, level = 0u;
    CHECK(av_d3d_CreateDevice11(wire, 1u, 0u, 0u, &hresult, &level));
    make_triangle(wire);
    av::d3d::PassthroughGeometryShader shader(av::d3d::PassthroughPrimitive::point);
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
    buffer.bind_flags = 0x10u;
    CHECK(av_d3d_CreateResource(wire, out, buffer, 0u, 0u, 0u));
    bind_triangle(wire);
    CHECK(av_d3d_SetTopology(wire, 1u));
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
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    constexpr std::uint64_t key = 0x5100000000000007ull;
    HeldBackend     held_creator(first_backend, rig.log), held_opener(second_backend, rig.log);
    av::D3dBackend &creator = held_creator.backend(), &opener = held_opener.backend();
    av::HostShares   &shares = shares_of(apis, device);
    av::SessionShares creator_shares, opener_shares;
    creator_shares.bind(shares, 1u, creator);
    opener_shares.bind(shares, 1u, opener);
    CHECK(creator.initialize(creator_shares, rig.imports, device));
    CHECK(opener.initialize(opener_shares, rig.imports, device));
    av::D3dLoopback made(creator, rig.imports), opening(opener, rig.imports);
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
    CHECK(av_d3d_CreateResource(made, 1u, surface, key, 0u, 0u));
    const std::uint32_t held = shares.live();

    ResourceDesc other = surface;
    other.width = size / 2u;
    CHECK(av_d3d_CreateResource(opening, 9u, other, key, 0u, 0u));
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

TEST(d3d_backend_device, each_device_of_a_session_draws_with_its_own_pipeline_state)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    HeldBackend     held(first_backend, rig.log);
    av::D3dBackend &backend = held.backend();
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, rig.imports, device));
    av::D3dLoopback wire(backend, rig.imports);

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

    CHECK(av_d3d_DestroyDevice11(wire, 2u));
    CHECK(av_d3d_SelectDevice11(wire, 2u));
    CHECK(av_d3d_ClearRenderTargetView(wire, 2u, Float4{{0.0f, 1.0f, 0.0f, 1.0f}}));
    CHECK(av_d3d_SelectDevice11(wire, 1u));
    CHECK(av_d3d_Finish11(wire, &removed));
    CHECK_EQ(removed, 0u);
    CHECK(av_d3d_ReadSubresource(wire, 1u, 0u, &hresult, &row_pitch, &depth_pitch, pixels, sizeof(pixels), &read));
    CHECK_EQ(pixel(pixels, size - 4u, size / 2u), 0xff0000ffu);

    CHECK(av_d3d_CreateDevice11(wire, 3u, 0u, 0u, &hresult, &level));
    CHECK(av_d3d_DestroyDevice11(wire, 3u));
    CHECK(av_d3d_Finish11(wire, &removed));
    CHECK_EQ(removed, 0u);
    ResourceDesc buffer = {};
    buffer.dimension = 1u;
    buffer.width = 64u;
    buffer.bind_flags = bind_vertex;
    CHECK(av_d3d_CreateResource(wire, 20u, buffer, 0u, 0u, 0u));
    CHECK_EQ(wire.refused(), 0u);
    CHECK(!av_d3d_Draw(wire, 3u, 0u));
    CHECK_EQ(wire.refused(), 1u);
    CHECK(backend.invariants_hold());
    backend.shut_down();
    CHECK(backend.invariants_hold());
}

TEST(d3d_backend_device, a_present_point_stages_the_frame_the_commands_before_it_drew)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    constexpr std::uint64_t shown = 0x5150u;
    av::HostHeap            surfaces_heap;
    av::SharedSurfaces      surfaces(surfaces_heap);
    HeldBackend             held(first_backend, rig.log);
    av::D3dBackend         &backend = held.backend();
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, rig.imports, device, nullptr, &surfaces));
    av::D3dLoopback wire(backend, rig.imports);
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
    CHECK_EQ(edge, 0xff0000ffu);
    surfaces.unmap(av::SurfacePoint(shown, 1u));
    CHECK(!surfaces.map(av::SurfacePoint(shown, 1u), 0u, view));
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
    CHECK(av_d3d_CreateResource(wire, 9u, unshared_texture, 0u, 0u, 0u));
    CHECK(av_d3d_PresentPoint(wire, 9u, 0u, 0u));
    CHECK(av_d3d_PresentPoint(wire, 3u, 0u, 2u));
    CHECK(!surfaces.map(av::SurfacePoint(shown, 2u), 0u, view));
    std::uint32_t found = 1u;
    ResourceDesc  opened = {};
    CHECK(av_d3d_OpenResource(wire, 10u, shown, &found, &opened));
    CHECK_EQ(found, 0u);
    CHECK(av_d3d_Finish11(wire, &removed));
    CHECK_EQ(removed, 0u);
    CHECK_EQ(wire.refused(), 0u);
    CHECK(surfaces.invariants_hold());
}

TEST(d3d_backend_device, a_flip_of_a_shared_target_is_shown_through_its_copy)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    constexpr std::uint64_t key = 0x5100000000000009ull;
    HeldBackend       held(first_backend, rig.log);
    av::D3dBackend   &backend = held.backend();
    av::HostShares   &shares = shares_of(apis, device);
    av::SessionShares session;
    session.bind(shares, 1u, backend);
    CHECK(backend.initialize(session, rig.imports, device));
    av::D3dLoopback wire(backend, rig.imports);
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
    CHECK(av_d3d_CreateResource(wire, 1u, target, key, 0u, 0u));
    av::D3dShares d3d_shares(shares);
    IUnknown *copy = d3d_shares.shown(1u, key);
    CHECK(copy == nullptr);
    CHECK(av_d3d_PresentPoint(wire, 1u, 0u, 0u));
    copy = d3d_shares.shown(1u, key);
    CHECK(copy != nullptr);
    CHECK(av_d3d_PresentPoint(wire, 1u, 0u, 0u));
    IUnknown *const again = d3d_shares.shown(1u, key);
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

TEST(d3d_backend_device, what_the_host_has_no_memory_for_is_out_of_memory_to_the_guest)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    av::HostMemorySource *source = nullptr;
    for (std::uint32_t index = 0u; index < apis.gpu_count(); index++)
        if (apis.d3d11_device(index) == device) source = apis.memory_source(index);
    constexpr std::uint32_t out_of_memory = 0x8007000Eu;
    av::MockPlatform platform;
    {
        HeldBackend     held(first_backend, rig.log);
        av::D3dBackend &backend = held.backend();
        av::WatchedBlocksProbe::draw_from(av::D3dBackendProbe::blocks(backend), platform);
        av::SessionShares unshared;
        CHECK(backend.initialize(unshared, rig.imports, device));
        constexpr std::size_t region_bytes = 16u * 4096u;
        av::D3dLoopback wire(backend, rig.imports, source, source != nullptr ? region_bytes : 0u);

        std::uint32_t hresult = 0u, level = 0u, removed = 0u;
        platform.fail_allocation_at(1u);
        CHECK(av_d3d_CreateDevice11(wire, 1u, 0u, 0u, &hresult, &level));
        CHECK_EQ(hresult, out_of_memory);
        CHECK(av_d3d_CreateDevice11(wire, 1u, 0u, 0u, &hresult, &level));
        CHECK_EQ(hresult, 0u);
        make_triangle(wire);
        CHECK_EQ(wire.refused(), 0u);

        CHECK(av_d3d_CreateDevice11(wire, 2u, 0u, 0u, &hresult, &level));
        platform.fail_allocation_at(1u);
        ResourceDesc buffer = {};
        buffer.dimension = 1u;
        buffer.width = 64u;
        buffer.bind_flags = bind_vertex;
        for (std::uint32_t id = 100u; id < 200u && removed == 0u; id++) {
            CHECK(av_d3d_CreateResource(wire, id, buffer, 0u, 0u, 0u));
            CHECK(av_d3d_Finish11(wire, &removed));
        }
        CHECK_EQ(removed, out_of_memory);

        CHECK(av_d3d_CreateDevice11(wire, 3u, 0u, 0u, &hresult, &level));
        platform.fail_allocation_at(1u);
        const ConstantBufferBinding constants = {4u, 0u, 0u};
        CHECK(av_d3d_SetConstantBuffers(wire, 4u, 0u, 1u, &constants));
        CHECK(av_d3d_Finish11(wire, &removed));
        CHECK_EQ(removed, out_of_memory);

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
        CHECK(av_d3d_CreateResource(wire, 30u, target, 0u, 0u, 0u));
        CHECK(av_d3d_Finish11(wire, &removed));
        CHECK_EQ(removed, 0u);
        platform.fail_allocation_at(1u);
        const std::uint32_t rotated[2] = {1u, 30u};
        CHECK(av_d3d_RotateResources11(wire, 2u, rotated));
        CHECK(av_d3d_Finish11(wire, &removed));
        CHECK_EQ(removed, out_of_memory);

        CHECK(av_d3d_CreateDevice11(wire, 5u, 0u, 0u, &hresult, &level));
        CHECK(av_d3d_SetShader(wire, 4u, 99u));
        CHECK(av_d3d_Finish11(wire, &removed));
        CHECK_EQ(removed, driver_internal_error);

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
