/* The Direct3D 9 half of a Direct3D session on this machine's GPU, through
 * DXVK's native d3d9: devices with no window made on the GPU the session is
 * on, a render target cleared and read back, judged pixel by pixel.
 *
 * DXVK's d3d9 is loaded from AV_D3D_PATH, or build-d3d/lib in this checkout
 * (tools/build_d3d_runtime.sh). A host without it returns without judging, as
 * the other device tests do. */
#include "device/executor/d3d_backend.hpp"
#include "device/shim/d3d11_host_device.hpp"
#include "device/shim/d3d9_host_device.hpp"
#include "device/shim/loader_host_apis.hpp"
#include "harness/executor/d3d_loopback.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/d3d9/d3d9_dispatch_client.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

using namespace av::d3d9proto;

constexpr std::uint32_t format_a8r8g8b8 = 21u;
constexpr std::uint32_t kind_render_target = 1u;
constexpr std::uint32_t clear_target = 0x1u;
constexpr std::uint32_t size = 64u;
constexpr std::uint32_t red = 0xffff0000u;
constexpr std::uint32_t blue = 0xff0000ffu;
constexpr std::uint32_t invalid_call = 0x8876086cu;

enum Id : std::uint32_t { device = 1, target, other_device, other_target };

/* The first GPU here with both devices; nulls when there is none. */
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

}  // namespace

TEST(d3d9_session_device, a_cleared_render_target_reads_back_as_cleared)
{
    av::LoaderHostApis      apis;
    av::D3d11HostDevice    *device11 = nullptr;
    av::D3d9HostDevice     *device9 = nullptr;
    if (!host_devices(apis, device11, device9)) return;
    av::D3dBackend backend;
    CHECK(backend.initialize(device11, nullptr, nullptr, device9));
    av::D3dLoopback wire(backend);

    std::uint32_t hresult = 1u;
    CHECK(av_d3d9_CreateDevice9(wire, device, 0u, &hresult));
    CHECK_EQ(hresult, 0u);
    CHECK(av_d3d9_CreateSurface9(wire, target, kind_render_target, size, size, format_a8r8g8b8, 0u, 0u, 0u));
    CHECK(av_d3d9_SetRenderTarget9(wire, 0u, target, 0u));
    CHECK(av_d3d9_Clear9(wire, 0u, nullptr, clear_target, red, 1.0f, 0u));
    /* Only the right half, so a clear that ignored its rectangle shows. */
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
    CHECK(av_d3d9_Finish9(wire, &hresult));
    CHECK_EQ(hresult, 0u);
    CHECK_EQ(wire.refused(), 0u);
}

/* A surface belongs to the device that made it: naming it while another device
 * is selected is refused, and removes the device that named it. */
TEST(d3d9_session_device, another_devices_surface_is_refused_and_removes_the_device)
{
    av::LoaderHostApis      apis;
    av::D3d11HostDevice    *device11 = nullptr;
    av::D3d9HostDevice     *device9 = nullptr;
    if (!host_devices(apis, device11, device9)) return;
    av::D3dBackend backend;
    CHECK(backend.initialize(device11, nullptr, nullptr, device9));
    av::D3dLoopback wire(backend);

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

    /* The first device was not the one that named it, and still draws. */
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

/* What the device cannot be asked for is refused before DXVK sees it. */
TEST(d3d9_session_device, a_surface_the_host_cannot_make_names_nothing)
{
    av::LoaderHostApis      apis;
    av::D3d11HostDevice    *device11 = nullptr;
    av::D3d9HostDevice     *device9 = nullptr;
    if (!host_devices(apis, device11, device9)) return;
    av::D3dBackend backend;
    CHECK(backend.initialize(device11, nullptr, nullptr, device9));
    av::D3dLoopback wire(backend);

    std::uint32_t hresult = 1u;
    CHECK(av_d3d9_CreateDevice9(wire, device, 0u, &hresult));
    CHECK_EQ(hresult, 0u);
    /* Zero width, an unknown kind, and a size past what the wire allows. */
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
    draw_device = 10, draw_target, vertices, declaration, vertex_shader, pixel_shader, texture, dxt_texture
};

/* vs_2_0: dcl_position v0; dcl_color v1; mov oPos, v0; mov oD0, v1. */
constexpr std::uint32_t pass_through_vs[] = {0xfffe0200u, 0x0200001fu, 0x80000000u, 0x900f0000u, 0x0200001fu,
                                             0x8000000au, 0x900f0001u, 0x02000001u, 0xc00f0000u, 0x90e40000u,
                                             0x02000001u, 0xd00f0000u, 0x90e40001u, 0x0000ffffu};
/* ps_2_0: dcl v0; mov oC0, v0. */
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

}  // namespace

/* A triangle over the lower-left half, the way Portal draws: a vertex buffer,
 * a declaration and shader model 2 shaders as their tokens. */
TEST(d3d9_session_device, a_shaded_triangle_lands_where_it_was_drawn)
{
    av::LoaderHostApis      apis;
    av::D3d11HostDevice    *device11 = nullptr;
    av::D3d9HostDevice     *device9 = nullptr;
    if (!host_devices(apis, device11, device9)) return;
    av::D3dBackend backend;
    CHECK(backend.initialize(device11, nullptr, nullptr, device9));
    av::D3dLoopback wire(backend);
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

/* The fixed-function pipeline with pre-transformed vertices, which menus and
 * DirectDraw-era drawing use: no shader bound at all. */
TEST(d3d9_session_device, a_pretransformed_triangle_needs_no_shader)
{
    av::LoaderHostApis      apis;
    av::D3d11HostDevice    *device11 = nullptr;
    av::D3d9HostDevice     *device9 = nullptr;
    if (!host_devices(apis, device11, device9)) return;
    av::D3dBackend backend;
    CHECK(backend.initialize(device11, nullptr, nullptr, device9));
    av::D3dLoopback wire(backend);
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

/* A write lands in the box it names and nowhere else, and a block format's
 * box is counted in blocks. */
TEST(d3d9_session_device, a_texture_write_reads_back_where_it_was_written)
{
    av::LoaderHostApis      apis;
    av::D3d11HostDevice    *device11 = nullptr;
    av::D3d9HostDevice     *device9 = nullptr;
    if (!host_devices(apis, device11, device9)) return;
    av::D3dBackend backend;
    CHECK(backend.initialize(device11, nullptr, nullptr, device9));
    av::D3dLoopback wire(backend);
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

    /* A box past the level, or data short of it, is refused and removes the device. */
    CHECK(av_d3d9_WriteTexture9(wire, texture, 0u, Box{0u, 0u, 0u, 9u, 8u, 1u}, 36u, 0u, 0u,
                                reinterpret_cast<const std::uint8_t *>(zeros), sizeof(zeros)));
    CHECK(av_d3d9_Finish9(wire, &hresult));
    CHECK(hresult != 0u);
}
