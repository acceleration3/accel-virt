/* A reserved resource's tiles on this machine's GPU, through vkd3d-proton: a
 * texture with no memory of its own, its tiles mapped to a heap on a queue,
 * filled and read back through CopyTiles from and into guest memory, its
 * mappings shared with a second texture by CopyTileMappings and remapped by
 * ranges that unmap, skip and reuse a tile - each judged by the bytes that
 * come back. And every region or range that would have vkd3d-proton walk past
 * a resource or a heap is refused before it gets there.
 *
 * vkd3d-proton is loaded as test_d3d12_session_device.cpp loads it; a host
 * without it, or whose GPU has no sparse residency, returns without judging. */
#include "core/support/object_storage.hpp"
#include "executor/d3d/d3d12_session.hpp"
#include "executor/d3d/d3d12_tile_update.hpp"
#include "executor/d3d/d3d_backend.hpp"
#include "executor/d3d/d3d_object_table.hpp"
#include "executor/guest_region.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/d3d12_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "harness/executor/d3d_loopback.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

using namespace av::d3d12proto;

constexpr std::uint32_t format_r32g32b32a32 = 2u;
constexpr std::uint32_t format_r8g8b8a8 = 28u;
constexpr std::uint32_t dimension_buffer = 1u;
constexpr std::uint32_t dimension_texture2d = 3u;
constexpr std::uint32_t layout_row_major = 1u;
constexpr std::uint32_t layout_64kb_undefined_swizzle = 2u;
constexpr std::uint32_t heap_default = 1u;
constexpr std::uint32_t heap_upload = 2u;
constexpr std::uint32_t heap_readback = 3u;
/* D3D12_HEAP_FLAG_DENY_BUFFERS | DENY_RT_DS_TEXTURES: a tile pool for
 * textures that are neither, which every resource heap tier allows. */
constexpr std::uint32_t texture_tile_pool = 0x44u;
constexpr std::uint32_t state_copy_dest = 0x400u;
constexpr std::uint32_t state_copy_source = 0x800u;
constexpr std::uint32_t state_generic_read = 0xac3u;
constexpr std::uint32_t copy_into_tiles = 0x2u;
constexpr std::uint32_t copy_out_of_tiles = 0x4u;
constexpr std::uint32_t range_null = 0x1u;
constexpr std::uint32_t range_skip = 0x2u;
constexpr std::uint32_t range_reuse = 0x4u;
constexpr std::uint32_t no_hazard = 0x1u;
constexpr std::uint32_t driver_internal_error = 0x887A0020u;

/* 256 x 256 of 4-byte texels is 2 x 2 tiles of 128 x 128, with no mips to
 * pack. */
constexpr std::uint32_t side = 256u;
constexpr std::uint32_t tiles = 4u;
constexpr std::uint64_t tile_bytes = 64u * 1024u;
constexpr std::uint64_t tiles_bytes = tiles * tile_bytes;

constexpr std::size_t   guest_bytes = 8u << 20u;
constexpr std::size_t   lent_bytes = 1u << 20u;
constexpr std::uint64_t upload_at = lent_bytes;
constexpr std::uint64_t readback_at = lent_bytes + tiles_bytes;
constexpr std::uint64_t upload_address = 0x100000000ull;
constexpr std::uint64_t readback_address = 0x200000000ull;
constexpr std::uint64_t sparse_address = 0x300000000ull;

enum Id : std::uint32_t {
    queue = 1, allocator, list, fence, tile_heap, tiled, mirror, wide, sparse, upload_heap, upload, readback_heap,
    readback
};

bool host_devices(av::LoaderHostApis &apis, av::D3d11HostDevice *&device11, av::D3d12HostDevice *&device12)
{
    if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
    apis.open();
    for (std::uint32_t index = 0u; index < apis.gpu_count(); index++) {
        device11 = apis.d3d11_device(index);
        device12 = apis.d3d12_device(index);
        if (device11 != nullptr && device12 != nullptr) return true;
    }
    std::fprintf(stderr, "no GPU here runs vkd3d-proton: nothing judged\n");
    return false;
}

/* Guest memory this vkd3d-proton imports, on a GPU with sparse residency. */
bool tiled_here(av::D3d12HostDevice &device, av::D3dLoopback &wire)
{
    if (device.caps().tiled_resources_tier == 0u) {
        std::fprintf(stderr, "this GPU has no tiled resources: nothing judged\n");
        return false;
    }
    ID3D12Heap *const imported = device.import_heap(wire.guest(upload_at), tiles_bytes);
    if (imported == nullptr) {
        std::fprintf(stderr, "this vkd3d-proton imports no host memory (tools/vkd3d-proton-patches): nothing judged\n");
        return false;
    }
    imported->Release();
    return true;
}

HeapProperties heap_of(std::uint32_t type)
{
    return HeapProperties{type, 0u, 0u, 0u};
}

ResourceDesc buffer_of(std::uint64_t bytes)
{
    ResourceDesc desc = {};
    desc.width = bytes;
    desc.dimension = dimension_buffer;
    desc.height = 1u;
    desc.depth_or_array = 1u;
    desc.mip_levels = 1u;
    desc.sample_count = 1u;
    desc.layout = layout_row_major;
    return desc;
}

ResourceDesc reserved_texture(std::uint32_t format = format_r8g8b8a8)
{
    ResourceDesc desc = {};
    desc.width = side;
    desc.dimension = dimension_texture2d;
    desc.height = side;
    desc.depth_or_array = 1u;
    desc.mip_levels = 1u;
    desc.format = format;
    desc.sample_count = 1u;
    desc.layout = layout_64kb_undefined_swizzle;
    return desc;
}

TileRegion run_of(std::uint32_t x, std::uint32_t count)
{
    return TileRegion{TileCoordinate{x, 0u, 0u, 0u}, TileSize{count, 0u, 0u, 0u, 0u}};
}

/* A queue and a list; a tile pool of four tiles; two reserved textures of
 * 4-byte texels, one of 16-byte texels and a reserved buffer of four tiles;
 * and an upload buffer of four tiles' bytes and a readback buffer of eight,
 * both guest memory. The first texture's tiles are mapped to the pool in
 * order. */
void make_tiles(av::D3dLoopback &wire)
{
    std::uint32_t hresult = 1u;
    Caps12        caps = {};
    CHECK(av_d3d12_OpenDevice12(wire, 0u, &hresult, &caps));
    CHECK(av_d3d12_CreateCommandQueue12(wire, queue, 0u, 0u, 0u));
    CHECK(av_d3d12_CreateCommandAllocator12(wire, allocator, 0u));
    CHECK(av_d3d12_CreateCommandList12(wire, list, allocator, 0u, 0u));
    CHECK(av_d3d12_CreateFence12(wire, fence, 0u, 0u));
    CHECK(av_d3d12_CreateHeap12(wire, tile_heap, heap_of(heap_default), texture_tile_pool, tiles_bytes, tile_bytes,
                                0u));
    CHECK(av_d3d12_CreateReservedResource12(wire, tiled, reserved_texture(), state_copy_dest, ClearValue{}, 0u));
    CHECK(av_d3d12_CreateReservedResource12(wire, mirror, reserved_texture(), state_copy_dest, ClearValue{}, 0u));
    CHECK(av_d3d12_CreateReservedResource12(wire, wide, reserved_texture(format_r32g32b32a32), state_copy_source,
                                            ClearValue{}, 0u));
    CHECK(av_d3d12_CreateReservedResource12(wire, sparse, buffer_of(tiles_bytes), 0u, ClearValue{}, sparse_address));
    CHECK(av_d3d12_CreateHeap12(wire, upload_heap, heap_of(heap_upload), 0u, tiles_bytes, 0u, upload_at));
    CHECK(av_d3d12_CreatePlacedResource12(wire, upload, upload_heap, 0u, buffer_of(tiles_bytes), state_generic_read,
                                           ClearValue{}, upload_address));
    CHECK(av_d3d12_CreateHeap12(wire, readback_heap, heap_of(heap_readback), 0u, 2u * tiles_bytes, 0u, readback_at));
    CHECK(av_d3d12_CreatePlacedResource12(wire, readback, readback_heap, 0u, buffer_of(2u * tiles_bytes),
                                           state_copy_dest, ClearValue{}, readback_address));
    const TileRegion whole = run_of(0u, tiles);
    const TileRange  in_order = {0u, 0u, tiles};
    CHECK(av_d3d12_UpdateTileMappings12(wire, queue, tiled, 1u, &whole, tile_heap, 1u, &in_order, 0u));
}

/* What was recorded, executed and waited for; the list reset for more. */
void run_list(av::D3dLoopback &wire, std::uint64_t value)
{
    CHECK(av_d3d12_CloseCommandList12(wire, list));
    const std::uint32_t lists[1] = {list};
    CHECK(av_d3d12_ExecuteCommandLists12(wire, queue, 1u, lists));
    CHECK(av_d3d12_SignalFence12(wire, queue, fence, value));
    std::uint64_t completed = 0u;
    CHECK(av_d3d12_FenceValue12(wire, fence, 1u, value, &completed));
    CHECK(completed >= value);
    CHECK(av_d3d12_ResetCommandAllocator12(wire, allocator));
    CHECK(av_d3d12_ResetCommandList12(wire, list, allocator, 0u));
}

std::uint8_t mark_of(std::uint32_t tile)
{
    return static_cast<std::uint8_t>(0x11u * (tile + 1u));
}

/* Whether the tile at `index` of what was read back at `offset` is every byte
 * `mark`: its first, its middle and its last. */
bool tile_is(const av::D3dLoopback &wire, std::uint64_t offset, std::uint32_t index, std::uint8_t mark)
{
    const std::uint8_t *const bytes = wire.guest(readback_at + offset + index * tile_bytes);
    return bytes[0] == mark && bytes[tile_bytes / 2u] == mark && bytes[tile_bytes - 1u] == mark;
}

void barrier(av::D3dLoopback &wire, std::uint32_t resource, std::uint32_t before, std::uint32_t after)
{
    const Barrier transition = {0u, 0u, resource, 0xffffffffu, before, after, 0u, 0u};
    CHECK(av_d3d12_ResourceBarrier12(wire, list, 1u, &transition));
}

/* One tile of `resource` from tile `x`, out to the readback buffer at
 * `offset`. */
void copy_tiles(av::D3dLoopback &wire, std::uint32_t resource, std::uint32_t x, std::uint64_t offset)
{
    const TileRegion one = run_of(x, 1u);
    CHECK(av_d3d12_CopyTiles12(wire, list, resource, one.start, one.size, readback, offset, 0u));
}

}  // namespace

TEST(d3d12_session_tiles_device, a_reserved_texture_keeps_what_its_mapped_tiles_were_given)
{
    av::LoaderHostApis   apis;
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    if (!host_devices(apis, device11, device12)) return;
    av::D3dBackend backend;
    CHECK(backend.initialize(device11, device12));
    av::D3dLoopback wire(backend, guest_bytes, lent_bytes);
    if (!tiled_here(*device12, wire)) {
        backend.shut_down();
        return;
    }
    make_tiles(wire);
    std::uint32_t count = 0u, packed_mips = 9u, packed_tiles = 9u;
    CHECK(av_d3d12_ResourceTiling12(wire, tiled, &count, &packed_mips, &packed_tiles));
    CHECK_EQ(count, tiles);
    CHECK_EQ(packed_mips, 0u);
    CHECK_EQ(packed_tiles, 0u);
    /* A reserved buffer is tiles too; a buffer of memory of its own has
     * none. */
    CHECK(av_d3d12_ResourceTiling12(wire, sparse, &count, &packed_mips, &packed_tiles));
    CHECK_EQ(count, tiles);
    CHECK(av_d3d12_ResourceTiling12(wire, upload, &count, &packed_mips, &packed_tiles));
    CHECK_EQ(count, 0u);

    /* Each tile's bytes in, and out again: what comes back is what went to
     * that tile, so the tiles are memory and they are distinct. */
    for (std::uint32_t tile = 0u; tile < tiles; tile++)
        std::memset(wire.guest(upload_at + tile * tile_bytes), mark_of(tile), tile_bytes);
    std::memset(wire.guest(readback_at), 0, 2u * tiles_bytes);
    const TileRegion whole = run_of(0u, tiles);
    CHECK(av_d3d12_CopyTiles12(wire, list, tiled, whole.start, whole.size, upload, 0u, copy_into_tiles));
    barrier(wire, tiled, state_copy_dest, state_copy_source);
    CHECK(av_d3d12_CopyTiles12(wire, list, tiled, whole.start, whole.size, readback, 0u, copy_out_of_tiles | no_hazard));
    run_list(wire, 1u);
    for (std::uint32_t tile = 0u; tile < tiles; tile++) CHECK(tile_is(wire, 0u, tile, mark_of(tile)));
    /* A 2 x 2 box of them is the same four tiles, row by row. */
    std::memset(wire.guest(readback_at), 0, tiles_bytes);
    const TileSize box = {tiles, 1u, 2u, 2u, 1u};
    CHECK(av_d3d12_CopyTiles12(wire, list, tiled, TileCoordinate{}, box, readback, 0u, 0u));
    run_list(wire, 5u);
    for (std::uint32_t tile = 0u; tile < tiles; tile++) CHECK(tile_is(wire, 0u, tile, mark_of(tile)));

    /* The second texture given the first's mappings reads the same memory. */
    CHECK(av_d3d12_CopyTileMappings12(wire, queue, mirror, whole.start, tiled, whole.start, whole.size, 0u));
    barrier(wire, mirror, state_copy_dest, state_copy_source);
    CHECK(av_d3d12_CopyTiles12(wire, list, mirror, whole.start, whole.size, readback, tiles_bytes, 0u));
    run_list(wire, 6u);
    for (std::uint32_t tile = 0u; tile < tiles; tile++) CHECK(tile_is(wire, tiles_bytes, tile, mark_of(tile)));

    /* Its first tile unmapped, its second left as it was, its last two both
     * the pool's first tile. */
    const TileRange remap[3] = {{range_null, 0u, 1u}, {range_skip, 0u, 1u}, {range_reuse, 0u, 2u}};
    CHECK(av_d3d12_UpdateTileMappings12(wire, queue, mirror, 1u, &whole, tile_heap, 3u, remap, no_hazard));
    const TileRegion last_three = run_of(1u, 3u);
    CHECK(av_d3d12_CopyTiles12(wire, list, mirror, last_three.start, last_three.size, readback, tiles_bytes, 0u));
    run_list(wire, 7u);
    CHECK(tile_is(wire, tiles_bytes, 0u, mark_of(1u)));
    CHECK(tile_is(wire, tiles_bytes, 1u, mark_of(0u)));
    CHECK(tile_is(wire, tiles_bytes, 2u, mark_of(0u)));

    /* Its right-hand column of tiles - 1 and 3, a box of 1 x 2 - to the
     * pool's last two: read as a run of two from tile 1, it would be 1 and 2. */
    const TileRegion column = {TileCoordinate{1u, 0u, 0u, 0u}, TileSize{2u, 1u, 1u, 2u, 1u}};
    const TileRange  last_two = {0u, 2u, 2u};
    CHECK(av_d3d12_UpdateTileMappings12(wire, queue, mirror, 1u, &column, tile_heap, 1u, &last_two, 0u));
    CHECK(av_d3d12_CopyTiles12(wire, list, mirror, last_three.start, last_three.size, readback, tiles_bytes, 0u));
    run_list(wire, 9u);
    CHECK(tile_is(wire, tiles_bytes, 0u, mark_of(2u)));
    CHECK(tile_is(wire, tiles_bytes, 1u, mark_of(0u)));
    CHECK(tile_is(wire, tiles_bytes, 2u, mark_of(3u)));

    /* A texture of 16-byte texels copies at whole texels, and a buffer's
     * tiles at any byte. */
    copy_tiles(wire, wide, 0u, tiles_bytes - 16u);
    copy_tiles(wire, sparse, 0u, 2u);
    run_list(wire, 10u);

    std::uint32_t removed = 1u;
    CHECK(av_d3d12_Finish12(wire, &removed));
    CHECK_EQ(removed, 0u);
    CHECK_EQ(wire.refused(), 0u);
    backend.shut_down();
}

namespace {

using Misuse = void (*)(av::D3dLoopback &wire);

TileSize box_of(std::uint32_t width, std::uint32_t height, std::uint32_t depth)
{
    return TileSize{width * height * depth, 1u, width, height, depth};
}

void map(av::D3dLoopback &wire, const TileRegion &region, std::uint32_t heap, const TileRange &range,
         std::uint32_t flags)
{
    CHECK(av_d3d12_UpdateTileMappings12(wire, queue, tiled, 1u, &region, heap, 1u, &range, flags));
}

void copy_out(av::D3dLoopback &wire, std::uint32_t resource, const TileRegion &region, std::uint32_t buffer,
              std::uint64_t offset, std::uint32_t flags)
{
    CHECK(av_d3d12_CopyTiles12(wire, list, resource, region.start, region.size, buffer, offset, flags));
}

constexpr TileRange in_place = {0u, 0u, 1u};

constexpr Misuse misuses[] = {
    /* Regions past the texture's four tiles, or no region at all. */
    [](av::D3dLoopback &wire) { map(wire, run_of(4u, 1u), tile_heap, in_place, 0u); },
    [](av::D3dLoopback &wire) { map(wire, run_of(3u, 2u), tile_heap, TileRange{0u, 0u, 2u}, 0u); },
    [](av::D3dLoopback &wire) { map(wire, TileRegion{TileCoordinate{0u, 0u, 0u, 1u}, TileSize{1u, 0u, 0u, 0u, 0u}}, tile_heap,
                                    in_place, 0u); },
    [](av::D3dLoopback &wire) { map(wire, TileRegion{TileCoordinate{1u, 1u, 0u, 0u}, box_of(2u, 1u, 1u)}, tile_heap,
                                    TileRange{0u, 0u, 2u}, 0u); },
    [](av::D3dLoopback &wire) { map(wire, TileRegion{TileCoordinate{}, TileSize{1u, 1u, 0u, 1u, 1u}}, tile_heap,
                                    in_place, 0u); },
    [](av::D3dLoopback &wire) { map(wire, TileRegion{TileCoordinate{}, TileSize{1u, 1u, 1u, 0x10000u, 1u}},
                                    tile_heap, in_place, 0u); },
    [](av::D3dLoopback &wire) { map(wire, TileRegion{TileCoordinate{}, TileSize{1u, 1u, 1u, 1u, 0x10000u}},
                                    tile_heap, in_place, 0u); },
    [](av::D3dLoopback &wire) {
        const TileRange range = in_place;
        CHECK(av_d3d12_UpdateTileMappings12(wire, queue, tiled, 0u, nullptr, tile_heap, 1u, &range, 0u));
    },
    /* Ranges past the pool, mapping with no pool, of no tiles, or of no
     * flag; no range at all; a mapping flag that is not one. */
    [](av::D3dLoopback &wire) { map(wire, run_of(0u, 2u), tile_heap, TileRange{0u, 3u, 2u}, 0u); },
    [](av::D3dLoopback &wire) { map(wire, run_of(0u, 1u), 0u, in_place, 0u); },
    [](av::D3dLoopback &wire) { map(wire, run_of(0u, 1u), tile_heap, TileRange{range_null, 0u, 0u}, 0u); },
    [](av::D3dLoopback &wire) { map(wire, run_of(0u, 1u), tile_heap, TileRange{0x8u, 0u, 1u}, 0u); },
    [](av::D3dLoopback &wire) {
        const TileRegion region = run_of(0u, 1u);
        CHECK(av_d3d12_UpdateTileMappings12(wire, queue, tiled, 1u, &region, tile_heap, 0u, nullptr, 0u));
    },
    [](av::D3dLoopback &wire) { map(wire, run_of(0u, 1u), tile_heap, in_place, 0x2u); },
    /* A pool, queue or texture that is not one. */
    [](av::D3dLoopback &wire) { map(wire, run_of(0u, 1u), upload, in_place, 0u); },
    [](av::D3dLoopback &wire) {
        const TileRegion region = run_of(0u, 1u);
        CHECK(av_d3d12_UpdateTileMappings12(wire, list, tiled, 1u, &region, tile_heap, 1u, &in_place, 0u));
    },
    [](av::D3dLoopback &wire) {
        const TileRegion region = run_of(0u, 1u);
        CHECK(av_d3d12_UpdateTileMappings12(wire, queue, upload, 1u, &region, tile_heap, 1u, &in_place, 0u));
    },
    /* Mappings copied from or to past a texture, from a resource with no
     * tiles, or with a flag that is not one. */
    [](av::D3dLoopback &wire) {
        CHECK(av_d3d12_CopyTileMappings12(wire, queue, mirror, TileCoordinate{}, tiled, TileCoordinate{2u, 1u, 0u, 0u},
                                          TileSize{2u, 0u, 0u, 0u, 0u}, 0u));
    },
    [](av::D3dLoopback &wire) {
        CHECK(av_d3d12_CopyTileMappings12(wire, queue, mirror, TileCoordinate{3u, 1u, 0u, 0u}, tiled, TileCoordinate{},
                                          TileSize{1u, 0u, 0u, 0u, 0u}, 0u));
    },
    [](av::D3dLoopback &wire) {
        CHECK(av_d3d12_CopyTileMappings12(wire, queue, mirror, TileCoordinate{}, upload, TileCoordinate{}, TileSize{1u, 0u, 0u, 0u, 0u},
                                          0u));
    },
    [](av::D3dLoopback &wire) {
        CHECK(av_d3d12_CopyTileMappings12(wire, queue, mirror, TileCoordinate{}, tiled, TileCoordinate{}, TileSize{1u, 0u, 0u, 0u, 0u},
                                          0x2u));
    },
    [](av::D3dLoopback &wire) {
        CHECK(av_d3d12_CopyTileMappings12(wire, list, mirror, TileCoordinate{}, tiled, TileCoordinate{}, TileSize{1u, 0u, 0u, 0u, 0u},
                                          0u));
    },
    /* Resources nothing made, in each place one is named. */
    [](av::D3dLoopback &wire) {
        std::uint32_t count = 0u, packed_mips = 0u, packed_tiles = 0u;
        static_cast<void>(av_d3d12_ResourceTiling12(wire, 99u, &count, &packed_mips, &packed_tiles));
    },
    [](av::D3dLoopback &wire) {
        const TileRegion region = run_of(0u, 1u);
        CHECK(av_d3d12_UpdateTileMappings12(wire, queue, 99u, 1u, &region, tile_heap, 1u, &in_place, 0u));
    },
    [](av::D3dLoopback &wire) {
        CHECK(av_d3d12_CopyTileMappings12(wire, queue, 99u, TileCoordinate{}, tiled, TileCoordinate{},
                                          TileSize{1u, 0u, 0u, 0u, 0u}, 0u));
    },
    [](av::D3dLoopback &wire) {
        CHECK(av_d3d12_CopyTileMappings12(wire, queue, mirror, TileCoordinate{}, 99u, TileCoordinate{},
                                          TileSize{1u, 0u, 0u, 0u, 0u}, 0u));
    },
    [](av::D3dLoopback &wire) { copy_out(wire, 99u, run_of(0u, 1u), readback, 0u, 0u); },
    /* Tiles copied past the texture, both ways at once, with a flag that is
     * not one, past the buffer's end, at a split texel, into a texture, or
     * out of a resource with no tiles. */
    [](av::D3dLoopback &wire) { copy_out(wire, tiled, run_of(2u, 3u), readback, 0u, 0u); },
    [](av::D3dLoopback &wire) { copy_out(wire, tiled, run_of(0u, 1u), readback, 0u, copy_into_tiles | copy_out_of_tiles); },
    [](av::D3dLoopback &wire) { copy_out(wire, tiled, run_of(0u, 1u), readback, 0u, 0x8u); },
    [](av::D3dLoopback &wire) { copy_out(wire, tiled, run_of(0u, 4u), upload, tile_bytes, 0u); },
    [](av::D3dLoopback &wire) { copy_out(wire, tiled, run_of(0u, 1u), upload, 5u * tile_bytes, 0u); },
    [](av::D3dLoopback &wire) { copy_out(wire, tiled, run_of(0u, 1u), readback, 2u, 0u); },
    [](av::D3dLoopback &wire) { copy_out(wire, wide, run_of(0u, 1u), readback, 4u, 0u); },
    [](av::D3dLoopback &wire) { copy_out(wire, tiled, run_of(0u, 1u), mirror, 0u, 0u); },
    [](av::D3dLoopback &wire) { copy_out(wire, upload, run_of(0u, 1u), readback, 0u, 0u); },
    [](av::D3dLoopback &wire) { copy_out(wire, tiled, run_of(0u, 1u), 99u, 0u, 0u); },
    [](av::D3dLoopback &wire) {
        CHECK(av_d3d12_CopyTiles12(wire, queue, tiled, TileCoordinate{}, TileSize{1u, 0u, 0u, 0u, 0u}, readback, 0u, 0u));
    },
};

}  // namespace

TEST(d3d12_session_tiles_device, each_region_or_range_past_its_resource_or_heap_removes_a_healthy_device)
{
    av::LoaderHostApis   apis;
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    if (!host_devices(apis, device11, device12)) return;
    for (const Misuse misuse : misuses) {
        av::D3dBackend backend;
        CHECK(backend.initialize(device11, device12));
        av::D3dLoopback wire(backend, guest_bytes, lent_bytes);
        if (!tiled_here(*device12, wire)) {
            backend.shut_down();
            return;
        }
        make_tiles(wire);
        std::uint32_t removed = 1u;
        CHECK(av_d3d12_Finish12(wire, &removed));
        CHECK_EQ(removed, 0u);
        misuse(wire);
        CHECK(av_d3d12_Finish12(wire, &removed));
        CHECK_EQ(removed, driver_internal_error);
        backend.shut_down();
    }
}

namespace {

/* The session alone, its handlers called as the decoder would call them: in
 * static storage, as test_d3d12_session.cpp keeps it. */
av::ObjectStorage<av::D3d12Session> session_storage;
av::D3dObjectTable                  objects;
alignas(64) std::uint8_t guest[4096];
TileRegion one_tile_regions[av::D3d12TileUpdate::most + 1u];

}  // namespace

/* The decoder never hands the session more than one command carries; a
 * session that was handed more anyway maps none of them. */
TEST(d3d12_session_tiles_device, more_regions_than_one_command_carries_map_none)
{
    av::LoaderHostApis   apis;
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    if (!host_devices(apis, device11, device12)) return;
    if (device12->caps().tiled_resources_tier == 0u) return;
    av::GuestRegion region;
    region.take(guest, sizeof guest);
    objects.clear();
    CHECK_STATUS(session_storage.construct(region, objects), av::Status::ok);
    av::D3d12Session &session = session_storage.object();
    session.initialize(device12);
    CHECK(session.on_CreateCommandQueue12(queue, 0u, 0u, 0u));
    CHECK(session.on_CreateReservedResource12(tiled, reserved_texture(), state_copy_dest, ClearValue{}, 0u));
    for (TileRegion &each : one_tile_regions) each = run_of(0u, 1u);
    const TileRange unmapped = {range_null, 0u, 1u};
    CHECK(session.on_UpdateTileMappings12(queue, tiled, av::D3d12TileUpdate::most, one_tile_regions, 0u, 1u,
                                          &unmapped, 0u));
    CHECK(!session.on_UpdateTileMappings12(queue, tiled, av::D3d12TileUpdate::most + 1u, one_tile_regions, 0u, 1u,
                                           &unmapped, 0u));
    session.shut_down();
    session_storage.destroy();
}
