#include "core/log/driver_log.hpp"
#include "executor/d3d/d3d9_lanes.hpp"
#include "executor/d3d/d3d9_slices.hpp"
#include "executor/d3d/slice_storages.hpp"
#include "executor/eventfd_wake.hpp"
#include "executor/host_memory_source.hpp"
#include "executor/import_table.hpp"
#include "executor/source_memory.hpp"
#include "executor/sharing/host_shares.hpp"
#include "executor/sharing/session_shares.hpp"
#include "executor/d3d/d3d_backend.hpp"
#include "executor/shim/compositor_device.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/d3d9_host_device.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "executor/d3d/shared_surfaces.hpp"
#include "proto/resources/surface_point.hpp"
#include "harness/executor/d3d_loopback.hpp"
#include "harness/executor/executor_log_rig.hpp"
#include "executor/shim/host_heap.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/generated/d3d9/d3d9_dispatch_client.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace av {

struct D3dBackendProbe {
    static WatchedBlocks &blocks(D3dBackend &backend) { return backend.blocks_; }
};

struct WatchedBlocksProbe {
    static void draw_from(WatchedBlocks &blocks, BlockSource &source) { blocks.blocks_ = &source; }
};

}

namespace {

using namespace av::d3d9proto;

constexpr std::uint32_t format_a8r8g8b8 = 21u;
constexpr std::uint32_t kind_render_target = 1u;
constexpr std::uint32_t clear_target = 0x1u;
constexpr std::uint32_t size = 64u;
constexpr std::uint32_t red = 0xffff0000u;
constexpr std::uint32_t blue = 0xff0000ffu;
constexpr std::uint32_t invalid_call = 0x8876086cu;
constexpr std::uint32_t more_data = 0x88760867u;

enum Id : std::uint32_t { device = 1, target, other_device, other_target };

bool host_devices(av::LoaderHostApis &apis, av::D3d11HostDevice *&device11, av::D3d9HostDevice *&device9)
{
    if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
    apis.open();
    for (std::uint32_t index = 0u; index < apis.gpu_count(); index++) {
        device11 = apis.d3d11_device(index);
        device9 = apis.d3d9_device(index);
        if (device11 != nullptr && device9 != nullptr) return true;
    }
    std::fprintf(stderr, "no GPU here runs DXVK's d3d9: nothing judged\n");
    return false;
}

struct Pixels {
    std::uint32_t hresult = 1u, width = 0u, height = 0u, format = 0u, pitch = 0u;
    std::uint8_t  bytes[size * size * 4u] = {};
    std::uint64_t written = 0u;

    std::uint32_t at(std::uint32_t x, std::uint32_t y) const
    {
        std::uint32_t value = 0u;
        std::memcpy(&value, bytes + std::size_t{y} * pitch + std::size_t{x} * 4u, sizeof(value));
        return value;
    }
};

bool read(av::D3dLoopback &wire, std::uint32_t surface, Pixels &out)
{
    return av_d3d9_ReadSurface9(wire, surface, 0u, &out.hresult, &out.width, &out.height, &out.format, &out.pitch,
                                out.bytes, sizeof(out.bytes), &out.written);
}

}

TEST(d3d9_session_device, a_cleared_render_target_reads_back_as_cleared)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice    *device11 = nullptr;
    av::D3d9HostDevice     *device9 = nullptr;
    if (!host_devices(apis, device11, device9)) return;
    av::D3dBackend backend(rig.log);
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, rig.imports, device11, nullptr, nullptr, device9));
    av::D3dLoopback wire(backend, rig.imports);

    std::uint32_t hresult = 1u;
    CHECK(av_d3d9_CreateDevice9(wire, device, 0u, &hresult));
    CHECK_EQ(hresult, 0u);
    CHECK(av_d3d9_CreateSurface9(wire, target, kind_render_target, size, size, format_a8r8g8b8, 0u, 0u, 0u));
    CHECK(av_d3d9_SetRenderTarget9(wire, 0u, target, 0u));
    CHECK(av_d3d9_Clear9(wire, 0u, nullptr, clear_target, red, 1.0f, 0u));
    const Rect right = {static_cast<std::int32_t>(size / 2u), 0, static_cast<std::int32_t>(size),
                        static_cast<std::int32_t>(size)};
    CHECK(av_d3d9_Clear9(wire, 1u, &right, clear_target, blue, 1.0f, 0u));

    Pixels pixels;
    CHECK(read(wire, target, pixels));
    CHECK_EQ(pixels.hresult, 0u);
    CHECK_EQ(pixels.width, size);
    CHECK_EQ(pixels.height, size);
    CHECK_EQ(pixels.format, format_a8r8g8b8);
    CHECK(pixels.pitch >= size * 4u);
    CHECK_EQ(pixels.written, std::uint64_t{pixels.pitch} * size);
    CHECK_EQ(pixels.at(0u, 0u), red);
    CHECK_EQ(pixels.at(size / 2u - 1u, size - 1u), red);
    CHECK_EQ(pixels.at(size / 2u, 0u), blue);
    CHECK_EQ(pixels.at(size - 1u, size - 1u), blue);
    Pixels short_of_room;
    CHECK(av_d3d9_ReadSurface9(wire, target, 0u, &short_of_room.hresult, &short_of_room.width, &short_of_room.height,
                               &short_of_room.format, &short_of_room.pitch, short_of_room.bytes,
                               std::uint64_t{pixels.pitch} * (size - 1u), &short_of_room.written));
    CHECK_EQ(short_of_room.hresult, more_data);
    CHECK_EQ(short_of_room.written, 0u);
    CHECK(av_d3d9_Finish9(wire, &hresult));
    CHECK_EQ(hresult, 0u);
    CHECK_EQ(wire.refused(), 0u);
}

TEST(d3d9_session_device, another_devices_surface_is_refused_and_removes_the_device)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice    *device11 = nullptr;
    av::D3d9HostDevice     *device9 = nullptr;
    if (!host_devices(apis, device11, device9)) return;
    av::D3dBackend backend(rig.log);
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, rig.imports, device11, nullptr, nullptr, device9));
    av::D3dLoopback wire(backend, rig.imports);

    std::uint32_t hresult = 1u;
    CHECK(av_d3d9_CreateDevice9(wire, device, 0u, &hresult));
    CHECK_EQ(hresult, 0u);
    CHECK(av_d3d9_CreateSurface9(wire, target, kind_render_target, size, size, format_a8r8g8b8, 0u, 0u, 0u));
    CHECK(av_d3d9_CreateDevice9(wire, other_device, 0u, &hresult));
    CHECK_EQ(hresult, 0u);
    CHECK(av_d3d9_CreateSurface9(wire, other_target, kind_render_target, size, size, format_a8r8g8b8, 0u, 0u, 0u));

    CHECK(av_d3d9_SetRenderTarget9(wire, 0u, target, 0u));
    CHECK(av_d3d9_Finish9(wire, &hresult));
    CHECK(hresult != 0u);

    CHECK(av_d3d9_SelectDevice9(wire, device));
    CHECK(av_d3d9_Finish9(wire, &hresult));
    CHECK_EQ(hresult, 0u);
    CHECK(av_d3d9_SetRenderTarget9(wire, 0u, target, 0u));
    CHECK(av_d3d9_Clear9(wire, 0u, nullptr, clear_target, red, 1.0f, 0u));
    Pixels pixels;
    CHECK(read(wire, target, pixels));
    CHECK_EQ(pixels.hresult, 0u);
    CHECK_EQ(pixels.at(1u, 1u), red);
}

TEST(d3d9_session_device, a_surface_the_host_cannot_make_names_nothing)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice    *device11 = nullptr;
    av::D3d9HostDevice     *device9 = nullptr;
    if (!host_devices(apis, device11, device9)) return;
    av::D3dBackend backend(rig.log);
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, rig.imports, device11, nullptr, nullptr, device9));
    av::D3dLoopback wire(backend, rig.imports);

    std::uint32_t hresult = 1u;
    CHECK(av_d3d9_CreateDevice9(wire, device, 0u, &hresult));
    CHECK_EQ(hresult, 0u);
    CHECK(av_d3d9_CreateSurface9(wire, target, kind_render_target, 0u, size, format_a8r8g8b8, 0u, 0u, 0u));
    CHECK(av_d3d9_CreateSurface9(wire, target, 9u, size, size, format_a8r8g8b8, 0u, 0u, 0u));
    CHECK(av_d3d9_CreateSurface9(wire, target, kind_render_target, 0x10000u, size, format_a8r8g8b8, 0u, 0u, 0u));
    Pixels pixels;
    CHECK(read(wire, target, pixels));
    CHECK_EQ(pixels.hresult, invalid_call);
    CHECK_EQ(pixels.written, 0u);
}

namespace {

constexpr std::uint32_t format_dxt1 = 0x31545844u;
constexpr std::uint32_t pool_default = 0u;
constexpr std::uint32_t pool_managed = 1u;
constexpr std::uint32_t usage_write_only = 0x8u;
constexpr std::uint32_t triangle_list = 4u;
constexpr std::uint32_t render_state_cull = 22u;
constexpr std::uint32_t cull_none = 1u;
constexpr std::uint32_t render_state_lighting = 137u;
constexpr std::uint32_t fvf_xyzrhw_diffuse = 0x004u | 0x040u;
constexpr std::uint32_t decl_float4 = 3u;
constexpr std::uint32_t decl_d3dcolor = 4u;
constexpr std::uint32_t usage_position = 0u;
constexpr std::uint32_t usage_color = 10u;
constexpr std::uint32_t green = 0xff00ff00u;
constexpr std::uint32_t black = 0xff000000u;

enum DrawId : std::uint32_t {
    draw_device = 10, draw_target, vertices, declaration, vertex_shader, pixel_shader, texture, dxt_texture,
    plain_surface, lockable_target, unlockable_target
};

constexpr std::uint32_t pass_through_vs[] = {0xfffe0200u, 0x0200001fu, 0x80000000u, 0x900f0000u, 0x0200001fu,
                                             0x8000000au, 0x900f0001u, 0x02000001u, 0xc00f0000u, 0x90e40000u,
                                             0x02000001u, 0xd00f0000u, 0x90e40001u, 0x0000ffffu};
constexpr std::uint32_t pass_through_ps[] = {0xffff0200u, 0x0200001fu, 0x80000000u, 0x900f0000u,
                                             0x02000001u, 0x800f0800u, 0x90e40000u, 0x0000ffffu};

struct ClipVertex {
    float         x, y, z, w;
    std::uint32_t color;
};

struct ScreenVertex {
    float         x, y, z, rhw;
    std::uint32_t color;
};

void make_target(av::D3dLoopback &wire)
{
    std::uint32_t hresult = 1u;
    CHECK(av_d3d9_CreateDevice9(wire, draw_device, 0u, &hresult));
    CHECK_EQ(hresult, 0u);
    CHECK(av_d3d9_CreateSurface9(wire, draw_target, kind_render_target, size, size, format_a8r8g8b8, 0u, 0u, 0u));
    CHECK(av_d3d9_SetRenderTarget9(wire, 0u, draw_target, 0u));
    CHECK(av_d3d9_Clear9(wire, 0u, nullptr, clear_target, black, 1.0f, 0u));
    const std::uint32_t states[] = {render_state_cull, cull_none, render_state_lighting, 0u};
    CHECK(av_d3d9_SetRenderStates9(wire, 4u, states));
}

}

TEST(d3d9_session_device, a_shaded_triangle_lands_where_it_was_drawn)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice    *device11 = nullptr;
    av::D3d9HostDevice     *device9 = nullptr;
    if (!host_devices(apis, device11, device9)) return;
    av::D3dBackend backend(rig.log);
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, rig.imports, device11, nullptr, nullptr, device9));
    av::D3dLoopback wire(backend, rig.imports);
    make_target(wire);

    const ClipVertex triangle[3] = {{-1.0f, 1.0f, 0.5f, 1.0f, green},
                                    {1.0f, -1.0f, 0.5f, 1.0f, green},
                                    {-1.0f, -1.0f, 0.5f, 1.0f, green}};
    CHECK(av_d3d9_CreateVertexBuffer9(wire, vertices, sizeof(triangle), usage_write_only, 0u, pool_default));
    CHECK(av_d3d9_WriteBuffer9(wire, vertices, 0u, 0u, reinterpret_cast<const std::uint8_t *>(triangle),
                               sizeof(triangle)));
    const VertexElement elements[2] = {{0u, 0u, decl_float4, 0u, usage_position, 0u},
                                       {0u, 16u, decl_d3dcolor, 0u, usage_color, 0u}};
    CHECK(av_d3d9_CreateVertexDeclaration9(wire, declaration, 2u, elements));
    CHECK(av_d3d9_CreateVertexShader9(wire, vertex_shader, reinterpret_cast<const std::uint8_t *>(pass_through_vs),
                                      sizeof(pass_through_vs)));
    CHECK(av_d3d9_CreatePixelShader9(wire, pixel_shader, reinterpret_cast<const std::uint8_t *>(pass_through_ps),
                                     sizeof(pass_through_ps)));
    CHECK(av_d3d9_SetVertexDeclaration9(wire, declaration));
    CHECK(av_d3d9_SetVertexShader9(wire, vertex_shader));
    CHECK(av_d3d9_SetPixelShader9(wire, pixel_shader));
    CHECK(av_d3d9_SetStreamSource9(wire, 0u, vertices, 0u, sizeof(ClipVertex)));
    CHECK(av_d3d9_DrawPrimitive9(wire, triangle_list, 0u, 1u));

    Pixels pixels;
    CHECK(read(wire, draw_target, pixels));
    CHECK_EQ(pixels.hresult, 0u);
    CHECK_EQ(pixels.at(2u, size - 3u), green);
    CHECK_EQ(pixels.at(size - 3u, 2u), black);
    std::uint32_t hresult = 1u;
    CHECK(av_d3d9_Finish9(wire, &hresult));
    CHECK_EQ(hresult, 0u);
}

TEST(d3d9_session_device, a_pretransformed_triangle_needs_no_shader)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice    *device11 = nullptr;
    av::D3d9HostDevice     *device9 = nullptr;
    if (!host_devices(apis, device11, device9)) return;
    av::D3dBackend backend(rig.log);
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, rig.imports, device11, nullptr, nullptr, device9));
    av::D3dLoopback wire(backend, rig.imports);
    make_target(wire);

    const float               edge = static_cast<float>(size);
    const ScreenVertex triangle[3] = {{0.0f, 0.0f, 0.5f, 1.0f, green},
                                      {edge, edge, 0.5f, 1.0f, green},
                                      {0.0f, edge, 0.5f, 1.0f, green}};
    CHECK(av_d3d9_CreateVertexBuffer9(wire, vertices, sizeof(triangle), usage_write_only, fvf_xyzrhw_diffuse,
                                      pool_managed));
    CHECK(av_d3d9_WriteBuffer9(wire, vertices, 0u, 0u, reinterpret_cast<const std::uint8_t *>(triangle),
                               sizeof(triangle)));
    CHECK(av_d3d9_SetFvf9(wire, fvf_xyzrhw_diffuse));
    CHECK(av_d3d9_SetStreamSource9(wire, 0u, vertices, 0u, sizeof(ScreenVertex)));
    CHECK(av_d3d9_DrawPrimitive9(wire, triangle_list, 0u, 1u));

    Pixels pixels;
    CHECK(read(wire, draw_target, pixels));
    CHECK_EQ(pixels.at(2u, size - 3u), green);
    CHECK_EQ(pixels.at(size - 3u, 2u), black);
    CHECK_EQ(wire.refused(), 0u);
}

TEST(d3d9_session_device, a_texture_write_reads_back_where_it_was_written)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice    *device11 = nullptr;
    av::D3d9HostDevice     *device9 = nullptr;
    if (!host_devices(apis, device11, device9)) return;
    av::D3dBackend backend(rig.log);
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, rig.imports, device11, nullptr, nullptr, device9));
    av::D3dLoopback wire(backend, rig.imports);
    std::uint32_t hresult = 1u;
    CHECK(av_d3d9_CreateDevice9(wire, draw_device, 0u, &hresult));
    CHECK_EQ(hresult, 0u);

    CHECK(av_d3d9_CreateTexture9(wire, texture, 1u, 8u, 8u, 1u, 1u, 0u, format_a8r8g8b8, pool_managed));
    std::uint32_t zeros[64] = {};
    CHECK(av_d3d9_WriteTexture9(wire, texture, 0u, Box{0u, 0u, 0u, 8u, 8u, 1u}, 32u, 0u, 0u,
                                reinterpret_cast<const std::uint8_t *>(zeros), sizeof(zeros)));
    const std::uint32_t corner[4] = {red, blue, blue, red};
    CHECK(av_d3d9_WriteTexture9(wire, texture, 0u, Box{6u, 6u, 0u, 8u, 8u, 1u}, 8u, 0u, 0u,
                                reinterpret_cast<const std::uint8_t *>(corner), sizeof(corner)));
    Pixels pixels;
    CHECK(read(wire, texture, pixels));
    CHECK_EQ(pixels.hresult, 0u);
    CHECK_EQ(pixels.at(6u, 6u), red);
    CHECK_EQ(pixels.at(7u, 6u), blue);
    CHECK_EQ(pixels.at(6u, 7u), blue);
    CHECK_EQ(pixels.at(7u, 7u), red);
    CHECK_EQ(pixels.at(5u, 6u), 0u);

    CHECK(av_d3d9_CreateTexture9(wire, dxt_texture, 1u, 8u, 8u, 1u, 1u, 0u, format_dxt1, pool_managed));
    std::uint8_t blocks[32];
    for (std::uint32_t at = 0u; at < sizeof(blocks); at++) blocks[at] = static_cast<std::uint8_t>(at * 7u + 1u);
    CHECK(av_d3d9_WriteTexture9(wire, dxt_texture, 0u, Box{0u, 0u, 0u, 8u, 8u, 1u}, 16u, 0u, 0u, blocks,
                                sizeof(blocks)));
    Pixels compressed;
    CHECK(read(wire, dxt_texture, compressed));
    CHECK_EQ(compressed.hresult, 0u);
    CHECK_EQ(compressed.written, std::uint64_t{compressed.pitch} * 2u);
    CHECK(std::memcmp(compressed.bytes, blocks, 16u) == 0);
    CHECK(std::memcmp(compressed.bytes + compressed.pitch, blocks + 16u, 16u) == 0);

    CHECK(av_d3d9_WriteTexture9(wire, texture, 0u, Box{0u, 0u, 0u, 9u, 8u, 1u}, 36u, 0u, 0u,
                                reinterpret_cast<const std::uint8_t *>(zeros), sizeof(zeros)));
    CHECK(av_d3d9_Finish9(wire, &hresult));
    CHECK(hresult != 0u);
}

TEST(d3d9_session_device, a_surface_write_reads_back_where_it_was_written)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice    *device11 = nullptr;
    av::D3d9HostDevice     *device9 = nullptr;
    if (!host_devices(apis, device11, device9)) return;
    av::D3dBackend backend(rig.log);
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, rig.imports, device11, nullptr, nullptr, device9));
    av::D3dLoopback wire(backend, rig.imports);
    std::uint32_t hresult = 1u;
    CHECK(av_d3d9_CreateDevice9(wire, draw_device, 0u, &hresult));
    CHECK_EQ(hresult, 0u);

    constexpr std::uint32_t kind_offscreen_plain = 3u;
    constexpr std::uint32_t lockable = 0x1u;
    CHECK(av_d3d9_CreateSurface9(wire, plain_surface, kind_offscreen_plain, 8u, 8u, format_a8r8g8b8, 0u, 0u, 0u));
    CHECK(av_d3d9_CreateSurface9(wire, lockable_target, kind_render_target, 8u, 8u, format_a8r8g8b8, 0u, 0u,
                                 lockable));
    CHECK(av_d3d9_CreateSurface9(wire, unlockable_target, kind_render_target, 8u, 8u, format_a8r8g8b8, 0u, 0u, 0u));
    const std::uint32_t corner[4] = {red, blue, blue, red};
    for (const std::uint32_t surface : {plain_surface, lockable_target, unlockable_target}) {
        CHECK(av_d3d9_ColorFill9(wire, surface, 0u, Rect{0, 0, 0, 0}, black));
        CHECK(av_d3d9_WriteTexture9(wire, surface, 0u, Box{6u, 6u, 0u, 8u, 8u, 1u}, 8u, 0u, 0u,
                                    reinterpret_cast<const std::uint8_t *>(corner), sizeof(corner)));
        Pixels pixels;
        CHECK(read(wire, surface, pixels));
        CHECK_EQ(pixels.hresult, 0u);
        CHECK_EQ(pixels.at(6u, 6u), red);
        CHECK_EQ(pixels.at(7u, 6u), blue);
        CHECK_EQ(pixels.at(6u, 7u), blue);
        CHECK_EQ(pixels.at(7u, 7u), red);
        CHECK_EQ(pixels.at(5u, 6u), black);
        CHECK_EQ(pixels.at(0u, 0u), black);
    }
    CHECK(av_d3d9_Finish9(wire, &hresult));
    CHECK_EQ(hresult, 0u);
    CHECK_EQ(wire.refused(), 0u);

    CHECK(av_d3d9_WriteTexture9(wire, plain_surface, 0u, Box{6u, 6u, 0u, 9u, 8u, 1u}, 12u, 0u, 0u,
                                reinterpret_cast<const std::uint8_t *>(corner), sizeof(corner)));
    CHECK(av_d3d9_Finish9(wire, &hresult));
    CHECK(hresult != 0u);
}

TEST(d3d9_session_device, a_present_point_stages_the_frame_the_commands_before_it_drew)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d9HostDevice  *device9 = nullptr;
    if (!host_devices(apis, device11, device9)) return;
    constexpr std::uint64_t key = 0x9150u;
    constexpr std::uint32_t format_r5g6b5 = 23u;
    av::HostHeap            surfaces_heap;
    av::SharedSurfaces      surfaces(surfaces_heap);
    av::D3dBackend          backend(rig.log);
    av::HostShares *shares = nullptr;
    for (std::uint32_t index = 0u; index < apis.gpu_count(); index++)
        if (apis.d3d11_device(index) == device11) shares = apis.shares(index);
    CHECK(shares != nullptr);
    if (shares == nullptr) return;
    av::SessionShares session;
    session.bind(*shares, 1u, backend);
    CHECK(backend.initialize(session, rig.imports, device11, nullptr, &surfaces, device9));
    av::D3dLoopback wire(backend, rig.imports);
    std::uint32_t   hresult = 1u;
    CHECK(av_d3d9_CreateDevice9(wire, device, 0u, &hresult));
    CHECK(av_d3d9_CreateSurface9(wire, target, kind_render_target, size, size, format_a8r8g8b8, 0u, 0u, 0u));
    CHECK(av_d3d9_SetRenderTarget9(wire, 0u, target, 0u));
    CHECK(av_d3d9_Clear9(wire, 0u, nullptr, clear_target, red, 1.0f, 0u));
    const Rect right = {static_cast<std::int32_t>(size / 2u), 0, static_cast<std::int32_t>(size),
                        static_cast<std::int32_t>(size)};
    CHECK(av_d3d9_Clear9(wire, 1u, &right, clear_target, blue, 1.0f, 0u));
    CHECK(av_d3d9_PresentPoint9(wire, target, 0u, key, 1u));
    CHECK(av_d3d9_Clear9(wire, 0u, nullptr, clear_target, green, 1.0f, 0u));

    AvFrameView view = {};
    CHECK(surfaces.map(av::SurfacePoint(key, 1u), 1000u, view));
    CHECK_EQ(view.width, size);
    CHECK_EQ(view.height, size);
    CHECK(view.row_pitch >= size * 4u);
    std::uint32_t west = 0u, east = 0u;
    std::memcpy(&west, view.pixels + (size / 2u) * view.row_pitch, sizeof west);
    std::memcpy(&east, view.pixels + (size / 2u) * view.row_pitch + (size - 1u) * 4u, sizeof east);
    CHECK_EQ(west, red);
    CHECK_EQ(east, blue);
    surfaces.unmap(av::SurfacePoint(key, 1u));
    CHECK(!surfaces.map(av::SurfacePoint(key, 1u), 0u, view));

    constexpr std::uint32_t four_samples = 4u;
    constexpr std::uint32_t sampled = 77u;
    CHECK(av_d3d9_CreateSurface9(wire, sampled, kind_render_target, size, size, format_a8r8g8b8, four_samples, 0u,
                                 0u));
    CHECK(av_d3d9_SetRenderTarget9(wire, 0u, sampled, 0u));
    CHECK(av_d3d9_Clear9(wire, 0u, nullptr, clear_target, blue, 1.0f, 0u));
    CHECK(av_d3d9_PresentPoint9(wire, sampled, 0u, key, 4u));
    CHECK(surfaces.map(av::SurfacePoint(key, 4u), 1000u, view));
    std::uint32_t resolved = 0u;
    std::memcpy(&resolved, view.pixels + (size / 2u) * view.row_pitch + (size / 2u) * 4u, sizeof resolved);
    CHECK_EQ(resolved, blue);
    surfaces.unmap(av::SurfacePoint(key, 4u));
    CHECK(av_d3d9_SetRenderTarget9(wire, 0u, target, 0u));

    CHECK(av_d3d9_PresentPoint9(wire, target, 0u, key, 0u));
    CHECK(av_d3d9_PresentPoint9(wire, target, 0u, 0u, 2u));
    CHECK(av_d3d9_CreateSurface9(wire, other_target, kind_render_target, size, size, format_r5g6b5, 0u, 0u, 0u));
    CHECK(av_d3d9_PresentPoint9(wire, other_target, 0u, key, 3u));
    CHECK(!surfaces.map(av::SurfacePoint(key, 3u), 0u, view));
    CHECK(av_d3d9_Finish9(wire, &hresult));
    CHECK_EQ(hresult, 0u);
    CHECK_EQ(wire.refused(), 0u);
    CHECK(surfaces.invariants_hold());
    backend.shut_down();
    shares->release(1u, key);
    device11->retire_shares();
}

TEST(d3d9_session_device, a_present_point_of_nothing_is_refused)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d9HostDevice  *device9 = nullptr;
    if (!host_devices(apis, device11, device9)) return;
    av::HostHeap       surfaces_heap;
    av::SharedSurfaces surfaces(surfaces_heap);
    av::D3dBackend     backend(rig.log);
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, rig.imports, device11, nullptr, &surfaces, device9));
    av::D3dLoopback wire(backend, rig.imports);
    std::uint32_t   hresult = 1u;
    CHECK(av_d3d9_CreateDevice9(wire, device, 0u, &hresult));
    CHECK(av_d3d9_PresentPoint9(wire, target, 0u, 0x9151u, 1u));
    CHECK(av_d3d9_Finish9(wire, &hresult));
    CHECK(hresult != 0u);
    CHECK_EQ(wire.refused(), 0u);
}


TEST(d3d9_session_device, a_flipped_primary_is_shared_under_its_key_as_it_was_drawn)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d9HostDevice  *device9 = nullptr;
    if (!host_devices(apis, device11, device9)) return;
    av::HostShares *const shares = apis.shares(0u);
    constexpr std::uint64_t key = 0x9160u, other_key = 0x9161u;
    constexpr std::uint32_t domain = 1u, format_r5g6b5 = 23u;
    av::HostHeap       surfaces_heap;
    av::SharedSurfaces surfaces(surfaces_heap);
    av::D3dBackend     backend(rig.log);
    av::SessionShares  session;
    session.bind(*shares, domain, backend);
    CHECK(backend.initialize(session, rig.imports, device11, nullptr, &surfaces, device9));
    av::D3dLoopback wire(backend, rig.imports);
    std::uint32_t   hresult = 1u;
    CHECK(av_d3d9_CreateDevice9(wire, device, 0u, &hresult));
    CHECK(av_d3d9_CreateSurface9(wire, target, kind_render_target, size, size, format_a8r8g8b8, 0u, 0u, 0u));
    CHECK(av_d3d9_SetRenderTarget9(wire, 0u, target, 0u));
    CHECK(av_d3d9_Clear9(wire, 0u, nullptr, clear_target, red, 1.0f, 0u));
    CHECK(av_d3d9_PresentPoint9(wire, target, 0u, key, 0u));
    CHECK(av_d3d9_Finish9(wire, &hresult));

    std::uint32_t shown[size * size] = {};
    constexpr std::uint32_t most_flips = 4u;
    const auto flips_until = [&](std::uint32_t colour) {
        for (std::uint32_t flip = 0u; flip < most_flips; flip++) {
            CHECK(av_d3d9_PresentPoint9(wire, target, 0u, key, 0u));
            CHECK(av_d3d9_Finish9(wire, &hresult));
            if (device11->read_image(domain, key, shown, size * 4u, size, size) &&
                (shown[size * size / 2u] & 0x00ffffffu) == (colour & 0x00ffffffu))
                return true;
        }
        return false;
    };
    CHECK(flips_until(red));
    CHECK_EQ(shown[0] & 0x00ffffffu, red & 0x00ffffffu);
    CHECK_EQ(shown[size * size - 1u] & 0x00ffffffu, red & 0x00ffffffu);
    CHECK(av_d3d9_Clear9(wire, 0u, nullptr, clear_target, blue, 1.0f, 0u));
    CHECK(flips_until(blue));

    CHECK(av_d3d9_CreateSurface9(wire, other_target, kind_render_target, size, size, format_r5g6b5, 0u, 0u, 0u));
    CHECK(av_d3d9_PresentPoint9(wire, other_target, 0u, other_key, 0u));
    CHECK(av_d3d9_Finish9(wire, &hresult));
    CHECK(!device11->read_image(domain, other_key, shown, size * 4u, size, size));
    CHECK_EQ(wire.refused(), 0u);
    backend.shut_down();
    shares->release(domain, 0u);
}

TEST(d3d9_session_device, a_flip_with_no_compositor_crosses_through_the_cpu)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d9HostDevice  *device9 = nullptr;
    if (!host_devices(apis, device11, device9)) return;
    av::CompositorDevice *const compositor = device11->compositor();
    device11->use_compositor(nullptr);
    av::HostShares *const shares = apis.shares(0u);
    constexpr std::uint64_t key = 0x9170u;
    constexpr std::uint32_t domain = 1u;
    {
        av::HostHeap       surfaces_heap;
        av::SharedSurfaces surfaces(surfaces_heap);
        av::D3dBackend     backend(rig.log);
        av::SessionShares  session;
        session.bind(*shares, domain, backend);
        CHECK(backend.initialize(session, rig.imports, device11, nullptr, &surfaces, device9));
        av::D3dLoopback wire(backend, rig.imports);
        std::uint32_t   hresult = 1u;
        CHECK(av_d3d9_CreateDevice9(wire, device, 0u, &hresult));
        CHECK(av_d3d9_CreateSurface9(wire, target, kind_render_target, size, size, format_a8r8g8b8, 0u, 0u, 0u));
        CHECK(av_d3d9_SetRenderTarget9(wire, 0u, target, 0u));
        CHECK(av_d3d9_Clear9(wire, 0u, nullptr, clear_target, blue, 1.0f, 0u));
        std::uint32_t shown[size * size] = {};
        bool          arrived = false;
        for (std::uint32_t flip = 0u; flip < 4u && !arrived; flip++) {
            CHECK(av_d3d9_PresentPoint9(wire, target, 0u, key, 0u));
            CHECK(av_d3d9_Finish9(wire, &hresult));
            arrived = device11->read_image(domain, key, shown, size * 4u, size, size) &&
                      (shown[size * size / 2u] & 0x00ffffffu) == (blue & 0x00ffffffu);
        }
        CHECK(arrived);
        backend.shut_down();
    }
    shares->release(domain, 0u);
    device11->use_compositor(compositor);
}

TEST(d3d9_session_device, each_flipping_device_has_a_lane_of_its_own_and_gives_it_back)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d9HostDevice  *device9 = nullptr;
    if (!host_devices(apis, device11, device9)) return;
    av::HostShares *const shares = apis.shares(0u);
    constexpr std::uint32_t domain = 1u, devices = 8u;
    av::MockPlatform platform;
    {
        av::HostHeap       surfaces_heap;
        av::SharedSurfaces surfaces(surfaces_heap);
        av::D3dBackend     backend(rig.log);
        av::WatchedBlocksProbe::draw_from(av::D3dBackendProbe::blocks(backend), platform);
        av::SessionShares session;
        session.bind(*shares, domain, backend);
        CHECK(backend.initialize(session, rig.imports, device11, nullptr, &surfaces, device9));
        av::D3dLoopback wire(backend, rig.imports);
        std::uint32_t   hresult = 1u;
        std::uint32_t   shown[size * size] = {};
        const auto      shows = [&](std::uint32_t made, std::uint32_t drawn, std::uint64_t key, bool starved) {
            CHECK(av_d3d9_CreateDevice9(wire, made, 0u, &hresult));
            CHECK_EQ(hresult, 0u);
            CHECK(av_d3d9_CreateSurface9(wire, drawn, kind_render_target, size, size, format_a8r8g8b8, 0u, 0u, 0u));
            CHECK(av_d3d9_SetRenderTarget9(wire, 0u, drawn, 0u));
            CHECK(av_d3d9_Clear9(wire, 0u, nullptr, clear_target, blue, 1.0f, 0u));
            if (starved) platform.fail_allocation_at(1u);
            bool arrived = false;
            for (std::uint32_t flip = 0u; flip < 4u && !arrived; flip++) {
                CHECK(av_d3d9_PresentPoint9(wire, drawn, 0u, key, 0u));
                CHECK(av_d3d9_Finish9(wire, &hresult));
                arrived = device11->read_image(domain, key, shown, size * 4u, size, size) &&
                          (shown[size * size / 2u] & 0x00ffffffu) == (blue & 0x00ffffffu);
            }
            platform.fail_allocation_at(0u);
            return arrived;
        };
        CHECK(shows(1u, 2u, 0x9180u, true));
        CHECK(av_d3d9_Destroy9(wire, 2u));
        CHECK(av_d3d9_Destroy9(wire, 1u));
        CHECK(shows(1u, 2u, 0x9181u, true));
        CHECK(av_d3d9_Destroy9(wire, 2u));
        CHECK(av_d3d9_Destroy9(wire, 1u));
        for (std::uint32_t index = 0u; index < devices; index++)
            CHECK(shows(10u + 2u * index, 11u + 2u * index, 0x9190u + index, false));
        CHECK_EQ(wire.refused(), 0u);
        CHECK(backend.invariants_hold());
        backend.shut_down();
    }
    shares->release(domain, 0u);
}

namespace {

constexpr std::uint32_t slice_bytes = 256u;
constexpr std::uint32_t slice_usage = 0x208u;

bool slice_host(av::LoaderHostApis &apis, av::D3d11HostDevice *&device11, av::D3d9HostDevice *&device9,
                av::HostMemorySource *&source)
{
    if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
    apis.open();
    for (std::uint32_t index = 0u; index < apis.gpu_count(); index++) {
        device11 = apis.d3d11_device(index);
        device9 = apis.d3d9_device(index);
        source = apis.memory_source(index);
        if (device11 != nullptr && device9 != nullptr && source != nullptr) return true;
    }
    std::fprintf(stderr, "no GPU here runs DXVK's d3d9 with memory to share: nothing judged\n");
    return false;
}

bool buffer_holds(IDirect3DVertexBuffer9 *buffer, const std::uint8_t *expected)
{
    void *mapped = nullptr;
    if (FAILED(buffer->Lock(0u, 0u, &mapped, D3DLOCK_READONLY))) return false;
    const bool same = std::memcmp(mapped, expected, slice_bytes) == 0;
    buffer->Unlock();
    return same;
}

void fill_buffer(IDirect3DVertexBuffer9 *buffer, std::uint8_t byte)
{
    void *mapped = nullptr;
    CHECK(SUCCEEDED(buffer->Lock(0u, 0u, &mapped, D3DLOCK_DISCARD)));
    std::memset(mapped, byte, slice_bytes);
    buffer->Unlock();
}

}

TEST(d3d9_session_device, a_render_target_is_copied_into_lent_memory_row_by_row_and_a_copy_that_does_not_fit_is_refused)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice  *device11 = nullptr;
    av::D3d9HostDevice   *device9 = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!slice_host(apis, device11, device9, source)) return;
    av::D3dBackend backend(rig.log);
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, rig.imports, device11, nullptr, nullptr, device9));
    av::D3dLoopback wire(backend, rig.imports, source, 16u << 20u, 8u << 20u);
    constexpr std::uint32_t pitch = size * 4u + 32u;
    const std::uint64_t memory = wire.open_memory(av::HostMemoryRequest(av::HostMemoryRequest::Kind::readback, 0u),
                                                  std::size_t{pitch} * size);
    av::SourceMemory *const store = rig.imports.named(memory);
    CHECK(store != nullptr);
    std::memset(store->pointer(), 0x5a, store->size());

    std::uint32_t hresult = 1u;
    CHECK(av_d3d9_CreateDevice9(wire, device, 0u, &hresult));
    CHECK(av_d3d9_CreateSurface9(wire, target, kind_render_target, size, size, format_a8r8g8b8, 0u, 0u, 0u));
    CHECK(av_d3d9_SetRenderTarget9(wire, 0u, target, 0u));
    CHECK(av_d3d9_Clear9(wire, 0u, nullptr, clear_target, red, 1.0f, 0u));
    const Rect right = {static_cast<std::int32_t>(size / 2u), 0, static_cast<std::int32_t>(size),
                        static_cast<std::int32_t>(size)};
    CHECK(av_d3d9_Clear9(wire, 1u, &right, clear_target, blue, 1.0f, 0u));
    const Rect all = {0, 0, static_cast<std::int32_t>(size), static_cast<std::int32_t>(size)};
    CHECK(av_d3d9_CopyToLent9(wire, target, 0u, all, memory, pitch, 0u, 0u));
    CHECK(av_d3d9_Finish9(wire, &hresult));
    CHECK_EQ(hresult, 0u);
    CHECK_EQ(wire.refused(), 0u);
    for (std::uint32_t y = 0u; y < size; y += 9u) {
        const std::uint8_t *const row = store->pointer() + std::size_t{y} * pitch;
        std::uint32_t left = 0u, edge = 0u;
        std::memcpy(&left, row, sizeof(left));
        std::memcpy(&edge, row + (size / 2u) * 4u, sizeof(edge));
        CHECK_EQ(left, red);
        CHECK_EQ(edge, blue);
        CHECK_EQ(row[size * 4u], 0x5au);
    }
    CHECK(av_d3d9_CopyToLent9(wire, target, 0u, all, memory, size * 4u - 4u, 0u, 0u));
    CHECK(av_d3d9_Finish9(wire, &hresult));
    CHECK(hresult != 0u);
    wire.close_memory(memory);
    backend.shut_down();
}

TEST(d3d9_session_device, a_buffer_on_a_slice_it_cannot_be_drawn_from_in_place_is_copied_into_before_each_draw)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice  *device11 = nullptr;
    av::D3d9HostDevice   *device9 = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!slice_host(apis, device11, device9, source)) return;
    av::D3dBackend backend(rig.log);
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, rig.imports, device11, nullptr, nullptr, device9));
    av::D3dLoopback wire(backend, rig.imports, source, 16u << 20u, 8u << 20u);
    const std::uint64_t memory = wire.open_memory(av::HostMemoryRequest(av::HostMemoryRequest::Kind::upload, 0u),
                                                  4096u);
    av::SourceMemory *const store = rig.imports.named(memory);
    CHECK(store != nullptr);
    for (std::uint32_t at = 0u; at < slice_bytes; at++) store->pointer()[at] = static_cast<std::uint8_t>(at * 3u + 1u);

    IDirect3DDevice9 *device = nullptr;
    CHECK_EQ(device9->make_device(device), 0);
    IDirect3DVertexBuffer9 *buffer = nullptr;
    CHECK(SUCCEEDED(device->CreateVertexBuffer(slice_bytes, slice_usage, 0u, D3DPOOL_DEFAULT, &buffer, nullptr)));
    {
        av::MockPlatform   platform;
        av::DriverLog      lines(rig.sink, "slices: ");
        av::SliceStorages  storages(platform, lines);
        av::EventfdWake    wake;
        av::D3d9Lanes      lanes(platform, wake, lines);
        av::D3d9Slices     slices(platform, storages, lanes, lines);
        CHECK(!slices.supported());
        slices.initialize(device9, &rig.imports);
        CHECK(slices.supported());
        CHECK(slices.bind(device, 3u, buffer, memory, 0u, slice_bytes));
        CHECK(slices.stream_bound(device, 0u, 3u));
        fill_buffer(buffer, 0xEEu);
        slices.copy_in_use(device);
        CHECK(buffer_holds(buffer, store->pointer()));
        for (std::uint32_t at = 0u; at < slice_bytes; at++) store->pointer()[at] = static_cast<std::uint8_t>(at + 9u);
        slices.copy_in_use(device);
        CHECK(slices.invariants_hold());
        CHECK(slices.detach(3u, buffer));
        CHECK(buffer_holds(buffer, store->pointer()));
        slices.forget_device(device);
        slices.reclaim();
        CHECK(slices.invariants_hold());
        slices.shut_down();
        CHECK(!slices.supported());
    }
    buffer->Release();
    device->Release();
    wire.close_memory(memory);
    backend.shut_down();
}

TEST(d3d9_session_device, a_device_that_can_draw_from_a_slice_in_place_is_probed_allowed_and_leaves_it_with_the_buffers_bytes)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice  *device11 = nullptr;
    av::D3d9HostDevice   *device9 = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!slice_host(apis, device11, device9, source)) return;
    av::D3dBackend backend(rig.log);
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, rig.imports, device11, nullptr, nullptr, device9));
    av::D3dLoopback wire(backend, rig.imports, source, 16u << 20u, 8u << 20u);
    const std::uint64_t memory = wire.open_memory(av::HostMemoryRequest(av::HostMemoryRequest::Kind::upload, 0u),
                                                  4096u);
    av::SourceMemory *const store = rig.imports.named(memory);
    CHECK(store != nullptr);
    for (std::uint32_t at = 0u; at < slice_bytes; at++) store->pointer()[at] = static_cast<std::uint8_t>(at * 5u + 2u);

    IDirect3DDevice9 *device = nullptr;
    CHECK_EQ(device9->make_device(device), 0);
    IDirect3DVertexBuffer9 *buffer = nullptr;
    CHECK(SUCCEEDED(device->CreateVertexBuffer(slice_bytes, slice_usage, 0u, D3DPOOL_DEFAULT, &buffer, nullptr)));
    {
        av::MockPlatform   platform;
        av::DriverLog      lines(rig.sink, "slices: ");
        av::SliceStorages  storages(platform, lines);
        av::EventfdWake    wake;
        av::D3d9Lanes      lanes(platform, wake, lines);
        av::D3d9Slices     slices(platform, storages, lanes, lines);
        slices.initialize(device9, &rig.imports);
        if (slices.probe(device)) {
            av::D3d9Lane *const lane = lanes.find(device);
            CHECK(lane != nullptr);
            CHECK(lane->allowed());
            CHECK(lane->connected());
            CHECK(lane->bindable(buffer));
            CHECK(!lane->bindable(nullptr));
            CHECK(slices.bind(device, 3u, buffer, memory, 0u, slice_bytes));
            CHECK(slices.invariants_hold());
            CHECK(lanes.invariants_hold());
            lanes.touch(device);
            CHECK(lane->touched());
            lanes.mark_touched();
            CHECK(!lane->touched());
            CHECK(lanes.marked() >= 1u || lanes.marking_failed());
            CHECK(slices.detach(3u, buffer));
            CHECK(buffer_holds(buffer, store->pointer()));
        } else {
            std::fprintf(stderr, "this DXVK binds no buffer to imported storage: the in-place path is not judged\n");
        }
        slices.forget_device(device);
        slices.shut_down();
    }
    buffer->Release();
    device->Release();
    wire.close_memory(memory);
    backend.shut_down();
}
