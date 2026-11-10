#include "executor/d3d/shared_surfaces.hpp"
#include "fixtures/d3d/triangle_ps.hpp"
#include "fixtures/d3d/triangle_vs.hpp"
#include "core/support/object_storage.hpp"
#include "executor/d3d/d3d11_tiles.hpp"
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
#include <bit>
#include <cmath>
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

    static std::uint8_t pieces[size * size * 4u];
    constexpr std::uint64_t first_piece = size * 4u * 3u + 5u;
    std::uint64_t first_read = 0u, second_read = 0u;
    CHECK(av_d3d_ReadSubresourceRange(wire, 1u, 0u, 0u, &hresult, &row_pitch, &depth_pitch, pieces, first_piece, &first_read));
    CHECK_EQ(hresult, 0u);
    CHECK_EQ(first_read, first_piece);
    CHECK(av_d3d_ReadSubresourceRange(wire, 1u, 0u, first_piece, &hresult, &row_pitch, &depth_pitch, pieces + first_piece,
                                      sizeof(pieces), &second_read));
    CHECK_EQ(hresult, 0u);
    CHECK_EQ(second_read, sizeof(pieces) - first_piece);
    CHECK(std::memcmp(pieces, pixels, sizeof(pieces)) == 0);
    CHECK(av_d3d_ReadSubresourceRange(wire, 1u, 0u, sizeof(pieces) + 1u, &hresult, &row_pitch, &depth_pitch, pieces, 16u,
                                      &second_read));
    CHECK_EQ(hresult, static_cast<std::uint32_t>(0x80070057u));
    CHECK_EQ(second_read, std::uint64_t{0});
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

TEST(d3d_backend_device, a_tile_mapped_and_filled_reads_back_through_a_copy_to_a_buffer)
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
    Caps11 caps = {};
    static FormatCaps formats[256];
    std::uint64_t answered = 0u;
    CHECK(av_d3d_QueryCaps11(wire, &caps, formats, sizeof(formats), &answered));
    if (caps.tiled_resources_tier == 0u) {
        std::fprintf(stderr, "this GPU has no sparse binding under DXVK: nothing judged\n");
        backend.shut_down();
        return;
    }
    constexpr std::uint32_t tile_bytes = av::D3d11Tiles::tile_bytes;
    constexpr std::uint32_t pool_bit = 0x20000u;
    constexpr std::uint32_t tiled_bit = 0x40000u;
    constexpr std::uint32_t format_r8g8b8a8 = 28u;
    constexpr std::uint32_t linear_to_tiled = 0x2u;
    constexpr std::uint32_t tiled_to_linear = 0x4u;
    ResourceDesc pool = {};
    pool.dimension = 1u;
    pool.width = 2u * tile_bytes;
    pool.misc_flags = pool_bit;
    CHECK(av_d3d_CreateResource(wire, 1u, pool, 0u, 0u, 0u));
    ResourceDesc tiled = {};
    tiled.dimension = 3u;
    tiled.width = 128u;
    tiled.height = 128u;
    tiled.depth_or_array = 1u;
    tiled.mip_levels = 1u;
    tiled.format = format_r8g8b8a8;
    tiled.sample_count = 1u;
    tiled.misc_flags = tiled_bit;
    CHECK(av_d3d_CreateResource(wire, 2u, tiled, 0u, 0u, 0u));
    ResourceDesc linear = {};
    linear.dimension = 1u;
    linear.width = tile_bytes;
    CHECK(av_d3d_CreateResource(wire, 3u, linear, 0u, 0u, 0u));
    CHECK_EQ(wire.refused(), 0u);

    std::uint32_t tiles = 0u, packed_mips = 7u, packed_tiles = 7u;
    CHECK(av_d3d_ResourceTiling11(wire, 2u, &tiles, &packed_mips, &packed_tiles));
    CHECK_EQ(tiles, 1u);
    CHECK_EQ(packed_mips, 0u);
    CHECK_EQ(packed_tiles, 0u);

    const TileRegion region = {{0u, 0u, 0u, 0u}, {1u, 0u, 0u, 0u, 0u}};
    const TileRange  range = {0u, 1u, 1u};
    CHECK(av_d3d_UpdateTileMappings11(wire, 2u, 1u, &region, 1u, 1u, &range, 0u));
    static std::uint8_t pattern[tile_bytes];
    for (std::uint32_t index = 0u; index < tile_bytes; index++) pattern[index] = static_cast<std::uint8_t>(index * 7u + 3u);
    CHECK(av_d3d_UpdateTiles11(wire, 2u, region.start, region.size, 0u, pattern, sizeof(pattern)));
    CHECK(av_d3d_CopyTiles11(wire, 2u, region.start, region.size, 3u, 0u, tiled_to_linear));
    ResourceDesc half = {};
    half.dimension = 1u;
    half.width = tile_bytes / 2u;
    CHECK(av_d3d_CreateResource(wire, 4u, half, 0u, 0u, 0u));
    static std::uint8_t back[tile_bytes / 2u];
    for (std::uint32_t part = 0u; part < 2u; part++) {
        const Box piece = {part * tile_bytes / 2u, 0u, 0u, (part + 1u) * tile_bytes / 2u, 1u, 1u};
        CHECK(av_d3d_CopySubresourceRegion(wire, 4u, 0u, 0u, 0u, 0u, 3u, 0u, 1u, piece, 0u));
        std::uint32_t row_pitch = 0u, depth_pitch = 0u;
        std::uint64_t read = 0u;
        CHECK(av_d3d_ReadSubresource(wire, 4u, 0u, &hresult, &row_pitch, &depth_pitch, back, sizeof(back), &read));
        CHECK_EQ(hresult, 0u);
        CHECK_EQ(read, static_cast<std::uint64_t>(tile_bytes / 2u));
        CHECK(std::memcmp(pattern + part * tile_bytes / 2u, back, sizeof(back)) == 0);
    }
    CHECK_EQ(wire.refused(), 0u);

    const TileCoordinate whole_start = {0u, 0u, 0u, 0u};
    CHECK(av_d3d_CopyTileMappings11(wire, 2u, whole_start, 2u, whole_start, region.size, 0u));
    CHECK(av_d3d_ResizeTilePool11(wire, 1u, tile_bytes));
    CHECK(av_d3d_TiledResourceBarrier11(wire, 2u, 3u));
    CHECK(av_d3d_CopyTiles11(wire, 2u, region.start, region.size, 3u, 0u, linear_to_tiled));
    CHECK_EQ(wire.refused(), 0u);

    const TileRegion empty = {{0u, 0u, 0u, 0u}, {0u, 0u, 0u, 0u, 0u}};
    CHECK(av_d3d_UpdateTileMappings11(wire, 2u, 1u, &empty, 1u, 1u, &range, 0u));
    std::uint32_t removed = 1u;
    CHECK(av_d3d_Finish11(wire, &removed));
    CHECK_EQ(removed, driver_internal_error);
    CHECK(backend.invariants_hold());
    backend.shut_down();
}

TEST(d3d_backend_device, a_pipeline_statistics_query_counts_the_vertices_and_primitives_of_the_draw_it_brackets)
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
    bind_triangle(wire);
    constexpr std::uint32_t query_pipeline_statistics = 4u;
    CHECK(av_d3d_CreateQuery(wire, 50u, query_pipeline_statistics, 0u));
    CHECK(av_d3d_BeginQuery(wire, 50u));
    CHECK(av_d3d_Draw(wire, 3u, 0u));
    CHECK(av_d3d_EndQuery(wire, 50u));
    std::uint64_t counts[16] = {};
    std::uint32_t ready = 0u;
    std::uint64_t answered = 0u;
    for (int attempt = 0; attempt < 200 && ready == 0u; attempt++)
        CHECK(av_d3d_GetQueryData(wire, 50u, 0u, &ready, counts, sizeof(counts), &answered));
    CHECK_EQ(ready, 1u);
    CHECK_EQ(answered, 88u);
    CHECK_EQ(counts[0], 3u);
    CHECK_EQ(counts[1], 1u);
    CHECK_EQ(counts[2], 3u);
    std::uint32_t removed = 1u;
    CHECK(av_d3d_Finish11(wire, &removed));
    CHECK_EQ(removed, 0u);
    backend.shut_down();
}

TEST(d3d_backend_device, a_pipeline_statistics_query_counts_every_draw_it_brackets_instanced_ones_too)
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
    bind_triangle(wire);
    constexpr std::uint32_t query_pipeline_statistics = 4u;
    CHECK(av_d3d_CreateQuery(wire, 50u, query_pipeline_statistics, 0u));
    CHECK(av_d3d_BeginQuery(wire, 50u));
    CHECK(av_d3d_Draw(wire, 3u, 0u));
    CHECK(av_d3d_DrawInstanced(wire, 3u, 2u, 0u, 0u));
    CHECK(av_d3d_EndQuery(wire, 50u));
    std::uint64_t counts[16] = {};
    std::uint32_t ready = 0u;
    std::uint64_t answered = 0u;
    for (int attempt = 0; attempt < 200 && ready == 0u; attempt++)
        CHECK(av_d3d_GetQueryData(wire, 50u, 0u, &ready, counts, sizeof(counts), &answered));
    CHECK_EQ(ready, 1u);
    CHECK_EQ(counts[0], 9u);
    CHECK_EQ(counts[1], 3u);
    CHECK_EQ(counts[2], 9u);
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

namespace {

void stream_vertices_as(av::d3d::PassthroughPrimitive primitive, std::uint32_t topology, bool append = false, bool draw_auto = false)
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
    av::d3d::PassthroughGeometryShader shader(primitive);
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
    const std::uint32_t passes = append ? 2u : 1u;
    buffer.width = passes * 3u * stride;
    buffer.bind_flags = draw_auto ? 0x11u : 0x10u;
    CHECK(av_d3d_CreateResource(wire, out, buffer, 0u, 0u, 0u));
    bind_triangle(wire);
    CHECK(av_d3d_SetTopology(wire, topology));
    CHECK(av_d3d_SetShader(wire, stage_geometry, gs));
    const StreamOutTarget target = {out, 0u};
    CHECK(av_d3d_SetStreamOutTargets(wire, 1u, &target));
    CHECK(av_d3d_Draw(wire, 3u, 0u));
    if (append) {
        const StreamOutTarget appended = {out, 0xFFFFFFFFu};
        CHECK(av_d3d_SetStreamOutTargets(wire, 1u, &appended));
        CHECK(av_d3d_Draw(wire, 3u, 0u));
    }
    if (draw_auto) {
        CHECK(av_d3d_SetStreamOutTargets(wire, 0u, nullptr));
        CHECK(av_d3d_SetShader(wire, stage_geometry, 0u));
        const VertexBufferBinding from_stream = {out, stride, 0u};
        CHECK(av_d3d_SetVertexBuffers(wire, 0u, 1u, &from_stream));
        CHECK(av_d3d_DrawAuto(wire));
        std::uint32_t removed_by_draw_auto = 1u;
        CHECK(av_d3d_Finish11(wire, &removed_by_draw_auto));
        CHECK_EQ(removed_by_draw_auto, 0u);
    }
    static float streamed_out[2u * 3u * 8u];
    std::uint32_t row_pitch = 0u, depth_pitch = 0u;
    std::uint64_t read = 0u;
    CHECK(av_d3d_ReadSubresource(wire, out, 0u, &hresult, &row_pitch, &depth_pitch, streamed_out, sizeof streamed_out,
                                 &read));
    CHECK_EQ(hresult, 0u);
    const float expected[3u * 8u] = {-1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
                                     -1.0f, 1.0f,  0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
                                     0.0f,  1.0f,  0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f};
    std::uint32_t wrong = 0u;
    for (std::uint32_t index = 0u; index < passes * 3u * 8u; index++) {
        if (streamed_out[index] != expected[index % (3u * 8u)]) {
            if (wrong < 8u) std::fprintf(stderr, "float %u: %f, not %f\n", index, streamed_out[index], expected[index % (3u * 8u)]);
            wrong++;
        }
    }
    CHECK_EQ(wrong, 0u);
    CHECK_EQ(wire.refused(), 0u);
}

}

TEST(d3d_backend_device, a_stream_output_with_no_shader_streams_what_the_vertex_shader_wrote)
{ stream_vertices_as(av::d3d::PassthroughPrimitive::point, 1u); }

TEST(d3d_backend_device, a_stream_output_of_a_triangle_list_streams_every_vertex_through_the_triangle_variant)
{ stream_vertices_as(av::d3d::PassthroughPrimitive::triangle, 4u); }

TEST(d3d_backend_device, a_second_draw_with_the_append_offset_streams_after_the_first)
{ stream_vertices_as(av::d3d::PassthroughPrimitive::triangle, 4u, true); }

TEST(d3d_backend_device, a_draw_auto_over_what_the_stream_output_wrote_keeps_the_device)
{ stream_vertices_as(av::d3d::PassthroughPrimitive::triangle, 4u, false, true); }

TEST(d3d_backend_device, a_stream_output_of_a_triangle_strip_streams_every_vertex_through_the_triangle_variant)
{ stream_vertices_as(av::d3d::PassthroughPrimitive::triangle, 5u); }


namespace {

constexpr std::uint32_t four_stream_tokens[] = {0x00020050u, 0x000000c8u, 0x0100086au, 0x05000061u, 0x002010f2u, 0x00000001u, 0x00000000u, 0x00000001u, 0x0400005fu, 0x00201072u, 0x00000001u, 0x00000001u, 0x02000068u, 0x00000001u, 0x0100085du, 0x0300008fu, 0x00110000u, 0x00000000u, 0x0100085cu, 0x04000067u, 0x001020f2u, 0x00000000u, 0x00000001u, 0x03000065u, 0x00102072u, 0x00000001u, 0x0300008fu, 0x00110000u, 0x00000001u, 0x0100085cu, 0x04000067u, 0x001020f2u, 0x00000000u, 0x00000001u, 0x03000065u, 0x00102072u, 0x00000001u, 0x0300008fu, 0x00110000u, 0x00000002u, 0x0100085cu, 0x04000067u, 0x001020f2u, 0x00000000u, 0x00000001u, 0x03000065u, 0x00102072u, 0x00000001u, 0x0300008fu, 0x00110000u, 0x00000003u, 0x0100085cu, 0x04000067u, 0x001020f2u, 0x00000000u, 0x00000001u, 0x03000065u, 0x00102072u, 0x00000001u, 0x0200005eu, 0x0000000cu, 0x06000036u, 0x00102032u, 0x00000000u, 0x00201046u, 0x00000000u, 0x00000000u, 0x08000036u, 0x001020c2u, 0x00000000u, 0x00004002u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x06000036u, 0x00102072u, 0x00000001u, 0x00201246u, 0x00000000u, 0x00000001u, 0x03000075u, 0x00110000u, 0x00000000u, 0x0b000038u, 0x00100032u, 0x00000000u, 0x00004002u, 0x3f000000u, 0x3f000000u, 0x00000000u, 0x00000000u, 0x00201046u, 0x00000000u, 0x00000000u, 0x05000036u, 0x00102032u, 0x00000000u, 0x00100046u, 0x00000000u, 0x08000036u, 0x001020c2u, 0x00000000u, 0x00004002u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x06000036u, 0x00102072u, 0x00000001u, 0x00201496u, 0x00000000u, 0x00000001u, 0x03000075u, 0x00110000u, 0x00000001u, 0x10000032u, 0x00100032u, 0x00000000u, 0x00201046u, 0x00000000u, 0x00000000u, 0x00004002u, 0x3e800000u, 0x3e800000u, 0x00000000u, 0x00000000u, 0x00004002u, 0x3f000000u, 0xbf000000u, 0x00000000u, 0x00000000u, 0x05000036u, 0x00102032u, 0x00000000u, 0x00100046u, 0x00000000u, 0x08000036u, 0x001020c2u, 0x00000000u, 0x00004002u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x06000036u, 0x00102072u, 0x00000001u, 0x00201926u, 0x00000000u, 0x00000001u, 0x03000075u, 0x00110000u, 0x00000002u, 0x0b000038u, 0x00100032u, 0x00000000u, 0x00004002u, 0x3f8ccccdu, 0x3f8ccccdu, 0x00000000u, 0x00000000u, 0x00201046u, 0x00000000u, 0x00000000u, 0x05000036u, 0x00102032u, 0x00000000u, 0x00100046u, 0x00000000u, 0x08000036u, 0x001020c2u, 0x00000000u, 0x00004002u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x0c000000u, 0x00100072u, 0x00000000u, 0x00004002u, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x00000000u, 0x80201246u, 0x00000041u, 0x00000000u, 0x00000001u, 0x05000036u, 0x00102072u, 0x00000001u, 0x00100246u, 0x00000000u, 0x03000075u, 0x00110000u, 0x00000003u, 0x0100003eu};

}

TEST(d3d_backend_device, a_geometry_shader_streams_each_of_its_four_streams_to_its_own_buffer)
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
    SignatureEntry outputs[8] = {};
    StreamOutEntry streamed[8] = {};
    for (std::uint32_t stream = 0u; stream < 4u; stream++) {
        outputs[stream * 2u] = SignatureEntry{1u, 0u, 15u, 3u, 0u, stream};
        outputs[stream * 2u + 1u] = SignatureEntry{0u, 1u, 7u, 3u, 0u, stream};
        streamed[stream * 2u] = StreamOutEntry{stream, stream, 0u, 0xFu};
        streamed[stream * 2u + 1u] = StreamOutEntry{stream, stream, 1u, 0x7u};
    }
    const std::uint32_t strides[4] = {28u, 28u, 28u, 28u};
    constexpr std::uint32_t stage_geometry = 3u, gs = 50u, out = 60u, no_rasterized_stream = 0xFFFFFFFFu;
    CHECK(av_d3d_CreateShader(wire, gs, stage_geometry, triangle_vs::outputs_count, triangle_vs::outputs, 8u, outputs, 0u,
                              nullptr, 8u, streamed, 4u, strides, no_rasterized_stream, four_stream_tokens,
                              sizeof four_stream_tokens));
    CHECK_EQ(wire.refused(), 0u);
    ResourceDesc buffer = {};
    buffer.dimension = 1u;
    buffer.width = 3u * 28u;
    buffer.bind_flags = 0x10u;
    for (std::uint32_t stream = 0u; stream < 4u; stream++) CHECK(av_d3d_CreateResource(wire, out + stream, buffer, 0u, 0u, 0u));
    bind_triangle(wire);
    CHECK(av_d3d_SetTopology(wire, 1u));
    CHECK(av_d3d_SetShader(wire, stage_geometry, gs));
    const StreamOutTarget targets[4] = {{out, 0u}, {out + 1u, 0u}, {out + 2u, 0u}, {out + 3u, 0u}};
    CHECK(av_d3d_SetStreamOutTargets(wire, 4u, targets));
    CHECK(av_d3d_Draw(wire, 3u, 0u));
    CHECK(av_d3d_SetStreamOutTargets(wire, 0u, nullptr));
    for (std::uint32_t stream = 0u; stream < 4u; stream++) {
        static float streamed_out[3u * 7u];
        std::uint32_t row_pitch = 0u, depth_pitch = 0u;
        std::uint64_t read = 0u;
        CHECK(av_d3d_ReadSubresource(wire, out + stream, 0u, &hresult, &row_pitch, &depth_pitch, streamed_out,
                                     sizeof streamed_out, &read));
        CHECK_EQ(hresult, 0u);
        float sum = 0.0f;
        for (const float value : streamed_out) sum += value < 0.0f ? -value : value;
        std::fprintf(stderr, "stream %u: sum %f first %f %f %f %f\n", stream, sum, streamed_out[0], streamed_out[1], streamed_out[2], streamed_out[3]);
        CHECK(sum > 0.0f);
    }
    CHECK_EQ(wire.refused(), 0u);
}


namespace {

constexpr std::uint32_t rov_tokens[] = {0x00000050u, 0x0000004au, 0x0100086au, 0x0402189cu, 0x0011e000u, 0x00000001u, 0x00005555u, 0x04002064u, 0x00101032u, 0x00000000u, 0x00000001u, 0x03001062u, 0x00101012u, 0x00000001u, 0x03000065u, 0x001020f2u, 0x00000000u, 0x02000068u, 0x00000002u, 0x0500001cu, 0x001000f2u, 0x00000000u, 0x00101546u, 0x00000000u, 0x890000a3u, 0x800000c2u, 0x00155543u, 0x00100012u, 0x00000001u, 0x00100fc6u, 0x00000000u, 0x0011ee46u, 0x00000001u, 0x09000032u, 0x00100022u, 0x00000001u, 0x0010100au, 0x00000001u, 0x00004001u, 0x3f333333u, 0x00004001u, 0x3e99999au, 0x07000038u, 0x00100022u, 0x00000001u, 0x0010001au, 0x00000001u, 0x00004001u, 0x3eb33333u, 0x09000032u, 0x00100012u, 0x00000001u, 0x0010000au, 0x00000001u, 0x00004001u, 0x3f000000u, 0x0010001au, 0x00000001u, 0x070000a4u, 0x0011e0f2u, 0x00000001u, 0x00100e46u, 0x00000000u, 0x00100006u, 0x00000001u, 0x08000036u, 0x001020f2u, 0x00000000u, 0x00004002u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x0100003eu};

}

TEST(d3d_backend_device, a_rasterizer_ordered_view_blends_overlapping_triangles_in_draw_order)
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
    constexpr std::uint32_t layers = 6u, format_r32_float = 41u, bind_unordered = 0x80u, uav_texture2d = 4u;
    ResourceDesc texture = {};
    texture.dimension = 3u;
    texture.width = size;
    texture.height = size;
    texture.depth_or_array = 1u;
    texture.mip_levels = 1u;
    texture.format = format_r32_float;
    texture.sample_count = 1u;
    texture.bind_flags = bind_unordered;
    CHECK(av_d3d_CreateResource(wire, 20u, texture, 0u, 0u, 0u));
    ViewDesc uav = {};
    uav.resource = 20u;
    uav.format = format_r32_float;
    uav.dimension = uav_texture2d;
    CHECK(av_d3d_CreateUnorderedAccessView(wire, 21u, uav));
    const SignatureEntry rov_inputs[2] = {{1u, 0u, 15u, 3u, 0u, 0u}, {0u, 1u, 15u, 3u, 0u, 0u}};
    const SignatureEntry rov_outputs[1] = {{0u, 0u, 15u, 3u, 0u, 0u}};
    CHECK(av_d3d_CreateShader(wire, 22u, 4u, 2u, rov_inputs, 1u, rov_outputs, 0u, nullptr, 0u, nullptr, 0u, nullptr,
                              0xffffffffu, rov_tokens, sizeof rov_tokens));
    Vertex vertices[3u * layers] = {};
    for (std::uint32_t layer = 0u; layer < layers; layer++) {
        const float radius = 0.9f - 0.08f * float(layer);
        for (std::uint32_t corner = 0u; corner < 3u; corner++) {
            const float angle = float(layer) * 0.7f - float(corner) * 2.0943951f;
            vertices[layer * 3u + corner] = Vertex{radius * std::cos(angle), radius * std::sin(angle), float(layer) * 0.19f, 0.0f, 0.0f, 1.0f};
        }
    }
    ResourceDesc buffer = {};
    buffer.dimension = 1u;
    buffer.width = sizeof vertices;
    buffer.bind_flags = bind_vertex;
    CHECK(av_d3d_CreateResource(wire, 23u, buffer, 0u, 0u, 0u));
    CHECK(av_d3d_UpdateSubresource(wire, 23u, 0u, 0u, Box{}, 0u, 0u, 0u, vertices, sizeof vertices));
    const std::uint32_t targets[1] = {2u};
    const UavBinding uavs[1] = {{21u, 0xFFFFFFFFu}};
    CHECK(av_d3d_SetRenderTargets(wire, 1u, targets, 0u, 1u, 1u, uavs));
    const Viewport viewport = {0.0f, 0.0f, float(size), float(size), 0.0f, 1.0f};
    CHECK(av_d3d_SetViewports(wire, 1u, &viewport));
    CHECK(av_d3d_SetInputLayout(wire, 7u));
    const VertexBufferBinding binding = {23u, sizeof(Vertex), 0u};
    CHECK(av_d3d_SetVertexBuffers(wire, 0u, 1u, &binding));
    CHECK(av_d3d_SetTopology(wire, topology_triangles));
    CHECK(av_d3d_SetShader(wire, 0u, 5u));
    CHECK(av_d3d_SetShader(wire, 4u, 22u));
    CHECK(av_d3d_Draw(wire, 3u * layers, 0u));
    static float result[size * size];
    std::uint32_t row_pitch = 0u, depth_pitch = 0u;
    std::uint64_t read = 0u;
    CHECK(av_d3d_ReadSubresource(wire, 20u, 0u, &hresult, &row_pitch, &depth_pitch, result, sizeof result, &read));
    CHECK_EQ(hresult, 0u);
    std::uint32_t wrong = 0u, judged = 0u;
    for (std::uint32_t y = 0u; y < size; y++)
        for (std::uint32_t x = 0u; x < size; x++) {
            const float px = (float(x) + 0.5f) / float(size) * 2.0f - 1.0f, py = 1.0f - (float(y) + 0.5f) / float(size) * 2.0f;
            float expected = 0.0f;
            bool near_edge = false;
            for (std::uint32_t layer = 0u; layer < layers; layer++) {
                float d[3];
                for (std::uint32_t corner = 0u; corner < 3u; corner++) {
                    const Vertex &a = vertices[layer * 3u + corner], &b = vertices[layer * 3u + (corner + 1u) % 3u];
                    d[corner] = (b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x);
                }
                for (const float value : d) near_edge = near_edge || std::fabs(value) < 0.05f;
                const bool inside = (d[0] > 0 && d[1] > 0 && d[2] > 0) || (d[0] < 0 && d[1] < 0 && d[2] < 0);
                if (inside) expected = expected * 0.5f + 0.35f * (0.3f + 0.7f * float(layer) * 0.19f);
            }
            if (near_edge) continue;
            judged++;
            if (std::fabs(result[y * size + x] - expected) > 1e-4f) wrong++;
        }
    std::fprintf(stderr, "rov: %u of %u judged pixels wrong\n", wrong, judged);
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

TEST(d3d_backend_device, a_staging_allocation_the_host_refuses_faults_its_session_and_not_the_device_others_share)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *const device = host_device(apis);
    if (device == nullptr) return;
    constexpr std::uint32_t out_of_memory = 0x8007000Eu;
    av::HostRamLedger *const host = apis.host_ram_ledger();
    const std::uint64_t kept = host->budget();
    {
        HeldBackend     greedy(first_backend, rig.log);
        HeldBackend     bystander(second_backend, rig.log);
        av::SessionShares unshared_a, unshared_b;
        CHECK(greedy.backend().initialize(unshared_a, rig.imports, device));
        CHECK(bystander.backend().initialize(unshared_b, rig.imports, device));
        av::D3dLoopback greedy_wire(greedy.backend(), rig.imports);
        av::D3dLoopback bystander_wire(bystander.backend(), rig.imports);
        std::uint32_t hresult = 1u, level = 0u, removed = 1u;
        CHECK(av_d3d_CreateDevice11(greedy_wire, 1u, 0u, 0u, &hresult, &level));
        CHECK(av_d3d_CreateDevice11(bystander_wire, 1u, 0u, 0u, &hresult, &level));
        ResourceDesc buffer = {};
        buffer.dimension = 1u;
        buffer.width = 1u << 20u;
        buffer.bind_flags = bind_vertex;
        CHECK(av_d3d_CreateResource(greedy_wire, 100u, buffer, 0u, 0u, 0u));
        host->budget(4096u);
        static std::uint8_t bytes[4096u];
        const Box head = {0u, 0u, 0u, sizeof(bytes), 1u, 1u};
        CHECK(av_d3d_UpdateSubresource(greedy_wire, 100u, 0u, 1u, head, 0u, 0u, 0u, bytes, sizeof(bytes)));
        CHECK(av_d3d_Finish11(greedy_wire, &removed));
        host->budget(kept);
        CHECK_EQ(removed, out_of_memory);
        CHECK(av_d3d_Finish11(bystander_wire, &removed));
        CHECK_EQ(removed, 0u);
        make_triangle(bystander_wire);
        CHECK_EQ(bystander_wire.refused(), 0u);
        bind_triangle(bystander_wire);
        CHECK(av_d3d_Draw(bystander_wire, 3u, 0u));
        CHECK(av_d3d_Finish11(bystander_wire, &removed));
        CHECK_EQ(removed, 0u);
        greedy.backend().shut_down();
        bystander.backend().shut_down();
    }
}

namespace {

constexpr std::uint32_t query_occlusion_predicate = 5u;
constexpr std::uint32_t format_r32_uint = 42u;
constexpr std::uint32_t format_d32_float = 40u;
constexpr std::uint32_t bind_depth_stencil = 0x40u;
constexpr std::uint32_t bind_unordered_access = 0x80u;
constexpr std::uint32_t dsv_texture2d = 3u;
constexpr std::uint32_t uav_texture2d = 4u;
constexpr std::uint32_t clear_depth = 0x1u;
constexpr std::uint32_t predicate_query = 50u;

ResourceDesc texture_desc(std::uint32_t format, std::uint32_t bind_flags, std::uint32_t samples = 1u)
{
    ResourceDesc desc = {};
    desc.dimension = 3u;
    desc.width = size;
    desc.height = size;
    desc.depth_or_array = 1u;
    desc.mip_levels = 1u;
    desc.format = format;
    desc.sample_count = samples;
    desc.bind_flags = bind_flags;
    return desc;
}

ViewDesc view_desc(std::uint32_t resource, std::uint32_t format, std::uint32_t dimension)
{
    ViewDesc desc = {};
    desc.resource = resource;
    desc.format = format;
    desc.dimension = dimension;
    return desc;
}

std::uint32_t first_texel(av::D3dLoopback &wire, std::uint32_t resource)
{
    static std::uint8_t texels[size * size * 4u];
    std::uint32_t hresult = 1u, row_pitch = 0u, depth_pitch = 0u;
    std::uint64_t read = 0u;
    CHECK(av_d3d_ReadSubresource(wire, resource, 0u, &hresult, &row_pitch, &depth_pitch, texels, sizeof(texels), &read));
    CHECK_EQ(hresult, 0u);
    std::uint32_t value = 0u;
    std::memcpy(&value, texels + (size / 2u) * row_pitch + (size / 2u) * 4u, sizeof(value));
    return value;
}

void check_predicated_operations(bool drawn, std::uint32_t predicate_value)
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
    bind_triangle(wire);

    constexpr std::uint32_t cleared = 20u, copy_dst = 22u, copy_src = 24u, depth = 26u, storage = 28u, multisampled = 30u,
                            resolve_dst = 32u;
    CHECK(av_d3d_CreateResource(wire, cleared, texture_desc(format_b8g8r8a8, bind_render_target), 0u, 0u, 0u));
    CHECK(av_d3d_CreateRenderTargetView(wire, 21u, view_desc(cleared, format_b8g8r8a8, rtv_texture2d)));
    CHECK(av_d3d_CreateResource(wire, copy_dst, texture_desc(format_b8g8r8a8, bind_render_target), 0u, 0u, 0u));
    CHECK(av_d3d_CreateRenderTargetView(wire, 23u, view_desc(copy_dst, format_b8g8r8a8, rtv_texture2d)));
    CHECK(av_d3d_CreateResource(wire, copy_src, texture_desc(format_b8g8r8a8, bind_render_target), 0u, 0u, 0u));
    CHECK(av_d3d_CreateRenderTargetView(wire, 25u, view_desc(copy_src, format_b8g8r8a8, rtv_texture2d)));
    CHECK(av_d3d_CreateResource(wire, depth, texture_desc(format_d32_float, bind_depth_stencil), 0u, 0u, 0u));
    CHECK(av_d3d_CreateDepthStencilView(wire, 27u, view_desc(depth, format_d32_float, dsv_texture2d)));
    CHECK(av_d3d_CreateResource(wire, storage, texture_desc(format_r32_uint, bind_unordered_access), 0u, 0u, 0u));
    CHECK(av_d3d_CreateUnorderedAccessView(wire, 29u, view_desc(storage, format_r32_uint, uav_texture2d)));
    CHECK(av_d3d_CreateResource(wire, multisampled, texture_desc(format_b8g8r8a8, bind_render_target, 4u), 0u, 0u, 0u));
    CHECK(av_d3d_CreateRenderTargetView(wire, 31u, view_desc(multisampled, format_b8g8r8a8, 6u)));
    CHECK(av_d3d_CreateResource(wire, resolve_dst, texture_desc(format_b8g8r8a8, bind_render_target), 0u, 0u, 0u));
    CHECK(av_d3d_CreateRenderTargetView(wire, 33u, view_desc(resolve_dst, format_b8g8r8a8, rtv_texture2d)));
    CHECK_EQ(wire.refused(), 0u);

    const Float4 red = {{1.0f, 0.0f, 0.0f, 1.0f}}, green = {{0.0f, 1.0f, 0.0f, 1.0f}}, blue = {{0.0f, 0.0f, 1.0f, 1.0f}},
                 yellow = {{1.0f, 1.0f, 0.0f, 1.0f}};
    CHECK(av_d3d_ClearRenderTargetView(wire, 21u, red));
    CHECK(av_d3d_ClearRenderTargetView(wire, 23u, blue));
    CHECK(av_d3d_ClearRenderTargetView(wire, 25u, yellow));
    CHECK(av_d3d_ClearDepthStencilView(wire, 27u, clear_depth, 0.25f, 0u));
    CHECK(av_d3d_ClearUnorderedAccessViewUint(wire, 29u, Uint4{{7u, 7u, 7u, 7u}}));
    CHECK(av_d3d_ClearRenderTargetView(wire, 31u, red));
    CHECK(av_d3d_ClearRenderTargetView(wire, 33u, blue));

    CHECK(av_d3d_CreateQuery(wire, predicate_query, query_occlusion_predicate, 0u));
    const Viewport away = {float(size) * 4.0f, 0.0f, float(size), float(size), 0.0f, 1.0f};
    const Viewport onto = {0.0f, 0.0f, float(size), float(size), 0.0f, 1.0f};
    CHECK(av_d3d_SetViewports(wire, 1u, drawn ? &onto : &away));
    CHECK(av_d3d_BeginQuery(wire, predicate_query));
    CHECK(av_d3d_Draw(wire, 3u, 0u));
    CHECK(av_d3d_EndQuery(wire, predicate_query));

    const bool performed = drawn != (predicate_value != 0u);
    CHECK(av_d3d_SetPredication(wire, predicate_query, predicate_value));
    CHECK(av_d3d_ClearRenderTargetView(wire, 21u, green));
    CHECK(av_d3d_CopySubresourceRegion(wire, copy_dst, 0u, 0u, 0u, 0u, copy_src, 0u, 0u, Box{}, 0u));
    CHECK(av_d3d_ClearDepthStencilView(wire, 27u, clear_depth, 0.75f, 0u));
    CHECK(av_d3d_ClearUnorderedAccessViewUint(wire, 29u, Uint4{{9u, 9u, 9u, 9u}}));
    CHECK(av_d3d_ResolveSubresource(wire, resolve_dst, 0u, multisampled, 0u, format_b8g8r8a8));
    CHECK(av_d3d_SetPredication(wire, 0u, 0u));

    CHECK_EQ(first_texel(wire, cleared), performed ? 0xff00ff00u : 0xffff0000u);
    CHECK_EQ(first_texel(wire, copy_dst), performed ? 0xffffff00u : 0xff0000ffu);
    CHECK_EQ(first_texel(wire, depth), std::bit_cast<std::uint32_t>(performed ? 0.75f : 0.25f));
    CHECK_EQ(first_texel(wire, storage), performed ? 9u : 7u);
    CHECK_EQ(first_texel(wire, resolve_dst), performed ? 0xffff0000u : 0xff0000ffu);
    CHECK_EQ(wire.refused(), 0u);

    std::uint32_t removed = 1u;
    CHECK(av_d3d_Finish11(wire, &removed));
    CHECK_EQ(removed, 0u);
    backend.shut_down();
}

}

TEST(d3d_backend_device, predicated_clears_copies_and_resolves_are_skipped_when_the_predicate_equals_the_value)
{
    check_predicated_operations(false, 0u);
    check_predicated_operations(true, 1u);
}

TEST(d3d_backend_device, predicated_clears_copies_and_resolves_run_when_the_predicate_differs_from_the_value)
{
    check_predicated_operations(false, 1u);
    check_predicated_operations(true, 0u);
}
