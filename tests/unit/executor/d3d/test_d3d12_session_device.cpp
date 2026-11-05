#include "executor/d3d/shared_surfaces.hpp"
#include "fixtures/d3d12/triangle_ps.hpp"
#include "fixtures/d3d12/triangle_vs.hpp"
#include "executor/d3d/d3d_backend.hpp"
#include "executor/host_memory_source.hpp"
#include "executor/owned_fd.hpp"
#include "executor/import_table.hpp"
#include "executor/imported_blob.hpp"
#include "proto/submission/blob_role.hpp"
#include "executor/source_memory.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/d3d12_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/sharing/host_shares.hpp"
#include "executor/sharing/session_shares.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "harness/executor/d3d_loopback.hpp"
#include "harness/executor/local_import.hpp"
#include "executor/shim/host_heap.hpp"
#include "harness/executor/executor_log_rig.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"
#include "proto/resources/host_memory_request.hpp"
#include "proto/resources/surface_point.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace {

using namespace av::d3d12proto;

constexpr std::uint32_t format_b8g8r8a8 = 87u;
constexpr std::uint32_t format_r32g32 = 16u;
constexpr std::uint32_t format_r32g32b32a32 = 2u;
constexpr std::uint32_t size = 64u;
constexpr std::uint32_t row_pitch = size * 4u;
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

constexpr std::size_t   region_bytes = 16u << 20u;
constexpr std::size_t   lent_bytes = 8u << 20u;
constexpr std::uint64_t heap_bytes = 64u * 1024u;
constexpr std::uint64_t readback_address = 0x100000000ull;
constexpr std::uint64_t upload_address = 0x200000000ull;

const av::HostMemoryRequest upload_memory(av::HostMemoryRequest::Kind::upload, 0u);
const av::HostMemoryRequest readback_memory(av::HostMemoryRequest::Kind::readback, 0u);

enum Id : std::uint32_t {
    queue = 1, allocator, list, target, rtv_heap, readback_heap, readback, upload_heap, vertices, constants,
    root, pipeline, signature, arguments, counts, shown, shown_rtv
};

struct HeapMemory {
    std::uint64_t readback;
    std::uint64_t upload;
};
HeapMemory heaps = {};

struct Vertex {
    float x, y;
    float r, g, b, a;
};

bool host_devices(av::LoaderHostApis &apis, av::D3d11HostDevice *&device11, av::D3d12HostDevice *&device12,
                  av::HostShares *&shares, av::HostMemorySource *&source)
{
    if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
    apis.open();
    for (std::uint32_t index = 0u; index < apis.gpu_count(); index++) {
        device11 = apis.d3d11_device(index);
        device12 = apis.d3d12_device(index);
        shares = apis.shares(index);
        source = apis.memory_source(index);
        if (device11 != nullptr && device12 != nullptr && source != nullptr) return true;
    }
    std::fprintf(stderr, "no GPU here runs vkd3d-proton: nothing judged\n");
    return false;
}

av::SessionShares *shared_as(av::SessionShares &session, av::HostShares *shares, av::D3dBackend &backend)
{
    session.bind(*shares, 1u, backend);
    return &session;
}

bool imports(av::D3d12HostDevice &device, av::D3dLoopback &wire)
{
    const std::uint64_t        memory = wire.open_memory(readback_memory, heap_bytes);
    const av::SourceMemory *const held = wire.source_memory(memory);
    av::OwnedFd                shared;
    const bool                 exported = held != nullptr && held->share(shared);
    ID3D12Heap *const imported =
        exported ? device.import_dmabuf_heap(held->pointer(), shared.fd(), held->size()) : nullptr;
    wire.close_memory(memory);
    if (imported == nullptr) {
        std::fprintf(stderr, "this vkd3d-proton opens no heap over a dma-buf (tools/vkd3d-proton-patches): nothing judged\n");
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

void make_frame(av::D3dLoopback &wire)
{
    std::uint32_t hresult = 1u;
    CHECK(av_d3d12_OpenDevice12(wire, 0u, &hresult));
    CHECK_EQ(hresult, 0u);
    Caps12        caps = {};
    std::uint8_t  formats[4096] = {};
    std::uint64_t formats_size = 0u;
    hresult = 1u;
    CHECK(av_d3d12_QueryCaps12(wire, &hresult, &caps, formats, sizeof formats, &formats_size));
    CHECK_EQ(hresult, 0u);
    CHECK(caps.feature_level >= 0xb000u);
    CHECK(caps.descriptor_sizes[descriptors_rtv] != 0u);
    CHECK(formats_size != 0u);
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

    heaps.readback = wire.open_memory(readback_memory, heap_bytes);
    CHECK(heaps.readback != 0u);
    std::memset(wire.memory(heaps.readback), 0xcd, heap_bytes);
    CHECK(av_d3d12_CreateHeap12(wire, readback_heap, heap_of(heap_readback), 0u, heap_bytes, 0u, heaps.readback, 0u));
    CHECK(av_d3d12_CreatePlacedResource12(wire, readback, readback_heap, 0u, buffer_of(row_pitch * size),
                                           state_copy_dest, ClearValue{}, readback_address));
}

void read_frame(av::D3dLoopback &wire)
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
    std::uint32_t removed = 1u;
    CHECK(av_d3d12_Finish12(wire, &removed));
    CHECK_EQ(removed, 0u);
}

std::uint32_t pixel(const av::D3dLoopback &wire, std::uint32_t x, std::uint32_t y)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, wire.memory(heaps.readback, y * row_pitch + x * 4u), sizeof(value));
    return value;
}

void make_triangle(av::D3dLoopback &wire, bool whole_containers)
{
    heaps.upload = wire.open_memory(upload_memory, heap_bytes);
    CHECK(heaps.upload != 0u);
    CHECK(av_d3d12_CreateHeap12(wire, upload_heap, heap_of(heap_upload), 0u, heap_bytes, 0u, heaps.upload, 0u));
    const Vertex triangle[3] = {{-1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
                                {-1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
                                {0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f}};
    std::memcpy(wire.memory(heaps.upload), triangle, sizeof(triangle));
    const float tint[4] = {0.5f, 0.5f, 0.5f, 1.0f};
    std::memcpy(wire.memory(heaps.upload, 256u), tint, sizeof(tint));
    CHECK(av_d3d12_CreatePlacedResource12(wire, vertices, upload_heap, 0u, buffer_of(256u), state_generic_read,
                                           ClearValue{}, upload_address));
    CHECK(av_d3d12_CreatePlacedResource12(wire, constants, upload_heap, 256u, buffer_of(256u), state_generic_read,
                                           ClearValue{}, upload_address + 256u));

    const RootParameter tint_view = {root_cbv, visibility_pixel, 0u, 0u, 0u, 0u, 0u, 0u};
    CHECK(av_d3d12_CreateRootSignature12(wire, root, allow_input_layout, 1u, &tint_view, 0u, nullptr, 0u, nullptr));

    GraphicsState state = {};
    for (RenderTargetBlend &blend : state.targets) {
        blend.src_blend = blend.src_blend_alpha = 2u;
        blend.dest_blend = blend.dest_blend_alpha = 1u;
        blend.blend_op = blend.blend_op_alpha = 1u;
        blend.logic_op = 4u;
        blend.write_mask = 0xfu;
    }
    state.sample_mask = 0xffffffffu;
    state.rasterizer.fill_mode = 3u;
    state.rasterizer.cull_mode = 1u;
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

void draw_triangle(av::D3dLoopback &wire)
{
    bind_triangle(wire);
    CHECK(av_d3d12_DrawInstanced12(wire, list, 3u, 1u, 0u, 0u));
}

void judge_triangle(const av::D3dLoopback &wire)
{
    const std::uint32_t tinted = pixel(wire, 4u, size / 2u);
    CHECK_EQ(tinted & 0xff00ffffu, 0xff000000u);
    CHECK((tinted >> 16u & 0xffu) >= 0x7fu && (tinted >> 16u & 0xffu) <= 0x80u);
    CHECK_EQ(pixel(wire, size - 4u, size / 2u), 0xff0000ffu);
}

}

TEST(d3d12_session_device, a_cleared_target_reads_back_through_guest_memory)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    av::HostShares      *shares = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!host_devices(apis, device11, device12, shares, source)) return;
    av::D3dBackend backend(rig.log);
    av::SessionShares session;
    CHECK(backend.initialize(*shared_as(session, shares, backend), rig.imports, device11, device12));
    av::D3dLoopback wire(backend, rig.imports, source, region_bytes, lent_bytes);
    if (!imports(*device12, wire)) {
        backend.shut_down();
        return;
    }
    make_frame(wire);

    CHECK(av_d3d12_ClearRenderTargetView12(wire, list, DescriptorRef{rtv_heap, 0u}, Float4{{1.0f, 0.0f, 0.0f, 1.0f}},
                                            0u, nullptr));
    read_frame(wire);
    CHECK_EQ(pixel(wire, 0u, 0u), 0xffff0000u);
    CHECK_EQ(pixel(wire, size - 1u, size - 1u), 0xffff0000u);
    CHECK_EQ(wire.refused(), 0u);

    CHECK(av_d3d12_ResetCommandAllocator12(wire, allocator));
    CHECK(av_d3d12_ResetCommandList12(wire, list, allocator, 0u));
    CHECK(av_d3d12_ClearRenderTargetView12(wire, list, DescriptorRef{rtv_heap, 0u}, Float4{{0.0f, 1.0f, 0.0f, 1.0f}},
                                            0u, nullptr));
    read_frame(wire);
    CHECK_EQ(pixel(wire, size / 2u, size / 2u), 0xff00ff00u);

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
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    av::HostShares      *shares = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!host_devices(apis, device11, device12, shares, source)) return;
    for (int whole = 0; whole < 2; whole++) {
        av::D3dBackend backend(rig.log);
        av::SessionShares session;
        CHECK(backend.initialize(*shared_as(session, shares, backend), rig.imports, device11, device12));
        av::D3dLoopback wire(backend, rig.imports, source, region_bytes, lent_bytes);
        if (!imports(*device12, wire)) {
            backend.shut_down();
            return;
        }
        make_frame(wire);
        make_triangle(wire, whole != 0);
        CHECK_EQ(wire.refused(), 0u);
        draw_triangle(wire);
        read_frame(wire);
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
constexpr std::uint32_t draw_bytes = 16u;
constexpr IndirectArgument draw_argument = {0u, 0u, 0u, 0u};

void make_indirect(av::D3dLoopback &wire)
{
    make_frame(wire);
    make_triangle(wire, false);
    const std::uint32_t draw[4] = {3u, 1u, 0u, 0u};
    std::memcpy(wire.memory(heaps.upload, arguments_at), draw, sizeof draw);
    const std::uint32_t one = 1u;
    std::memcpy(wire.memory(heaps.upload, counts_at), &one, sizeof one);
    CHECK(av_d3d12_CreatePlacedResource12(wire, arguments, upload_heap, arguments_at, buffer_of(argument_bytes),
                                           state_generic_read, ClearValue{}, upload_address + arguments_at));
    CHECK(av_d3d12_CreatePlacedResource12(wire, counts, upload_heap, counts_at, buffer_of(argument_bytes),
                                           state_generic_read, ClearValue{}, upload_address + counts_at));
    CHECK(av_d3d12_CreateCommandSignature12(wire, signature, 0u, draw_bytes, 1u, &draw_argument));
}

using Misuse = void (*)(av::D3dLoopback &wire);

constexpr Misuse misuses[] = {
    [](av::D3dLoopback &wire) {
        const IndirectArgument taken[2] = {{5u, 0u, 0u, 1u}, draw_argument};
        CHECK(av_d3d12_CreateCommandSignature12(wire, signature + 1u, root, 20u, 2u, taken));
    },
    [](av::D3dLoopback &wire) {
        const IndirectArgument taken[2] = {{6u, 1u, 0u, 0u}, draw_argument};
        CHECK(av_d3d12_CreateCommandSignature12(wire, signature + 1u, root, 24u, 2u, taken));
    },
    [](av::D3dLoopback &wire) {
        const IndirectArgument taken[2] = {{6u, 0u, 0u, 0u}, draw_argument};
        CHECK(av_d3d12_CreateCommandSignature12(wire, signature + 1u, pipeline, 24u, 2u, taken));
    },
    [](av::D3dLoopback &wire) {
        CHECK(av_d3d12_ExecuteIndirect12(wire, list, signature, argument_bytes / draw_bytes + 1u, arguments, 0u,
                                          0u, 0u));
    },
    [](av::D3dLoopback &wire) {
        CHECK(av_d3d12_ExecuteIndirect12(wire, list, signature, 1u, arguments, 0u, counts, 2u));
    },
    [](av::D3dLoopback &wire) {
        CHECK(av_d3d12_ExecuteIndirect12(wire, list, signature, 1u, arguments, 0u, counts, argument_bytes));
    },
    [](av::D3dLoopback &wire) {
        CHECK(av_d3d12_ExecuteIndirect12(wire, list, signature, 1u, arguments, 0u, pipeline, 0u));
    },
    [](av::D3dLoopback &wire) {
        CHECK(av_d3d12_ExecuteIndirect12(wire, list, signature, 1u, target, 0u, 0u, 0u));
    },
    [](av::D3dLoopback &wire) {
        CHECK(av_d3d12_ExecuteIndirect12(wire, list, signature, 1u, arguments, 0u, target, 0u));
    },
    [](av::D3dLoopback &wire) {
        CHECK(av_d3d12_ExecuteIndirect12(wire, list, pipeline, 1u, arguments, 0u, 0u, 0u));
    },
};

}

TEST(d3d12_session_device, a_triangle_drawn_indirectly_reads_back_where_it_was_drawn)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    av::HostShares      *shares = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!host_devices(apis, device11, device12, shares, source)) return;
    av::D3dBackend backend(rig.log);
    av::SessionShares session;
    CHECK(backend.initialize(*shared_as(session, shares, backend), rig.imports, device11, device12));
    av::D3dLoopback wire(backend, rig.imports, source, region_bytes, lent_bytes);
    if (!imports(*device12, wire)) {
        backend.shut_down();
        return;
    }
    make_indirect(wire);
    bind_triangle(wire);
    CHECK(av_d3d12_ExecuteIndirect12(wire, list, signature, 4u, arguments, 0u, counts, 0u));
    read_frame(wire);
    judge_triangle(wire);
    std::uint32_t removed = 1u;
    CHECK(av_d3d12_Finish12(wire, &removed));
    CHECK_EQ(removed, 0u);
    CHECK_EQ(wire.refused(), 0u);
    CHECK(backend.invariants_hold());
    backend.shut_down();
}

TEST(d3d12_session_device, each_indirect_misuse_removes_a_healthy_device)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    av::HostShares      *shares = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!host_devices(apis, device11, device12, shares, source)) return;
    for (const Misuse misuse : misuses) {
        av::D3dBackend backend(rig.log);
        av::SessionShares session;
        CHECK(backend.initialize(*shared_as(session, shares, backend), rig.imports, device11, device12));
        av::D3dLoopback wire(backend, rig.imports, source, region_bytes, lent_bytes);
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

namespace {

constexpr std::uint32_t named_heap = 40u;

using HeapMisuse = void (*)(av::D3dLoopback &wire);

const HeapMisuse heap_misuses[] = {
    [](av::D3dLoopback &wire) {
        CHECK(av_d3d12_CreateHeap12(wire, named_heap, heap_of(heap_upload), 0u, heap_bytes, 0u, 0x1234u, 0u));
    },
    [](av::D3dLoopback &wire) {
        CHECK(av_d3d12_CreateHeap12(wire, named_heap, heap_of(heap_readback), 0u, heap_bytes, 0u, 0u, 0u));
    },
    [](av::D3dLoopback &wire) {
        CHECK(av_d3d12_CreateHeap12(wire, named_heap, heap_of(heap_readback), 0u, heap_bytes + 4096u, 0u,
                                    heaps.readback, 0u));
    },
    [](av::D3dLoopback &wire) {
        CHECK(av_d3d12_CreateHeap12(wire, named_heap, heap_of(0u), 0u, heap_bytes, 0u, heaps.readback, 0u));
    },
    [](av::D3dLoopback &wire) {
        CHECK(av_d3d12_CreateHeap12(wire, named_heap, heap_of(5u), 0u, heap_bytes, 0u, heaps.readback, 0u));
    },
    [](av::D3dLoopback &wire) {
        CHECK(av_d3d12_CreateHeap12(wire, named_heap, heap_of(heap_readback), 0u, 0u, 0u, heaps.readback, 0u));
    },
    [](av::D3dLoopback &wire) {
        CHECK(av_d3d12_CreateHeap12(wire, named_heap, heap_of(heap_default), 0u, ~std::uint64_t{0}, 0u, 0u, 0u));
    },
    [](av::D3dLoopback &wire) {
        const std::uint64_t gone = wire.open_memory(upload_memory, heap_bytes);
        CHECK(gone != 0u);
        wire.close_memory(gone);
        CHECK(av_d3d12_CreateHeap12(wire, named_heap, heap_of(heap_upload), 0u, heap_bytes, 0u, gone, 0u));
    },
};

}

TEST(d3d12_session_device, each_cpu_visible_heap_over_memory_it_cannot_have_removes_a_healthy_device)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    av::HostShares      *shares = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!host_devices(apis, device11, device12, shares, source)) return;
    for (const HeapMisuse misuse : heap_misuses) {
        av::D3dBackend backend(rig.log);
        av::SessionShares session;
        CHECK(backend.initialize(*shared_as(session, shares, backend), rig.imports, device11, device12));
        av::D3dLoopback wire(backend, rig.imports, source, region_bytes, lent_bytes);
        if (!imports(*device12, wire)) {
            backend.shut_down();
            return;
        }
        make_frame(wire);
        std::uint32_t removed = 1u;
        CHECK(av_d3d12_Finish12(wire, &removed));
        CHECK_EQ(removed, 0u);
        misuse(wire);
        CHECK(av_d3d12_Finish12(wire, &removed));
        CHECK_EQ(removed, driver_internal_error);
        backend.shut_down();
    }
}

TEST(d3d12_session_device, a_heap_over_memory_vkd3d_cannot_open_removes_the_device)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    av::HostShares      *shares = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!host_devices(apis, device11, device12, shares, source)) return;
    av::D3dBackend backend(rig.log);
    av::SessionShares session;
    CHECK(backend.initialize(*shared_as(session, shares, backend), rig.imports, device11, device12));
    av::D3dLoopback wire(backend, rig.imports, source, region_bytes, lent_bytes);
    alignas(4096) static std::uint8_t bytes[heap_bytes];
    av::LocalImport unexported(bytes, sizeof bytes, 99u);
    unexported.memory().bind(*source);
    std::uint64_t unexported_handle = 0u;
    av::ImportedBlob *taken = nullptr;
    CHECK_STATUS(rig.imports.take(99u, unexported.memory(), taken), av::Status::ok);
    CHECK(backend.adopt(unexported.memory(), av::BlobRole(), unexported_handle));
    CHECK_STATUS(rig.imports.name(*taken, unexported_handle), av::Status::ok);
    std::uint32_t hresult = 1u;
    CHECK(av_d3d12_OpenDevice12(wire, 0u, &hresult));
    std::uint32_t removed = 1u;
    CHECK(av_d3d12_Finish12(wire, &removed));
    CHECK_EQ(removed, 0u);
    CHECK(av_d3d12_CreateHeap12(wire, named_heap, heap_of(heap_upload), 0u, heap_bytes, 0u, unexported_handle, 0u));
    CHECK(av_d3d12_Finish12(wire, &removed));
    CHECK_EQ(removed, driver_internal_error);
    backend.abandon(unexported.memory(), unexported_handle);
    CHECK(rig.imports.forget(99u) == &unexported.memory());
    backend.shut_down();
}

TEST(d3d12_session_device, a_heap_is_its_own_memory_and_outlives_the_guest_s_hold_on_it)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    av::HostShares      *shares = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!host_devices(apis, device11, device12, shares, source)) return;
    av::D3dBackend backend(rig.log);
    av::SessionShares session;
    CHECK(backend.initialize(*shared_as(session, shares, backend), rig.imports, device11, device12));
    av::D3dLoopback wire(backend, rig.imports, source, region_bytes, lent_bytes);
    if (!imports(*device12, wire)) {
        backend.shut_down();
        return;
    }
    make_frame(wire);
    const std::uint64_t memory = wire.open_memory(upload_memory, 2u * heap_bytes);
    CHECK(memory != 0u);
    CHECK(av_d3d12_CreateHeap12(wire, upload_heap, heap_of(heap_upload), 0u, heap_bytes, 0u, memory, 0u));
    CHECK(av_d3d12_CreatePlacedResource12(wire, vertices, upload_heap, 0u, buffer_of(256u), state_generic_read,
                                           ClearValue{}, upload_address));
    std::uint32_t words[64];
    for (std::uint32_t at = 0u; at < 64u; at++) words[at] = 0xa000u + at;
    std::memcpy(wire.memory(memory), words, sizeof words);
    wire.close_memory(memory);
    CHECK(wire.memory(memory) == nullptr);
    CHECK(av_d3d12_CopyBufferRegion12(wire, list, readback, 0u, vertices, 0u, sizeof words));
    CHECK(av_d3d12_CloseCommandList12(wire, list));
    const std::uint32_t lists[1] = {list};
    CHECK(av_d3d12_ExecuteCommandLists12(wire, queue, 1u, lists));
    std::uint32_t drained = 1u;
    CHECK(av_d3d12_Finish12(wire, &drained));
    CHECK_EQ(drained, 0u);
    CHECK_EQ(std::memcmp(wire.memory(heaps.readback), words, sizeof words), 0);
    std::uint32_t removed = 1u;
    CHECK(av_d3d12_Finish12(wire, &removed));
    CHECK_EQ(removed, 0u);
    CHECK_EQ(wire.refused(), 0u);
    CHECK(backend.invariants_hold());
    backend.shut_down();
}

TEST(d3d12_session_device, a_sampler_heap_past_2048_is_made_with_2048)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    av::HostShares      *shares = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!host_devices(apis, device11, device12, shares, source)) return;
    constexpr std::uint32_t descriptors_sampler = 1u, shader_visible = 1u, reported = 4000u, made = 2048u;
    const std::uint32_t indices[2] = {made - 1u, made};
    for (const std::uint32_t index : indices) {
        av::D3dBackend backend(rig.log);
        av::SessionShares session;
        CHECK(backend.initialize(*shared_as(session, shares, backend), rig.imports, device11, device12));
        av::D3dLoopback wire(backend, rig.imports, source, region_bytes, lent_bytes);
        std::uint32_t hresult = 1u;
        CHECK(av_d3d12_OpenDevice12(wire, 0u, &hresult));
        CHECK(av_d3d12_CreateDescriptorHeap12(wire, rtv_heap, descriptors_sampler, reported, shader_visible));
        SamplerDesc sampler = {};
        sampler.filter = 0u;
        sampler.address_u = sampler.address_v = sampler.address_w = 1u;
        sampler.comparison = 1u;
        sampler.max_lod = 1000.0f;
        CHECK(av_d3d12_CreateSampler12(wire, DescriptorRef{rtv_heap, index}, sampler));
        std::uint32_t removed = 1u;
        CHECK(av_d3d12_Finish12(wire, &removed));
        CHECK_EQ(removed, index < made ? 0u : driver_internal_error);
        backend.shut_down();
    }
}

TEST(d3d12_session_device, a_buffer_placed_inside_a_spanning_buffer_is_taken_and_a_misplaced_one_refused)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    av::HostShares      *shares = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!host_devices(apis, device11, device12, shares, source)) return;
    constexpr std::uint64_t heap_size = 1u << 20u, piece = 64u * 1024u, base = 0x300000000ull;
    constexpr std::uint32_t heap = 1u, spanning = 2u, inner = 3u, misplaced = 4u;
    av::D3dBackend backend(rig.log);
    av::SessionShares session;
    CHECK(backend.initialize(*shared_as(session, shares, backend), rig.imports, device11, device12));
    av::D3dLoopback wire(backend, rig.imports, source, region_bytes, lent_bytes);
    std::uint32_t hresult = 1u;
    CHECK(av_d3d12_OpenDevice12(wire, 0u, &hresult));
    CHECK(av_d3d12_CreateHeap12(wire, heap, heap_of(heap_default), 0u, heap_size, piece, 0u, 0u));
    CHECK(av_d3d12_CreatePlacedResource12(wire, spanning, heap, 0u, buffer_of(heap_size), 0u, ClearValue{}, base));
    CHECK(av_d3d12_CreatePlacedResource12(wire, inner, heap, piece, buffer_of(piece), 0u, ClearValue{}, base + piece));
    std::uint32_t removed = 1u;
    CHECK(av_d3d12_Finish12(wire, &removed));
    CHECK_EQ(removed, 0u);
    CHECK(av_d3d12_CreatePlacedResource12(wire, misplaced, heap, 2u * piece, buffer_of(piece), 0u, ClearValue{},
                                           base + piece));
    CHECK(av_d3d12_Finish12(wire, &removed));
    CHECK_EQ(removed, driver_internal_error);
    CHECK(backend.invariants_hold());
    backend.shut_down();
}

TEST(d3d12_session_device, a_placed_resource_past_its_heap_of_a_denied_kind_or_an_id_in_use_is_refused)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    av::HostShares      *shares = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!host_devices(apis, device11, device12, shares, source)) return;
    constexpr std::uint64_t heap_size = 1u << 20u, piece = 64u * 1024u, base = 0x300000000ull;
    constexpr std::uint32_t heap = 1u, first = 2u, deny_non_rt_ds_textures = 0x80u;
    ResourceDesc texture = {};
    texture.width = 64u;
    texture.dimension = dimension_texture2d;
    texture.height = 64u;
    texture.depth_or_array = 1u;
    texture.mip_levels = 1u;
    texture.format = format_b8g8r8a8;
    texture.sample_count = 1u;
    for (std::uint32_t refusal = 0u; refusal < 4u; refusal++) {
        av::D3dBackend backend(rig.log);
        av::SessionShares session;
        CHECK(backend.initialize(*shared_as(session, shares, backend), rig.imports, device11, device12));
        av::D3dLoopback wire(backend, rig.imports, source, region_bytes, lent_bytes);
        std::uint32_t hresult = 1u;
        CHECK(av_d3d12_OpenDevice12(wire, 0u, &hresult));
        CHECK(av_d3d12_CreateHeap12(wire, heap, heap_of(heap_default), refusal == 1u ? deny_non_rt_ds_textures : 0u,
                                    heap_size, piece, 0u, 0u));
        CHECK(av_d3d12_CreatePlacedResource12(wire, first, heap, 0u, buffer_of(piece), 0u, ClearValue{}, base));
        std::uint32_t removed = 1u;
        CHECK(av_d3d12_Finish12(wire, &removed));
        CHECK_EQ(removed, 0u);
        if (refusal == 0u)
            CHECK(av_d3d12_CreatePlacedResource12(wire, 3u, heap, heap_size - piece, buffer_of(2u * piece), 0u,
                                                   ClearValue{}, base + heap_size - piece));
        else if (refusal == 1u)
            CHECK(av_d3d12_CreatePlacedResource12(wire, 3u, heap, piece, texture, 0u, ClearValue{}, 0u));
        else if (refusal == 2u)
            CHECK(av_d3d12_CreatePlacedResource12(wire, first, heap, piece, texture, 0u, ClearValue{}, 0u));
        else
            CHECK(av_d3d12_CreatePlacedResource12(wire, first, heap, piece, buffer_of(piece), 0u, ClearValue{},
                                                   base + piece));
        CHECK(av_d3d12_Finish12(wire, &removed));
        CHECK_EQ(removed, driver_internal_error);
        CHECK(backend.invariants_hold());
        backend.shut_down();
    }
}

namespace {

constexpr std::uint32_t heap_flag_shared = 0x1u;
constexpr std::uint64_t shown_key = 0x5eed00000000d312ull;
constexpr std::uint32_t opaque_red = 0xffff0000u;
constexpr std::uint32_t opaque_green = 0xff00ff00u;

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

void draw_cleared(av::D3dLoopback &wire, Float4 colour)
{
    CHECK(av_d3d12_ClearRenderTargetView12(wire, list, DescriptorRef{shown_rtv, 0u}, colour, 0u, nullptr));
    CHECK(av_d3d12_CloseCommandList12(wire, list));
    const std::uint32_t lists[1] = {list};
    CHECK(av_d3d12_ExecuteCommandLists12(wire, queue, 1u, lists));
    CHECK(av_d3d12_ResetCommandAllocator12(wire, allocator));
    CHECK(av_d3d12_ResetCommandList12(wire, list, allocator, 0u));
}

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

bool shown_everywhere(av::D3d11HostDevice &device11, std::uint32_t expected)
{
    std::uint32_t pixels[size * size] = {};
    if (!device11.read_image(1u, shown_key, pixels, row_pitch, size, size)) return false;
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

}

TEST(d3d12_session_device, a_shared_buffer_drawn_on_d3d12_is_what_d3d11_reads_under_its_key)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    av::HostShares      *shares = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!host_devices(apis, device11, device12, shares, source)) return;
    av::D3dBackend backend(rig.log);
    av::SessionShares session;
    CHECK(backend.initialize(*shared_as(session, shares, backend), rig.imports, device11, device12));
    av::D3dLoopback wire(backend, rig.imports, source, region_bytes, lent_bytes);
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
    shares->release(1u, shown_key);
    device11->retire_shares();
    CHECK(!shown_everywhere(*device11, opaque_green));
}

TEST(d3d12_session_device, a_buffer_a_blt_read_is_staged_at_each_present_in_its_queue_s_order)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    av::HostShares      *shares = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!host_devices(apis, device11, device12, shares, source)) return;
    av::HostHeap       surfaces_heap;
    av::SharedSurfaces surfaces(surfaces_heap);
    av::D3dBackend     backend(rig.log);
    av::SessionShares session;
    CHECK(backend.initialize(*shared_as(session, shares, backend), rig.imports, device11, device12, &surfaces));
    av::D3dLoopback wire(backend, rig.imports, source, region_bytes, lent_bytes);
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
    shares->release(1u, shown_key);
    device11->retire_shares();
}

TEST(d3d12_session_device, each_share_or_present_misuse_removes_a_healthy_device)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    av::HostShares      *shares = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!host_devices(apis, device11, device12, shares, source)) return;
    for (const ShowMisuse misuse : show_misuses) {
        av::D3dBackend backend(rig.log);
        av::SessionShares session;
        CHECK(backend.initialize(*shared_as(session, shares, backend), rig.imports, device11, device12));
        av::D3dLoopback wire(backend, rig.imports, source, region_bytes, lent_bytes);
        make_shown(wire);
        std::uint32_t removed = 1u;
        CHECK(av_d3d12_Finish12(wire, &removed));
        CHECK_EQ(removed, 0u);
        misuse(wire);
        CHECK(av_d3d12_Finish12(wire, &removed));
        CHECK_EQ(removed, driver_internal_error);
        backend.shut_down();
        shares->release(1u, shown_key);
    device11->retire_shares();
    }
}
