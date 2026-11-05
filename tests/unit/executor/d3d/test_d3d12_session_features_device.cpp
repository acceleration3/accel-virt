#include "fixtures/d3d12/fill_cs.hpp"
#include "fixtures/d3d12/textured_ps.hpp"
#include "fixtures/d3d12/textured_vs.hpp"
#include "executor/sharing/session_shares.hpp"
#include "executor/d3d/d3d_backend.hpp"
#include "executor/host_memory_source.hpp"
#include "executor/owned_fd.hpp"
#include "proto/d3d/dxgi_format.hpp"
#include "executor/source_memory.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/d3d12_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "harness/executor/d3d_loopback.hpp"
#include "harness/executor/executor_log_rig.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/d3d12/d3d12_dispatch_client.h"
#include "proto/resources/host_memory_request.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace {

using namespace av::d3d12proto;

constexpr std::uint32_t format_b8g8r8a8 = 87u;
constexpr std::uint32_t size = 64u;
constexpr std::uint32_t row_pitch = size * 4u;
constexpr std::uint32_t texels = 4u;
constexpr std::uint32_t texel_pitch = 256u;
constexpr std::uint32_t heap_default = 1u, heap_upload = 2u, heap_readback = 3u;
constexpr std::uint32_t state_render_target = 0x4u, state_unordered = 0x8u,
                        state_pixel_resource = 0x80u, state_copy_dest = 0x400u, state_copy_source = 0x800u,
                        state_resolve_dest = 0x1000u, state_resolve_source = 0x2000u, state_generic_read = 0xac3u;
constexpr std::uint32_t flag_render_target = 0x1u, flag_unordered = 0x4u;
constexpr std::uint32_t descriptors_view = 0u, descriptors_rtv = 2u;
constexpr std::uint32_t heap_shader_visible = 0x1u;
constexpr std::uint32_t dimension_buffer = 1u, dimension_texture2d = 3u;
constexpr std::uint32_t layout_row_major = 1u;
constexpr std::uint32_t view_texture2d = 4u, view_buffer = 1u;
constexpr std::uint32_t default_mapping = 0x1688u;
constexpr std::uint32_t parameter_table = 0u, parameter_constants = 1u;
constexpr std::uint32_t range_srv = 0u, range_uav = 1u;
constexpr std::uint32_t visibility_all = 0u, visibility_pixel = 5u;
constexpr std::uint32_t query_occlusion = 0u;
constexpr std::uint32_t list_direct = 0u, list_compute = 2u;
constexpr std::uint32_t samples = 4u;
constexpr std::uint32_t fill_base = 1000u;
constexpr std::uint32_t fill_count = 256u;

constexpr std::size_t   region_bytes = 8u << 20u;
constexpr std::size_t   lent_bytes = region_bytes;
constexpr std::uint64_t heap_bytes = 64u * 1024u;
constexpr std::uint64_t query_offset = 32u * 1024u;
constexpr std::uint64_t readback_address = 0x100000000ull;
constexpr std::uint64_t upload_address = 0x200000000ull;
constexpr std::uint64_t buffer_address = 0x300000000ull;

enum Id : std::uint32_t {
    queue = 1, allocator, list, target, rtv_heap, readback_heap, readback, upload_heap, staging, texture,
    view_heap, root, pipeline, queries, compute_queue, compute_allocator, compute_list, written, multisampled
};

constexpr std::uint32_t quadrant[4] = {0xffff0000u, 0xff00ff00u, 0xff0000ffu, 0xffffffffu};

const av::HostMemoryRequest upload_memory(av::HostMemoryRequest::Kind::upload, 0u);
const av::HostMemoryRequest readback_memory(av::HostMemoryRequest::Kind::readback, 0u);

struct HeapMemory {
    std::uint64_t readback;
    std::uint64_t upload;
};
HeapMemory heap_memory = {};

bool host_devices(av::LoaderHostApis &apis, av::D3d11HostDevice *&device11, av::D3d12HostDevice *&device12,
                  av::HostMemorySource *&source)
{
    if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
    apis.open();
    for (std::uint32_t index = 0u; index < apis.gpu_count(); index++) {
        device11 = apis.d3d11_device(index);
        device12 = apis.d3d12_device(index);
        source = apis.memory_source(index);
        if (device11 != nullptr && device12 != nullptr && source != nullptr) return true;
    }
    std::fprintf(stderr, "no GPU here runs vkd3d-proton: nothing judged\n");
    return false;
}

bool imports(av::D3d12HostDevice &device, av::D3dLoopback &wire)
{
    const std::uint64_t           memory = wire.open_memory(readback_memory, heap_bytes);
    const av::SourceMemory *const held = wire.source_memory(memory);
    av::OwnedFd                   shared;
    const bool                    exported = held != nullptr && held->share(shared);
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

HeapProperties heap_of(std::uint32_t type) { return HeapProperties{type, 0u, 0u, 0u}; }

ResourceDesc buffer_of(std::uint64_t bytes, std::uint32_t flags = 0u)
{
    ResourceDesc desc = {};
    desc.width = bytes;
    desc.dimension = dimension_buffer;
    desc.height = 1u;
    desc.depth_or_array = 1u;
    desc.mip_levels = 1u;
    desc.sample_count = 1u;
    desc.layout = layout_row_major;
    desc.flags = flags;
    return desc;
}

ResourceDesc texture_of(std::uint32_t width, std::uint32_t flags, std::uint32_t sample_count = 1u)
{
    ResourceDesc desc = {};
    desc.width = width;
    desc.dimension = dimension_texture2d;
    desc.height = width;
    desc.depth_or_array = 1u;
    desc.mip_levels = 1u;
    desc.format = format_b8g8r8a8;
    desc.sample_count = sample_count;
    desc.flags = flags;
    return desc;
}

void open_list(av::D3dLoopback &wire, std::uint32_t type, std::uint32_t queue_id, std::uint32_t allocator_id,
               std::uint32_t list_id)
{
    CHECK(av_d3d12_CreateCommandQueue12(wire, queue_id, type, 0u, 0u));
    CHECK(av_d3d12_CreateCommandAllocator12(wire, allocator_id, type));
    CHECK(av_d3d12_CreateCommandList12(wire, list_id, allocator_id, type, 0u));
}

void make_device(av::D3dLoopback &wire)
{
    std::uint32_t hresult = 1u;
    CHECK(av_d3d12_OpenDevice12(wire, 0u, &hresult));
    CHECK_EQ(hresult, 0u);
    heap_memory.readback = wire.open_memory(readback_memory, heap_bytes);
    CHECK(heap_memory.readback != 0u);
    CHECK(av_d3d12_CreateHeap12(wire, readback_heap, heap_of(heap_readback), 0u, heap_bytes, 0u,
                                heap_memory.readback, 0u));
    CHECK(av_d3d12_CreatePlacedResource12(wire, readback, readback_heap, 0u, buffer_of(query_offset), state_copy_dest,
                                           ClearValue{}, readback_address));
}

void make_target(av::D3dLoopback &wire)
{
    CHECK(av_d3d12_CreateCommittedResource12(wire, target, heap_of(heap_default), 0u,
                                              texture_of(size, flag_render_target), state_render_target, ClearValue{},
                                              0u));
    CHECK(av_d3d12_CreateDescriptorHeap12(wire, rtv_heap, descriptors_rtv, 1u, 0u));
    ViewDesc view = {};
    view.resource = target;
    CHECK(av_d3d12_CreateRenderTargetView12(wire, DescriptorRef{rtv_heap, 0u}, 0u, view));
}

void execute(av::D3dLoopback &wire, std::uint32_t queue_id, std::uint32_t list_id)
{
    CHECK(av_d3d12_CloseCommandList12(wire, list_id));
    const std::uint32_t lists[1] = {list_id};
    CHECK(av_d3d12_ExecuteCommandLists12(wire, queue_id, 1u, lists));
    std::uint32_t drained = 1u;
    CHECK(av_d3d12_Finish12(wire, &drained));
    CHECK_EQ(drained, 0u);
}

void copy_frame(av::D3dLoopback &wire)
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
}

std::uint32_t word_at(const av::D3dLoopback &wire, std::uint64_t offset)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, wire.memory(heap_memory.readback, offset), sizeof(value));
    return value;
}

std::uint32_t pixel(const av::D3dLoopback &wire, std::uint32_t x, std::uint32_t y)
{
    return word_at(wire, std::uint64_t{y} * row_pitch + x * 4u);
}

void upload_texture(av::D3dLoopback &wire)
{
    heap_memory.upload = wire.open_memory(upload_memory, heap_bytes);
    CHECK(heap_memory.upload != 0u);
    CHECK(av_d3d12_CreateHeap12(wire, upload_heap, heap_of(heap_upload), 0u, heap_bytes, 0u, heap_memory.upload, 0u));
    for (std::uint32_t y = 0u; y < texels; y++)
        for (std::uint32_t x = 0u; x < texels; x++) {
            const std::uint32_t colour = quadrant[(y / 2u) * 2u + x / 2u];
            std::memcpy(wire.memory(heap_memory.upload, y * texel_pitch + x * 4u), &colour, sizeof(colour));
        }
    CHECK(av_d3d12_CreatePlacedResource12(wire, staging, upload_heap, 0u, buffer_of(texel_pitch * texels),
                                           state_generic_read, ClearValue{}, upload_address));
    CHECK(av_d3d12_CreateCommittedResource12(wire, texture, heap_of(heap_default), 0u, texture_of(texels, 0u),
                                              state_copy_dest, ClearValue{}, 0u));
    CopyLocation into = {};
    into.resource = texture;
    CopyLocation from = {};
    from.resource = staging;
    from.type = 1u;
    from.format = format_b8g8r8a8;
    from.width = texels;
    from.height = texels;
    from.depth = 1u;
    from.row_pitch = texel_pitch;
    CHECK(av_d3d12_CopyTextureRegion12(wire, list, into, 0u, 0u, 0u, from, 0u, Box{}));
    const Barrier to_read = {0u, 0u, texture, 0xffffffffu, state_copy_dest, state_pixel_resource, 0u, 0u};
    CHECK(av_d3d12_ResourceBarrier12(wire, list, 1u, &to_read));
    CHECK(av_d3d12_CreateDescriptorHeap12(wire, view_heap, descriptors_view, 2u, heap_shader_visible));
    ViewDesc view = {};
    view.resource = texture;
    view.format = format_b8g8r8a8;
    view.dimension = view_texture2d;
    view.mapping = default_mapping;
    view.args[1] = 1u;
    CHECK(av_d3d12_CreateShaderResourceView12(wire, DescriptorRef{view_heap, 1u}, 1u, view));
}

void make_textured_pipeline(av::D3dLoopback &wire, std::uint32_t sample_count)
{
    const DescriptorRange range = {range_srv, 1u, 0u, 0u, 0u, 0u};
    const RootParameter   table = {parameter_table, visibility_pixel, 0u, 0u, 0u, 0u, 1u, 0u};
    StaticSampler         sampler = {};
    sampler.address_u = sampler.address_v = sampler.address_w = 3u;
    sampler.comparison = 1u;
    sampler.max_lod = 1000.0f;
    sampler.visibility = visibility_pixel;
    CHECK(av_d3d12_CreateRootSignature12(wire, root, 0u, 1u, &table, 1u, &range, 1u, &sampler));
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
    state.topology_type = 3u;
    state.render_targets = 1u;
    state.rtv_formats[0] = format_b8g8r8a8;
    state.sample_count = sample_count;
    SignatureEntry signatures[8] = {};
    std::uint32_t  count = 0u;
    for (const SignatureEntry &entry : textured_vs::signatures) signatures[count++] = entry;
    for (const SignatureEntry &entry : textured_ps::signatures) signatures[count++] = entry;
    CHECK(av_d3d12_CreateGraphicsPipeline12(wire, pipeline, root, state, 0u, nullptr, count, signatures,
                                             textured_vs::program, textured_vs::program_bytes, nullptr, 0u, nullptr,
                                             0u, nullptr, 0u, textured_ps::program, textured_ps::program_bytes));
}

}

TEST(d3d12_session_features_device, a_texture_from_guest_memory_is_sampled_through_a_table_and_counted)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!host_devices(apis, device11, device12, source)) return;
    av::D3dBackend backend(rig.log);
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, rig.imports, device11, device12));
    av::D3dLoopback wire(backend, rig.imports, source, region_bytes, lent_bytes);
    if (!imports(*device12, wire)) {
        backend.shut_down();
        return;
    }
    make_device(wire);
    open_list(wire, list_direct, queue, allocator, list);
    make_target(wire);
    upload_texture(wire);
    make_textured_pipeline(wire, 1u);
    CHECK(av_d3d12_CreateQueryHeap12(wire, queries, query_occlusion, 1u));
    CHECK_EQ(wire.refused(), 0u);

    const DescriptorRef targets[1] = {{rtv_heap, 0u}};
    CHECK(av_d3d12_SetRenderTargets12(wire, list, 1u, targets, 0u, DescriptorRef{}));
    const Viewport viewport = {0.0f, 0.0f, float(size), float(size), 0.0f, 1.0f};
    CHECK(av_d3d12_SetViewports12(wire, list, 1u, &viewport));
    const Rect scissor = {0, 0, int(size), int(size)};
    CHECK(av_d3d12_SetScissorRects12(wire, list, 1u, &scissor));
    const std::uint32_t heaps[1] = {view_heap};
    CHECK(av_d3d12_SetDescriptorHeaps12(wire, list, 1u, heaps));
    CHECK(av_d3d12_SetRootSignature12(wire, list, 0u, root));
    CHECK(av_d3d12_SetPipelineState12(wire, list, pipeline));
    CHECK(av_d3d12_SetRootDescriptorTable12(wire, list, 0u, 0u, DescriptorRef{view_heap, 1u}));
    CHECK(av_d3d12_SetTopology12(wire, list, 4u));
    CHECK(av_d3d12_BeginQuery12(wire, list, queries, query_occlusion, 0u));
    CHECK(av_d3d12_DrawInstanced12(wire, list, 3u, 1u, 0u, 0u));
    CHECK(av_d3d12_EndQuery12(wire, list, queries, query_occlusion, 0u));
    copy_frame(wire);
    CHECK(av_d3d12_ResolveQueryData12(wire, list, queries, query_occlusion, 0u, 1u, readback, query_offset - 8u));
    std::memset(wire.memory(heap_memory.readback), 0xcd, query_offset);
    execute(wire, queue, list);

    CHECK_EQ(pixel(wire, 8u, 8u), quadrant[0]);
    CHECK_EQ(pixel(wire, size - 8u, 8u), quadrant[1]);
    CHECK_EQ(pixel(wire, 8u, size - 8u), quadrant[2]);
    CHECK_EQ(pixel(wire, size - 8u, size - 8u), quadrant[3]);
    std::uint64_t passed = 0u;
    std::memcpy(&passed, wire.memory(heap_memory.readback, query_offset - 8u), sizeof(passed));
    CHECK_EQ(passed, std::uint64_t{size} * size);
    std::uint32_t removed = 1u;
    CHECK(av_d3d12_Finish12(wire, &removed));
    CHECK_EQ(removed, 0u);
    CHECK_EQ(wire.refused(), 0u);
    CHECK(backend.invariants_hold());
    backend.shut_down();
}

TEST(d3d12_session_features_device, a_compute_dispatch_writes_through_a_view_with_root_constants)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!host_devices(apis, device11, device12, source)) return;
    av::D3dBackend backend(rig.log);
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, rig.imports, device11, device12));
    av::D3dLoopback wire(backend, rig.imports, source, region_bytes, lent_bytes);
    if (!imports(*device12, wire)) {
        backend.shut_down();
        return;
    }
    make_device(wire);
    open_list(wire, list_compute, compute_queue, compute_allocator, compute_list);
    CHECK(av_d3d12_CreateCommittedResource12(wire, written, heap_of(heap_default), 0u,
                                              buffer_of(fill_count * 4u, flag_unordered), state_unordered,
                                              ClearValue{}, buffer_address));
    CHECK(av_d3d12_CreateDescriptorHeap12(wire, view_heap, descriptors_view, 1u, heap_shader_visible));
    ViewDesc view = {};
    view.resource = written;
    view.dimension = view_buffer;
    view.args[0] = fill_count;
    view.args[1] = 4u;
    CHECK(av_d3d12_CreateUnorderedAccessView12(wire, DescriptorRef{view_heap, 0u}, 1u, view));
    const DescriptorRange range = {range_uav, 1u, 0u, 0u, 0u, 0u};
    const RootParameter   parameters[2] = {{parameter_constants, visibility_all, 0u, 0u, 1u, 0u, 0u, 0u},
                                           {parameter_table, visibility_all, 0u, 0u, 0u, 0u, 1u, 0u}};
    CHECK(av_d3d12_CreateRootSignature12(wire, root, 0u, 2u, parameters, 1u, &range, 0u, nullptr));
    CHECK(av_d3d12_CreateComputePipeline12(wire, pipeline, root, fill_cs::program, fill_cs::program_bytes));

    const std::uint32_t heaps[1] = {view_heap};
    CHECK(av_d3d12_SetDescriptorHeaps12(wire, compute_list, 1u, heaps));
    CHECK(av_d3d12_SetRootSignature12(wire, compute_list, 1u, root));
    CHECK(av_d3d12_SetPipelineState12(wire, compute_list, pipeline));
    const std::uint32_t base = fill_base;
    CHECK(av_d3d12_SetRootConstants12(wire, compute_list, 1u, 0u, 0u, 1u, &base));
    CHECK(av_d3d12_SetRootDescriptorTable12(wire, compute_list, 1u, 1u, DescriptorRef{view_heap, 0u}));
    CHECK(av_d3d12_Dispatch12(wire, compute_list, fill_count / 64u, 1u, 1u));
    const Barrier to_copy = {0u, 0u, written, 0xffffffffu, state_unordered, state_copy_source, 0u, 0u};
    CHECK(av_d3d12_ResourceBarrier12(wire, compute_list, 1u, &to_copy));
    CHECK(av_d3d12_CopyBufferRegion12(wire, compute_list, readback, 0u, written, 0u, fill_count * 4u));
    std::memset(wire.memory(heap_memory.readback), 0xcd, fill_count * 4u);
    execute(wire, compute_queue, compute_list);

    const std::uint32_t judged[] = {0u, 1u, 63u, 64u, 255u};
    for (const std::uint32_t index : judged) CHECK_EQ(word_at(wire, index * 4u), fill_base + index * 3u);
    std::uint32_t removed = 1u;
    CHECK(av_d3d12_Finish12(wire, &removed));
    CHECK_EQ(removed, 0u);
    CHECK_EQ(wire.refused(), 0u);
    CHECK(backend.invariants_hold());
    backend.shut_down();
}

TEST(d3d12_session_features_device, a_multisampled_target_resolves_into_a_single_sampled_one)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice *device11 = nullptr;
    av::D3d12HostDevice *device12 = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!host_devices(apis, device11, device12, source)) return;
    av::D3dBackend backend(rig.log);
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, rig.imports, device11, device12));
    av::D3dLoopback wire(backend, rig.imports, source, region_bytes, lent_bytes);
    if (!imports(*device12, wire)) {
        backend.shut_down();
        return;
    }
    make_device(wire);
    open_list(wire, list_direct, queue, allocator, list);
    make_target(wire);
    Caps12        caps = {};
    std::uint32_t caps_result = 1u;
    av::d3dproto::FormatCaps formats[av::DxgiFormat::count] = {};
    std::uint64_t formats_size = 0u;
    CHECK(av_d3d12_QueryCaps12(wire, &caps_result, &caps, formats, sizeof formats, &formats_size));
    CHECK_EQ(caps_result, 0u);
    CHECK_EQ(formats_size, sizeof formats);
    constexpr std::uint32_t four_samples = 1u;
    static_assert(samples == 2u << four_samples, "the quality levels of four samples sit second in a row");
    if (formats[format_b8g8r8a8].quality_levels[four_samples] == 0u) {
        std::fprintf(stderr, "no 4x multisampling of B8G8R8A8 here: nothing judged\n");
        backend.shut_down();
        return;
    }
    CHECK(av_d3d12_CreateCommittedResource12(wire, multisampled, heap_of(heap_default), 0u,
                                              texture_of(size, flag_render_target, samples), state_render_target,
                                              ClearValue{}, 0u));
    CHECK(av_d3d12_CreateDescriptorHeap12(wire, view_heap, descriptors_rtv, 1u, 0u));
    ViewDesc view = {};
    view.resource = multisampled;
    view.format = format_b8g8r8a8;
    view.dimension = 6u;
    CHECK(av_d3d12_CreateRenderTargetView12(wire, DescriptorRef{view_heap, 0u}, 1u, view));
    CHECK(av_d3d12_ClearRenderTargetView12(wire, list, DescriptorRef{view_heap, 0u}, Float4{{1.0f, 0.0f, 1.0f, 1.0f}},
                                            0u, nullptr));
    const Barrier to_resolve[2] = {
        {0u, 0u, multisampled, 0xffffffffu, state_render_target, state_resolve_source, 0u, 0u},
        {0u, 0u, target, 0xffffffffu, state_render_target, state_resolve_dest, 0u, 0u}};
    CHECK(av_d3d12_ResourceBarrier12(wire, list, 2u, to_resolve));
    CHECK(av_d3d12_ResolveSubresource12(wire, list, target, 0u, multisampled, 0u, format_b8g8r8a8));
    const Barrier back = {0u, 0u, target, 0xffffffffu, state_resolve_dest, state_render_target, 0u, 0u};
    CHECK(av_d3d12_ResourceBarrier12(wire, list, 1u, &back));
    copy_frame(wire);
    execute(wire, queue, list);
    CHECK_EQ(pixel(wire, 0u, 0u), 0xffff00ffu);
    CHECK_EQ(pixel(wire, size - 1u, size - 1u), 0xffff00ffu);
    std::uint32_t removed = 1u;
    CHECK(av_d3d12_Finish12(wire, &removed));
    CHECK_EQ(removed, 0u);
    CHECK_EQ(wire.refused(), 0u);
    backend.shut_down();
}
