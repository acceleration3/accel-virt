/* The Direct3D 12 half of a Direct3D session on this machine's GPU, through
 * vkd3d-proton: a render target cleared and a triangle drawn the way the
 * guest's user-mode driver will draw one - shaders as the DDI hands them (a
 * DXIL program and its signatures as registers, tests/fixtures/d3d12/), an
 * input layout naming registers, constants and vertices in an upload heap of
 * guest memory at guest GPU addresses - and read back through a readback
 * heap of guest memory, judged pixel by pixel where the answer is certain.
 *
 * vkd3d-proton is loaded from RVK_D3D_PATH, or build-d3d/lib in this checkout
 * (tools/build_d3d_runtime.sh, which applies the host-pointer heap patch this
 * needs). A host without it, or with a vkd3d-proton that cannot import guest
 * memory, returns without judging, as the other device tests do. */
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
    root, pipeline
};

struct Vertex {
    float x, y;
    float r, g, b, a;
};

/* The first GPU here with both devices; nulls when there is none. */
bool host_devices(av::LoaderHostApis &apis, av::D3d11HostDevice *&device11, av::D3d12HostDevice *&device12)
{
    if (std::getenv("RVK_D3D_PATH") == nullptr) setenv("RVK_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
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
    CHECK(rvk_d3d12_OpenDevice12(wire, 0u, &hresult, &caps));
    CHECK_EQ(hresult, 0u);
    CHECK(caps.feature_level >= 0xb000u);
    CHECK(caps.descriptor_sizes[descriptors_rtv] != 0u);
    CHECK(rvk_d3d12_CreateCommandQueue12(wire, queue, 0u, 0u, 0u));
    CHECK(rvk_d3d12_CreateCommandAllocator12(wire, allocator, 0u));
    CHECK(rvk_d3d12_CreateCommandList12(wire, list, allocator, 0u, 0u));

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
    CHECK(rvk_d3d12_ResourceAllocation12(wire, texture, &bytes, &alignment));
    CHECK(bytes >= std::uint64_t{size} * size * 4u);
    CHECK(alignment != 0u && (alignment & (alignment - 1u)) == 0u);
    ResourceDesc refused = texture;
    refused.dimension = 9u;
    CHECK(rvk_d3d12_ResourceAllocation12(wire, refused, &bytes, &alignment));
    CHECK_EQ(bytes, ~std::uint64_t{0});
    CHECK(caps.wave_lane_min >= 4u && caps.wave_lane_min <= caps.wave_lane_max);
    CHECK(caps.total_lanes >= caps.wave_lane_max);
    CHECK(rvk_d3d12_CreateCommittedResource12(wire, target, heap_of(heap_default), 0u, texture, state_render_target,
                                              ClearValue{}, 0u));
    CHECK(rvk_d3d12_CreateDescriptorHeap12(wire, rtv_heap, descriptors_rtv, 1u, 0u));
    ViewDesc view = {};
    view.resource = target;
    CHECK(rvk_d3d12_CreateRenderTargetView12(wire, DescriptorRef{rtv_heap, 0u}, 0u, view));

    CHECK(rvk_d3d12_CreateHeap12(wire, readback_heap, heap_of(heap_readback), 0u, heap_bytes, 0u, readback_at));
    CHECK(rvk_d3d12_CreatePlacedResource12(wire, readback, readback_heap, 0u, buffer_of(row_pitch * size),
                                           state_copy_dest, ClearValue{}, readback_address));
}

/* The frame into the readback buffer, executed, and waited for. */
void read_frame(av::D3dLoopback &wire, std::uint64_t value)
{
    const Barrier to_copy = {0u, 0u, target, 0xffffffffu, state_render_target, state_copy_source, 0u, 0u};
    CHECK(rvk_d3d12_ResourceBarrier12(wire, list, 1u, &to_copy));
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
    CHECK(rvk_d3d12_CopyTextureRegion12(wire, list, into, 0u, 0u, 0u, from, 0u, Box{}));
    const Barrier back = {0u, 0u, target, 0xffffffffu, state_copy_source, state_render_target, 0u, 0u};
    CHECK(rvk_d3d12_ResourceBarrier12(wire, list, 1u, &back));
    CHECK(rvk_d3d12_CloseCommandList12(wire, list));
    const std::uint32_t lists[1] = {list};
    CHECK(rvk_d3d12_ExecuteCommandLists12(wire, queue, 1u, lists));
    CHECK(rvk_d3d12_SignalFence12(wire, queue, fence, value));
    std::uint64_t completed = 0u;
    CHECK(rvk_d3d12_FenceValue12(wire, fence, 1u, value, &completed));
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
    CHECK(rvk_d3d12_CreateHeap12(wire, upload_heap, heap_of(heap_upload), 0u, heap_bytes, 0u, upload_at));
    const Vertex triangle[3] = {{-1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
                                {-1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
                                {0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f}};
    std::memcpy(wire.guest(upload_at), triangle, sizeof(triangle));
    const float tint[4] = {0.5f, 0.5f, 0.5f, 1.0f};
    std::memcpy(wire.guest(upload_at + 256u), tint, sizeof(tint));
    CHECK(rvk_d3d12_CreatePlacedResource12(wire, vertices, upload_heap, 0u, buffer_of(256u), state_generic_read,
                                           ClearValue{}, upload_address));
    CHECK(rvk_d3d12_CreatePlacedResource12(wire, constants, upload_heap, 256u, buffer_of(256u), state_generic_read,
                                           ClearValue{}, upload_address + 256u));

    const RootParameter tint_view = {root_cbv, visibility_pixel, 0u, 0u, 0u, 0u, 0u, 0u};
    CHECK(rvk_d3d12_CreateRootSignature12(wire, root, allow_input_layout, 1u, &tint_view, 0u, nullptr, 0u, nullptr));

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
    CHECK(rvk_d3d12_CreateGraphicsPipeline12(wire, pipeline, root, state, 2u, elements, whole_containers ? 0u : count,
                                             signatures, vs, vs_bytes, nullptr, 0u, nullptr, 0u, nullptr, 0u, ps,
                                             ps_bytes));
}

/* Cleared blue, the triangle drawn over the left half and down the middle. */
void draw_triangle(av::D3dLoopback &wire)
{
    CHECK(rvk_d3d12_ClearRenderTargetView12(wire, list, DescriptorRef{rtv_heap, 0u}, Float4{{0.0f, 0.0f, 1.0f, 1.0f}},
                                            0u, nullptr));
    const DescriptorRef targets[1] = {{rtv_heap, 0u}};
    CHECK(rvk_d3d12_SetRenderTargets12(wire, list, 1u, targets, 0u, DescriptorRef{}));
    const Viewport viewport = {0.0f, 0.0f, float(size), float(size), 0.0f, 1.0f};
    CHECK(rvk_d3d12_SetViewports12(wire, list, 1u, &viewport));
    const Rect scissor = {0, 0, int(size), int(size)};
    CHECK(rvk_d3d12_SetScissorRects12(wire, list, 1u, &scissor));
    CHECK(rvk_d3d12_SetRootSignature12(wire, list, 0u, root));
    CHECK(rvk_d3d12_SetPipelineState12(wire, list, pipeline));
    CHECK(rvk_d3d12_SetRootView12(wire, list, 0u, 0u, 0u, upload_address + 256u));
    const VertexBufferView binding = {upload_address, 3u * sizeof(Vertex), sizeof(Vertex)};
    CHECK(rvk_d3d12_SetVertexBuffers12(wire, list, 0u, 1u, &binding));
    CHECK(rvk_d3d12_SetTopology12(wire, list, topology_triangles));
    CHECK(rvk_d3d12_DrawInstanced12(wire, list, 3u, 1u, 0u, 0u));
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
    CHECK(rvk_d3d12_CreateFence12(wire, fence, 0u, 0u));
    /* The fence's completed value is published into guest memory. */
    std::uint64_t *const published = reinterpret_cast<std::uint64_t *>(wire.guest(fence_at));
    *published = 99u;
    CHECK(rvk_d3d12_PublishFence12(wire, fence, fence_at));
    CHECK_EQ(*published, 0u);

    CHECK(rvk_d3d12_ClearRenderTargetView12(wire, list, DescriptorRef{rtv_heap, 0u}, Float4{{1.0f, 0.0f, 0.0f, 1.0f}},
                                            0u, nullptr));
    read_frame(wire, 1u);
    CHECK_EQ(pixel(wire, 0u, 0u), 0xffff0000u);
    CHECK_EQ(pixel(wire, size - 1u, size - 1u), 0xffff0000u);
    static_cast<void>(backend.poll());
    CHECK_EQ(*published, 1u);
    CHECK_EQ(wire.refused(), 0u);

    /* The frame again, cleared green on the reset list. */
    CHECK(rvk_d3d12_ResetCommandAllocator12(wire, allocator));
    CHECK(rvk_d3d12_ResetCommandList12(wire, list, allocator, 0u));
    CHECK(rvk_d3d12_ClearRenderTargetView12(wire, list, DescriptorRef{rtv_heap, 0u}, Float4{{0.0f, 1.0f, 0.0f, 1.0f}},
                                            0u, nullptr));
    read_frame(wire, 2u);
    CHECK_EQ(pixel(wire, size / 2u, size / 2u), 0xff00ff00u);

    /* Names nothing was made under are refused, never followed: the device is
     * removed as far as the guest can tell, and the session goes on. */
    std::uint32_t removed = 1u;
    CHECK(rvk_d3d12_Finish12(wire, &removed));
    CHECK_EQ(removed, 0u);
    CHECK(rvk_d3d12_CloseCommandList12(wire, 77u));
    CHECK(rvk_d3d12_ClearRenderTargetView12(wire, list, DescriptorRef{rtv_heap, 5u}, Float4{}, 0u, nullptr));
    CHECK(rvk_d3d12_SetRootView12(wire, list, 0u, 0u, 0u, 0x999u));
    CHECK(rvk_d3d12_Finish12(wire, &removed));
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
        CHECK(rvk_d3d12_CreateFence12(wire, fence, 0u, 0u));
        make_triangle(wire, whole != 0);
        CHECK_EQ(wire.refused(), 0u);
        draw_triangle(wire);
        read_frame(wire, 1u);
        judge_triangle(wire);
        std::uint32_t removed = 1u;
        CHECK(rvk_d3d12_Finish12(wire, &removed));
        CHECK_EQ(removed, 0u);
        CHECK_EQ(wire.refused(), 0u);
        CHECK(backend.invariants_hold());
        backend.shut_down();
    }
}
