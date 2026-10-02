/* The Direct3D 12 half of a Direct3D session on this machine's GPU, through
 * vkd3d-proton: a render target cleared and a triangle drawn the way the
 * guest's user-mode driver will draw one - shaders as the DDI hands them (a
 * DXIL program and its signatures as registers, tests/fixtures/d3d12/), an
 * input layout naming registers, constants and vertices in an upload heap of
 * guest memory at guest GPU addresses - and read back through a readback
 * heap of guest memory, judged pixel by pixel where the answer is certain.
 *
 * vkd3d-proton is loaded from AV_D3D_PATH, or build-d3d/lib in this checkout
 * (tools/build_d3d_runtime.sh, which applies the host-pointer heap patch this
 * needs). A host without it, or with a vkd3d-proton that cannot import guest
 * memory, returns without judging, as the other device tests do. */
#include "device/surfaces/shared_surfaces.hpp"
#include "fixtures/d3d12/triangle_ps.hpp"
#include "fixtures/d3d12/triangle_vs.hpp"
#include "device/executor/d3d_backend.hpp"
#include "device/shim/d3d11_host_device.hpp"
#include "device/shim/d3d12_host_device.hpp"
#include "device/shim/d3d_headers.hpp"
#include "device/shim/loader_host_apis.hpp"
#include "harness/executor/d3d_loopback.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"
#include "proto/resources/surface_point.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

using namespace av::d3d12proto;

constexpr std::uint32_t format_b8g8r8a8 = 87u;
constexpr std::uint32_t format_r32g32 = 16u;
constexpr std::uint32_t format_r32g32b32a32 = 2u;
constexpr std::uint32_t size = 64u;
constexpr std::uint32_t row_pitch = size * 4u;   /* already 256-byte aligned */
constexpr std::uint32_t heap_default = 1u;
constexpr std::uint32_t heap_upload = 2u;
constexpr std::uint32_t heap_readback = 3u;
constexpr std::uint32_t state_render_target = 0x4u;
constexpr std::uint32_t state_copy_dest = 0x400u;
constexpr std::uint32_t state_copy_source = 0x800u;
constexpr std::uint32_t state_generic_read = 0xac3u;
constexpr std::uint32_t flag_render_target = 0x1u;
constexpr std::uint32_t descriptors_rtv = 2u;
constexpr std::uint32_t dimension_buffer = 1u;
constexpr std::uint32_t dimension_texture2d = 3u;
constexpr std::uint32_t layout_row_major = 1u;
constexpr std::uint32_t topology_type_triangle = 3u;
constexpr std::uint32_t topology_triangles = 4u;
constexpr std::uint32_t visibility_pixel = 5u;
constexpr std::uint32_t root_cbv = 2u;
constexpr std::uint32_t allow_input_layout = 0x1u;
constexpr std::uint32_t driver_internal_error = 0x887A0020u;

/* The guest arena: bulk data is lent from the first 8 MiB, heaps start there. */
constexpr std::size_t   guest_bytes = 32u << 20u;
constexpr std::size_t   lent_bytes = 8u << 20u;
constexpr std::uint64_t readback_at = lent_bytes;
constexpr std::uint64_t upload_at = lent_bytes + (1u << 20u);
constexpr std::uint64_t fence_at = lent_bytes + (2u << 20u);
constexpr std::uint64_t heap_bytes = 64u * 1024u;
/* Guest GPU addresses, as the guest's driver would have chosen them. */
constexpr std::uint64_t readback_address = 0x100000000ull;
constexpr std::uint64_t upload_address = 0x200000000ull;

enum Id : std::uint32_t {
    queue = 1, allocator, list, target, rtv_heap, readback_heap, readback, upload_heap, vertices, constants, fence,
    root, pipeline, signature, arguments, counts, shown, shown_rtv
};

struct Vertex {
    float x, y;
    float r, g, b, a;
};

/* The first GPU here with both devices; nulls when there is none. */
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

/* Whether this vkd3d-proton imports guest memory: a stock one does not. */
bool imports(av::D3d12HostDevice &device, av::D3dLoopback &wire)
{
    ID3D12Heap *const imported = device.import_heap(wire.guest(readback_at), heap_bytes);
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

/* Queue, allocator and an open list; a 64x64 render target and its view; a
 * readback buffer of guest memory the frame is copied into. */
void make_frame(av::D3dLoopback &wire)
{
    std::uint32_t hresult = 1u;
    Caps12 caps = {};
    CHECK(av_d3d12_OpenDevice12(wire, 0u, &hresult, &caps));
    CHECK_EQ(hresult, 0u);
    CHECK(caps.feature_level >= 0xb000u);
    CHECK(caps.descriptor_sizes[descriptors_rtv] != 0u);
    /* The GPU's own description, as Direct3D 11 reports it. */
    CHECK(caps.vendor_id != 0u);
    CHECK(caps.device_id != 0u);
    CHECK(caps.dedicated_video_memory_mb != 0u);
    CHECK(av_d3d12_CreateCommandQueue12(wire, queue, 0u, 0u, 0u));
    CHECK(av_d3d12_CreateCommandAllocator12(wire, allocator, 0u));
    CHECK(av_d3d12_CreateCommandList12(wire, list, allocator, 0u, 0u));

    ResourceDesc texture = {};
    texture.width = size;
    texture.dimension = dimension_texture2d;
    texture.height = size;
    texture.depth_or_array = 1u;
    texture.mip_levels = 1u;
    texture.format = format_b8g8r8a8;
    texture.sample_count = 1u;
    texture.flags = flag_render_target;
    /* The guest driver sizes a placed texture by the host's answer, and
     * reports the host's waves: both come from the device the resource will
     * be made on. A description the host would refuse has no size. */
    std::uint64_t bytes = 0u, alignment = 0u;
    CHECK(av_d3d12_ResourceAllocation12(wire, texture, &bytes, &alignment));
    CHECK(bytes >= std::uint64_t{size} * size * 4u);
    CHECK(alignment != 0u && (alignment & (alignment - 1u)) == 0u);
    ResourceDesc refused = texture;
    refused.dimension = 9u;
    CHECK(av_d3d12_ResourceAllocation12(wire, refused, &bytes, &alignment));
    CHECK_EQ(bytes, ~std::uint64_t{0});
    CHECK(caps.wave_lane_min >= 4u && caps.wave_lane_min <= caps.wave_lane_max);
    CHECK(caps.total_lanes >= caps.wave_lane_max);
    CHECK(av_d3d12_CreateCommittedResource12(wire, target, heap_of(heap_default), 0u, texture, state_render_target,
                                              ClearValue{}, 0u));
    CHECK(av_d3d12_CreateDescriptorHeap12(wire, rtv_heap, descriptors_rtv, 1u, 0u));
    ViewDesc view = {};
    view.resource = target;
    CHECK(av_d3d12_CreateRenderTargetView12(wire, DescriptorRef{rtv_heap, 0u}, 0u, view));

    CHECK(av_d3d12_CreateHeap12(wire, readback_heap, heap_of(heap_readback), 0u, heap_bytes, 0u, readback_at));
    CHECK(av_d3d12_CreatePlacedResource12(wire, readback, readback_heap, 0u, buffer_of(row_pitch * size),
                                           state_copy_dest, ClearValue{}, readback_address));
}

/* The frame into the readback buffer, executed, and waited for. */
void read_frame(av::D3dLoopback &wire, std::uint64_t value)
{
    const Barrier to_copy = {0u, 0u, target, 0xffffffffu, state_render_target, state_copy_source, 0u, 0u};
    CHECK(av_d3d12_ResourceBarrier12(wire, list, 1u, &to_copy));
    CopyLocation into = {};
    into.resource = readback;
    into.type = 1u;
    into.format = format_b8g8r8a8;
    into.width = size;
    into.height = size;
    into.depth = 1u;
    into.row_pitch = row_pitch;
    CopyLocation from = {};
    from.resource = target;
    CHECK(av_d3d12_CopyTextureRegion12(wire, list, into, 0u, 0u, 0u, from, 0u, Box{}));
    const Barrier back = {0u, 0u, target, 0xffffffffu, state_copy_source, state_render_target, 0u, 0u};
    CHECK(av_d3d12_ResourceBarrier12(wire, list, 1u, &back));
    CHECK(av_d3d12_CloseCommandList12(wire, list));
    const std::uint32_t lists[1] = {list};
    CHECK(av_d3d12_ExecuteCommandLists12(wire, queue, 1u, lists));
    CHECK(av_d3d12_SignalFence12(wire, queue, fence, value));
    std::uint64_t completed = 0u;
    CHECK(av_d3d12_FenceValue12(wire, fence, 1u, value, &completed));
    CHECK(completed >= value);
}

std::uint32_t pixel(const av::D3dLoopback &wire, std::uint32_t x, std::uint32_t y)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, wire.guest(readback_at + y * row_pitch + x * 4u), sizeof(value));
    return value;
}

/* Vertices and the tint in one upload heap of guest memory, the root
 * signature (the tint as a root constant buffer view for the pixel stage) and
 * the pipeline, from the shaders as the DDI gives them or whole. */
void make_triangle(av::D3dLoopback &wire, bool whole_containers)
{
    CHECK(av_d3d12_CreateHeap12(wire, upload_heap, heap_of(heap_upload), 0u, heap_bytes, 0u, upload_at));
    const Vertex triangle[3] = {{-1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
                                {-1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
                                {0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f}};
    std::memcpy(wire.guest(upload_at), triangle, sizeof(triangle));
    const float tint[4] = {0.5f, 0.5f, 0.5f, 1.0f};
    std::memcpy(wire.guest(upload_at + 256u), tint, sizeof(tint));
    CHECK(av_d3d12_CreatePlacedResource12(wire, vertices, upload_heap, 0u, buffer_of(256u), state_generic_read,
                                           ClearValue{}, upload_address));
    CHECK(av_d3d12_CreatePlacedResource12(wire, constants, upload_heap, 256u, buffer_of(256u), state_generic_read,
                                           ClearValue{}, upload_address + 256u));

    const RootParameter tint_view = {root_cbv, visibility_pixel, 0u, 0u, 0u, 0u, 0u, 0u};
    CHECK(av_d3d12_CreateRootSignature12(wire, root, allow_input_layout, 1u, &tint_view, 0u, nullptr, 0u, nullptr));

    GraphicsState state = {};
    for (RenderTargetBlend &blend : state.targets) {
        blend.src_blend = blend.src_blend_alpha = 2u;        /* ONE */
        blend.dest_blend = blend.dest_blend_alpha = 1u;      /* ZERO */
        blend.blend_op = blend.blend_op_alpha = 1u;          /* ADD */
        blend.logic_op = 4u;                                  /* NOOP */
        blend.write_mask = 0xfu;
    }
    state.sample_mask = 0xffffffffu;
    state.rasterizer.fill_mode = 3u;   /* SOLID */
    state.rasterizer.cull_mode = 1u;   /* NONE */
    state.rasterizer.depth_clip = 1u;
    state.depth_stencil.depth_func = 2u;
    state.topology_type = topology_type_triangle;
    state.render_targets = 1u;
    state.rtv_formats[0] = format_b8g8r8a8;
    state.sample_count = 1u;
    const InputElement elements[2] = {{0u, 0u, format_r32g32, 0u, 0u, 0u}, {0u, 8u, format_r32g32b32a32, 0u, 0u, 1u}};
    SignatureEntry signatures[8] = {};
    std::uint32_t  count = 0u;
    for (const SignatureEntry &entry : triangle_vs::signatures) signatures[count++] = entry;
    for (const SignatureEntry &entry : triangle_ps::signatures) signatures[count++] = entry;
    const void *const vs = whole_containers ? static_cast<const void *>(triangle_vs::container)
                                            : static_cast<const void *>(triangle_vs::program);
    const void *const ps = whole_containers ? static_cast<const void *>(triangle_ps::container)
                                            : static_cast<const void *>(triangle_ps::program);
    const std::uint64_t vs_bytes = whole_containers ? triangle_vs::container_bytes : triangle_vs::program_bytes;
    const std::uint64_t ps_bytes = whole_containers ? triangle_ps::container_bytes : triangle_ps::program_bytes;
    CHECK(av_d3d12_CreateGraphicsPipeline12(wire, pipeline, root, state, 2u, elements, whole_containers ? 0u : count,
                                             signatures, vs, vs_bytes, nullptr, 0u, nullptr, 0u, nullptr, 0u, ps,
                                             ps_bytes));
}

/* Cleared blue, and everything the triangle is drawn with bound. */
void bind_triangle(av::D3dLoopback &wire)
{
    CHECK(av_d3d12_ClearRenderTargetView12(wire, list, DescriptorRef{rtv_heap, 0u}, Float4{{0.0f, 0.0f, 1.0f, 1.0f}},
                                            0u, nullptr));
    const DescriptorRef targets[1] = {{rtv_heap, 0u}};
    CHECK(av_d3d12_SetRenderTargets12(wire, list, 1u, targets, 0u, DescriptorRef{}));
    const Viewport viewport = {0.0f, 0.0f, float(size), float(size), 0.0f, 1.0f};
    CHECK(av_d3d12_SetViewports12(wire, list, 1u, &viewport));
    const Rect scissor = {0, 0, int(size), int(size)};
    CHECK(av_d3d12_SetScissorRects12(wire, list, 1u, &scissor));
    CHECK(av_d3d12_SetRootSignature12(wire, list, 0u, root));
    CHECK(av_d3d12_SetPipelineState12(wire, list, pipeline));
    CHECK(av_d3d12_SetRootView12(wire, list, 0u, 0u, 0u, upload_address + 256u));
    const VertexBufferView binding = {upload_address, 3u * sizeof(Vertex), sizeof(Vertex)};
    CHECK(av_d3d12_SetVertexBuffers12(wire, list, 0u, 1u, &binding));
    CHECK(av_d3d12_SetTopology12(wire, list, topology_triangles));
}

/* The triangle drawn over the left half and down the middle. */
void draw_triangle(av::D3dLoopback &wire)
{
    bind_triangle(wire);
    CHECK(av_d3d12_DrawInstanced12(wire, list, 3u, 1u, 0u, 0u));
}

void judge_triangle(const av::D3dLoopback &wire)
{
    /* BGRA in memory: 0xAARRGGBB as a little-endian word. Half of red is 0x80
     * after rounding on every driver; the clear is exact. */
    const std::uint32_t tinted = pixel(wire, 4u, size / 2u);
    CHECK_EQ(tinted & 0xff00ffffu, 0xff000000u);
    CHECK((tinted >> 16u & 0xffu) >= 0x7fu && (tinted >> 16u & 0xffu) <= 0x80u);
    CHECK_EQ(pixel(wire, size - 4u, size / 2u), 0xff0000ffu);
}

}  // namespace

TEST(d3d12_session_device, a_cleared_target_reads_back_through_guest_memory)
{
    av::LoaderHostApis apis;
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    if (!host_devices(apis, device11, device12)) return;
    av::D3dBackend backend;
    CHECK(backend.initialize(device11, device12));
    av::D3dLoopback wire(backend, guest_bytes, lent_bytes);
    if (!imports(*device12, wire)) {
        backend.shut_down();
        return;
    }
    std::memset(wire.guest(readback_at), 0xcd, row_pitch * size);
    make_frame(wire);
    CHECK(av_d3d12_CreateFence12(wire, fence, 0u, 0u));
    /* The fence's completed value is published into guest memory. */
    std::uint64_t *const published = reinterpret_cast<std::uint64_t *>(wire.guest(fence_at));
    *published = 99u;
    CHECK(av_d3d12_PublishFence12(wire, fence, fence_at));
    CHECK_EQ(*published, 0u);

    CHECK(av_d3d12_ClearRenderTargetView12(wire, list, DescriptorRef{rtv_heap, 0u}, Float4{{1.0f, 0.0f, 0.0f, 1.0f}},
                                            0u, nullptr));
    read_frame(wire, 1u);
    CHECK_EQ(pixel(wire, 0u, 0u), 0xffff0000u);
    CHECK_EQ(pixel(wire, size - 1u, size - 1u), 0xffff0000u);
    static_cast<void>(backend.poll());
    CHECK_EQ(*published, 1u);
    CHECK_EQ(wire.refused(), 0u);

    /* The frame again, cleared green on the reset list. */
    CHECK(av_d3d12_ResetCommandAllocator12(wire, allocator));
    CHECK(av_d3d12_ResetCommandList12(wire, list, allocator, 0u));
    CHECK(av_d3d12_ClearRenderTargetView12(wire, list, DescriptorRef{rtv_heap, 0u}, Float4{{0.0f, 1.0f, 0.0f, 1.0f}},
                                            0u, nullptr));
    read_frame(wire, 2u);
    CHECK_EQ(pixel(wire, size / 2u, size / 2u), 0xff00ff00u);

    /* Names nothing was made under are refused, never followed: the device is
     * removed as far as the guest can tell, and the session goes on. */
    std::uint32_t removed = 1u;
    CHECK(av_d3d12_Finish12(wire, &removed));
    CHECK_EQ(removed, 0u);
    CHECK(av_d3d12_CloseCommandList12(wire, 77u));
    CHECK(av_d3d12_ClearRenderTargetView12(wire, list, DescriptorRef{rtv_heap, 5u}, Float4{}, 0u, nullptr));
    CHECK(av_d3d12_SetRootView12(wire, list, 0u, 0u, 0u, 0x999u));
    CHECK(av_d3d12_Finish12(wire, &removed));
    CHECK_EQ(removed, driver_internal_error);
    CHECK_EQ(wire.refused(), 0u);
    CHECK(backend.invariants_hold());
    backend.shut_down();
}

TEST(d3d12_session_device, a_triangle_drawn_from_ddi_dxil_reads_back_where_it_was_drawn)
{
    av::LoaderHostApis apis;
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    if (!host_devices(apis, device11, device12)) return;
    for (int whole = 0; whole < 2; whole++) {
        av::D3dBackend backend;
        CHECK(backend.initialize(device11, device12));
        av::D3dLoopback wire(backend, guest_bytes, lent_bytes);
        if (!imports(*device12, wire)) {
            backend.shut_down();
            return;
        }
        make_frame(wire);
        CHECK(av_d3d12_CreateFence12(wire, fence, 0u, 0u));
        make_triangle(wire, whole != 0);
        CHECK_EQ(wire.refused(), 0u);
        draw_triangle(wire);
        read_frame(wire, 1u);
        judge_triangle(wire);
        std::uint32_t removed = 1u;
        CHECK(av_d3d12_Finish12(wire, &removed));
        CHECK_EQ(removed, 0u);
        CHECK_EQ(wire.refused(), 0u);
        CHECK(backend.invariants_hold());
        backend.shut_down();
    }
}

namespace {

constexpr std::uint64_t arguments_at = 512u, counts_at = 768u, argument_bytes = 256u;
constexpr std::uint32_t draw_bytes = 16u;   /* D3D12_DRAW_ARGUMENTS */
constexpr IndirectArgument draw_argument = {0u, 0u, 0u, 0u};

/* The frame and the triangle, a draw of it in an argument buffer of guest
 * memory, a count of one in a count buffer, and a signature of one draw. */
void make_indirect(av::D3dLoopback &wire)
{
    make_frame(wire);
    CHECK(av_d3d12_CreateFence12(wire, fence, 0u, 0u));
    make_triangle(wire, false);
    const std::uint32_t draw[4] = {3u, 1u, 0u, 0u};
    std::memcpy(wire.guest(upload_at + arguments_at), draw, sizeof draw);
    const std::uint32_t one = 1u;
    std::memcpy(wire.guest(upload_at + counts_at), &one, sizeof one);
    CHECK(av_d3d12_CreatePlacedResource12(wire, arguments, upload_heap, arguments_at, buffer_of(argument_bytes),
                                           state_generic_read, ClearValue{}, upload_address + arguments_at));
    CHECK(av_d3d12_CreatePlacedResource12(wire, counts, upload_heap, counts_at, buffer_of(argument_bytes),
                                           state_generic_read, ClearValue{}, upload_address + counts_at));
    CHECK(av_d3d12_CreateCommandSignature12(wire, signature, 0u, draw_bytes, 1u, &draw_argument));
}

/* One command the host must refuse, sent to a session that was healthy. */
using Misuse = void (*)(av::D3dLoopback &wire);

/* The triangle's root signature has one parameter, a CBV. */
constexpr Misuse misuses[] = {
    [](av::D3dLoopback &wire) {   /* root constants on a CBV parameter */
        const IndirectArgument taken[2] = {{5u, 0u, 0u, 1u}, draw_argument};
        CHECK(av_d3d12_CreateCommandSignature12(wire, signature + 1u, root, 20u, 2u, taken));
    },
    [](av::D3dLoopback &wire) {   /* a CBV at a parameter past the last */
        const IndirectArgument taken[2] = {{6u, 1u, 0u, 0u}, draw_argument};
        CHECK(av_d3d12_CreateCommandSignature12(wire, signature + 1u, root, 24u, 2u, taken));
    },
    [](av::D3dLoopback &wire) {   /* a root that is no root signature */
        const IndirectArgument taken[2] = {{6u, 0u, 0u, 0u}, draw_argument};
        CHECK(av_d3d12_CreateCommandSignature12(wire, signature + 1u, pipeline, 24u, 2u, taken));
    },
    [](av::D3dLoopback &wire) {   /* one command past the argument buffer */
        CHECK(av_d3d12_ExecuteIndirect12(wire, list, signature, argument_bytes / draw_bytes + 1u, arguments, 0u,
                                          0u, 0u));
    },
    [](av::D3dLoopback &wire) {   /* a count off its word */
        CHECK(av_d3d12_ExecuteIndirect12(wire, list, signature, 1u, arguments, 0u, counts, 2u));
    },
    [](av::D3dLoopback &wire) {   /* a count past its buffer */
        CHECK(av_d3d12_ExecuteIndirect12(wire, list, signature, 1u, arguments, 0u, counts, argument_bytes));
    },
    [](av::D3dLoopback &wire) {   /* a count in no buffer */
        CHECK(av_d3d12_ExecuteIndirect12(wire, list, signature, 1u, arguments, 0u, pipeline, 0u));
    },
    [](av::D3dLoopback &wire) {   /* arguments in a texture */
        CHECK(av_d3d12_ExecuteIndirect12(wire, list, signature, 1u, target, 0u, 0u, 0u));
    },
    [](av::D3dLoopback &wire) {   /* a count in a texture */
        CHECK(av_d3d12_ExecuteIndirect12(wire, list, signature, 1u, arguments, 0u, target, 0u));
    },
    [](av::D3dLoopback &wire) {   /* a signature that is none */
        CHECK(av_d3d12_ExecuteIndirect12(wire, list, pipeline, 1u, arguments, 0u, 0u, 0u));
    },
};

}  // namespace

/* The same triangle drawn by ExecuteIndirect: its draw read from an argument
 * buffer of guest memory, up to four commands of which a count buffer says
 * one. */
TEST(d3d12_session_device, a_triangle_drawn_indirectly_reads_back_where_it_was_drawn)
{
    av::LoaderHostApis apis;
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    if (!host_devices(apis, device11, device12)) return;
    av::D3dBackend backend;
    CHECK(backend.initialize(device11, device12));
    av::D3dLoopback wire(backend, guest_bytes, lent_bytes);
    if (!imports(*device12, wire)) {
        backend.shut_down();
        return;
    }
    make_indirect(wire);
    bind_triangle(wire);
    CHECK(av_d3d12_ExecuteIndirect12(wire, list, signature, 4u, arguments, 0u, counts, 0u));
    read_frame(wire, 1u);
    judge_triangle(wire);
    std::uint32_t removed = 1u;
    CHECK(av_d3d12_Finish12(wire, &removed));
    CHECK_EQ(removed, 0u);
    CHECK_EQ(wire.refused(), 0u);
    CHECK(backend.invariants_hold());
    backend.shut_down();
}

/* Everything the host refuses on the real device - a root argument naming a
 * parameter of another kind or none, a root that is no root signature,
 * commands past the argument buffer, a count off its word or in no buffer,
 * a signature that is none - each alone, on a session that was healthy until
 * it came: the device is removed after it and not before. */
TEST(d3d12_session_device, each_indirect_misuse_removes_a_healthy_device)
{
    av::LoaderHostApis apis;
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    if (!host_devices(apis, device11, device12)) return;
    for (const Misuse misuse : misuses) {
        av::D3dBackend backend;
        CHECK(backend.initialize(device11, device12));
        av::D3dLoopback wire(backend, guest_bytes, lent_bytes);
        if (!imports(*device12, wire)) {
            backend.shut_down();
            return;
        }
        make_indirect(wire);
        std::uint32_t removed = 1u;
        CHECK(av_d3d12_Finish12(wire, &removed));
        CHECK_EQ(removed, 0u);
        misuse(wire);
        CHECK(av_d3d12_Finish12(wire, &removed));
        CHECK_EQ(removed, driver_internal_error);
        backend.shut_down();
    }
}

/* A shader-visible sampler heap of the size the guest's runtime reports - more
 * than the 2048 vkd3d-proton makes - is made with 2048: a sampler at the last
 * of them is written, one past them is refused as outside the heap. */
TEST(d3d12_session_device, a_sampler_heap_past_2048_is_made_with_2048)
{
    av::LoaderHostApis apis;
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    if (!host_devices(apis, device11, device12)) return;
    constexpr std::uint32_t descriptors_sampler = 1u, shader_visible = 1u, reported = 4000u, made = 2048u;
    const std::uint32_t indices[2] = {made - 1u, made};
    for (const std::uint32_t index : indices) {
        av::D3dBackend backend;
        CHECK(backend.initialize(device11, device12));
        av::D3dLoopback wire(backend, guest_bytes, lent_bytes);
        std::uint32_t hresult = 1u;
        Caps12 caps = {};
        CHECK(av_d3d12_OpenDevice12(wire, 0u, &hresult, &caps));
        CHECK(av_d3d12_CreateDescriptorHeap12(wire, rtv_heap, descriptors_sampler, reported, shader_visible));
        SamplerDesc sampler = {};
        sampler.filter = 0u;
        sampler.address_u = sampler.address_v = sampler.address_w = 1u;   /* WRAP */
        sampler.comparison = 1u;
        sampler.max_lod = 1000.0f;
        CHECK(av_d3d12_CreateSampler12(wire, DescriptorRef{rtv_heap, index}, sampler));
        std::uint32_t removed = 1u;
        CHECK(av_d3d12_Finish12(wire, &removed));
        CHECK_EQ(removed, index < made ? 0u : driver_internal_error);
        backend.shut_down();
    }
}

/* A heap as D3D12Core makes one - with a buffer over all of it - and a buffer
 * placed inside it, at the address that is the spanning buffer's plus the
 * offset: it is taken, since the host placed both at the same offsets. A
 * buffer whose guest address says it is somewhere its host placement is not
 * is refused. */
TEST(d3d12_session_device, a_buffer_placed_inside_a_spanning_buffer_is_taken_and_a_misplaced_one_refused)
{
    av::LoaderHostApis apis;
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    if (!host_devices(apis, device11, device12)) return;
    constexpr std::uint64_t heap_size = 1u << 20u, piece = 64u * 1024u, base = 0x300000000ull;
    constexpr std::uint32_t heap = 1u, spanning = 2u, inner = 3u, misplaced = 4u;
    av::D3dBackend backend;
    CHECK(backend.initialize(device11, device12));
    av::D3dLoopback wire(backend, guest_bytes, lent_bytes);
    std::uint32_t hresult = 1u;
    Caps12 caps = {};
    CHECK(av_d3d12_OpenDevice12(wire, 0u, &hresult, &caps));
    CHECK(av_d3d12_CreateHeap12(wire, heap, heap_of(heap_default), 0u, heap_size, piece, 0u));
    CHECK(av_d3d12_CreatePlacedResource12(wire, spanning, heap, 0u, buffer_of(heap_size), 0u, ClearValue{}, base));
    CHECK(av_d3d12_CreatePlacedResource12(wire, inner, heap, piece, buffer_of(piece), 0u, ClearValue{}, base + piece));
    std::uint32_t removed = 1u;
    CHECK(av_d3d12_Finish12(wire, &removed));
    CHECK_EQ(removed, 0u);
    /* Placed two pieces in, but claiming the address of one piece in. */
    CHECK(av_d3d12_CreatePlacedResource12(wire, misplaced, heap, 2u * piece, buffer_of(piece), 0u, ClearValue{},
                                           base + piece));
    CHECK(av_d3d12_Finish12(wire, &removed));
    CHECK_EQ(removed, driver_internal_error);
    CHECK(backend.invariants_hold());
    backend.shut_down();
}

namespace {

constexpr std::uint32_t heap_flag_shared = 0x1u;
/* What the miniport would have minted over the buffer's kernel allocation. */
constexpr std::uint64_t shown_key = 0x5eed00000000d312ull;
constexpr std::uint32_t opaque_red = 0xffff0000u;
constexpr std::uint32_t opaque_green = 0xff00ff00u;

/* The frame make_frame makes, and beside its target a 64x64 swap chain
 * buffer made shared, with a view, shown under shown_key. */
void make_shown(av::D3dLoopback &wire)
{
    make_frame(wire);
    ResourceDesc texture = {};
    texture.width = size;
    texture.dimension = dimension_texture2d;
    texture.height = size;
    texture.depth_or_array = 1u;
    texture.mip_levels = 1u;
    texture.format = format_b8g8r8a8;
    texture.sample_count = 1u;
    texture.flags = flag_render_target;
    CHECK(av_d3d12_CreateCommittedResource12(wire, shown, heap_of(heap_default), heap_flag_shared, texture,
                                              state_render_target, ClearValue{}, 0u));
    CHECK(av_d3d12_CreateDescriptorHeap12(wire, shown_rtv, descriptors_rtv, 1u, 0u));
    ViewDesc view = {};
    view.resource = shown;
    CHECK(av_d3d12_CreateRenderTargetView12(wire, DescriptorRef{shown_rtv, 0u}, 0u, view));
    CHECK(av_d3d12_ShareResource12(wire, shown, shown_key));
}

/* The buffer cleared `colour` and executed, and the list open again. */
void draw_cleared(av::D3dLoopback &wire, Float4 colour)
{
    CHECK(av_d3d12_ClearRenderTargetView12(wire, list, DescriptorRef{shown_rtv, 0u}, colour, 0u, nullptr));
    CHECK(av_d3d12_CloseCommandList12(wire, list));
    const std::uint32_t lists[1] = {list};
    CHECK(av_d3d12_ExecuteCommandLists12(wire, queue, 1u, lists));
    CHECK(av_d3d12_ResetCommandAllocator12(wire, allocator));
    CHECK(av_d3d12_ResetCommandList12(wire, list, allocator, 0u));
}

/* The buffer cleared `colour`, executed and presented as its `point`th present. */
void present_cleared(av::D3dLoopback &wire, Float4 colour, std::uint32_t point)
{
    CHECK(av_d3d12_ClearRenderTargetView12(wire, list, DescriptorRef{shown_rtv, 0u}, colour, 0u, nullptr));
    CHECK(av_d3d12_CloseCommandList12(wire, list));
    const std::uint32_t lists[1] = {list};
    CHECK(av_d3d12_ExecuteCommandLists12(wire, queue, 1u, lists));
    CHECK(av_d3d12_PresentResource12(wire, queue, shown, point));
    CHECK(av_d3d12_ResetCommandAllocator12(wire, allocator));
    CHECK(av_d3d12_ResetCommandList12(wire, list, allocator, 0u));
}

/* Whether every pixel of a staged frame is `expected`. */
bool frame_everywhere(const AvFrameView &view, std::uint32_t expected)
{
    for (std::uint32_t y = 0u; y < view.height; y++)
        for (std::uint32_t x = 0u; x < view.width; x++) {
            std::uint32_t value = 0u;
            std::memcpy(&value, view.pixels + y * view.row_pitch + x * 4u, sizeof value);
            if (value != expected) return false;
        }
    return view.width == size && view.height == size;
}

/* Whether every pixel of what the Direct3D 11 device has under shown_key -
 * DWM's view of the buffer - is `expected`. */
bool shown_everywhere(av::D3d11HostDevice &device11, std::uint32_t expected)
{
    std::uint32_t pixels[size * size] = {};
    if (!device11.read_image(shown_key, pixels, row_pitch, size, size)) return false;
    for (const std::uint32_t value : pixels)
        if (value != expected) return false;
    return true;
}

using ShowMisuse = void (*)(av::D3dLoopback &);

const ShowMisuse show_misuses[] = {
    [](av::D3dLoopback &wire) { CHECK(av_d3d12_ShareResource12(wire, target, shown_key + 1u)); },
    [](av::D3dLoopback &wire) { CHECK(av_d3d12_ShareResource12(wire, shown, 0u)); },
    [](av::D3dLoopback &wire) { CHECK(av_d3d12_ShareResource12(wire, 99u, shown_key + 1u)); },
    [](av::D3dLoopback &wire) { CHECK(av_d3d12_ShareResource12(wire, shown, shown_key)); },
    [](av::D3dLoopback &wire) { CHECK(av_d3d12_PresentResource12(wire, 99u, shown, 1u)); },
    [](av::D3dLoopback &wire) { CHECK(av_d3d12_PresentResource12(wire, queue, 99u, 1u)); },
};

}  // namespace

/* A swap chain buffer drawn on Direct3D 12 is the texture the Direct3D 11
 * device - the compositor's - has under the buffer's key: the same memory,
 * each present's pixels there once the presenting queue has drawn them. */
TEST(d3d12_session_device, a_shared_buffer_drawn_on_d3d12_is_what_d3d11_reads_under_its_key)
{
    av::LoaderHostApis apis;
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    if (!host_devices(apis, device11, device12)) return;
    av::D3dBackend backend;
    CHECK(backend.initialize(device11, device12));
    av::D3dLoopback wire(backend, guest_bytes, lent_bytes);
    if (!imports(*device12, wire)) {
        backend.shut_down();
        return;
    }
    make_shown(wire);
    present_cleared(wire, Float4{{1.0f, 0.0f, 0.0f, 1.0f}}, 1u);
    CHECK(shown_everywhere(*device11, opaque_red));
    present_cleared(wire, Float4{{0.0f, 1.0f, 0.0f, 1.0f}}, 2u);
    CHECK(shown_everywhere(*device11, opaque_green));
    std::uint32_t removed = 1u;
    CHECK(av_d3d12_Finish12(wire, &removed));
    CHECK_EQ(removed, 0u);
    CHECK_EQ(wire.refused(), 0u);
    CHECK(backend.invariants_hold());
    backend.shut_down();
    /* The guest let go of it everywhere: nothing is shown under the key. */
    device11->release_shared(shown_key);
    CHECK(!shown_everywhere(*device11, opaque_green));
}

/* A blt reads a swap chain buffer by its shared id and the point a present
 * published (docs/shared-surfaces.md): the first blt to ask finds nothing
 * staged - nobody knew one would - and from then on every present of it is
 * staged on its queue, behind the work that drew it and ahead of what follows:
 * a clear after the present never reaches the frame. */
TEST(d3d12_session_device, a_buffer_a_blt_read_is_staged_at_each_present_in_its_queue_s_order)
{
    av::LoaderHostApis apis;
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    if (!host_devices(apis, device11, device12)) return;
    av::SharedSurfaces surfaces;
    av::D3dBackend     backend;
    CHECK(backend.initialize(device11, device12, &surfaces));
    av::D3dLoopback wire(backend, guest_bytes, lent_bytes);
    if (!imports(*device12, wire)) {
        backend.shut_down();
        return;
    }
    make_shown(wire);
    present_cleared(wire, Float4{{1.0f, 0.0f, 0.0f, 1.0f}}, 1u);
    std::uint32_t removed = 1u;
    CHECK(av_d3d12_Finish12(wire, &removed));
    AvFrameView view = {};
    CHECK(!surfaces.map(av::SurfacePoint(shown_key, 1u), 1000u, view));
    present_cleared(wire, Float4{{0.0f, 1.0f, 0.0f, 1.0f}}, 2u);
    draw_cleared(wire, Float4{{0.0f, 0.0f, 1.0f, 1.0f}});
    CHECK(surfaces.map(av::SurfacePoint(shown_key, 2u), 1000u, view));
    CHECK(frame_everywhere(view, opaque_green));
    surfaces.unmap(av::SurfacePoint(shown_key, 2u));
    CHECK(av_d3d12_Finish12(wire, &removed));
    CHECK_EQ(removed, 0u);
    CHECK_EQ(wire.refused(), 0u);
    CHECK(surfaces.invariants_hold());
    backend.shut_down();
    surfaces.release(0u);
    device11->release_shared(shown_key);
}

/* A share of a texture not made shared, of none, under key 0 or a key already
 * shown; a present from a queue or of a resource nothing names: each alone
 * removes a session that was healthy until it came. */
TEST(d3d12_session_device, each_share_or_present_misuse_removes_a_healthy_device)
{
    av::LoaderHostApis apis;
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    if (!host_devices(apis, device11, device12)) return;
    for (const ShowMisuse misuse : show_misuses) {
        av::D3dBackend backend;
        CHECK(backend.initialize(device11, device12));
        av::D3dLoopback wire(backend, guest_bytes, lent_bytes);
        make_shown(wire);
        std::uint32_t removed = 1u;
        CHECK(av_d3d12_Finish12(wire, &removed));
        CHECK_EQ(removed, 0u);
        misuse(wire);
        CHECK(av_d3d12_Finish12(wire, &removed));
        CHECK_EQ(removed, driver_internal_error);
        backend.shut_down();
        device11->release_shared(shown_key);
    }
}
