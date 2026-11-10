#include "fixtures/d3d12/trace_library.hpp"
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
#include "harness/executor/d3d12_answers.hpp"
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

constexpr std::uint32_t heap_default = 1u, heap_upload = 2u, heap_readback = 3u;
constexpr std::uint32_t state_unordered = 0x8u, state_copy_source = 0x800u, state_generic_read = 0xac3u,
                        state_structure = 0x400000u;
constexpr std::uint32_t flag_unordered = 0x4u;
constexpr std::uint32_t dimension_buffer = 1u;
constexpr std::uint32_t layout_row_major = 1u;
constexpr std::uint32_t list_direct = 0u;
constexpr std::uint32_t barrier_uav = 2u;
constexpr std::uint32_t parameter_view_resource = 3u, parameter_view_unordered = 4u;
constexpr std::uint32_t visibility_all = 0u;
constexpr std::uint32_t view_resource = 1u, view_unordered = 2u;
constexpr std::uint32_t format_r32g32b32 = 6u, format_r16_uint = 57u;
constexpr std::uint32_t build_compaction = 2u;
constexpr std::uint32_t structure_top = 0u, structure_bottom = 1u;
constexpr std::uint32_t copy_clone = 0u, copy_compact = 1u;
constexpr std::uint32_t postbuild_compacted = 0u;
constexpr std::uint32_t rays = 8u;
constexpr std::uint32_t instance_id = 5u;

constexpr std::size_t   region_bytes = 8u << 20u;
constexpr std::size_t   lent_bytes = region_bytes;
constexpr std::uint64_t upload_bytes = 256u * 1024u;
constexpr std::uint64_t readback_bytes = 64u * 1024u;
constexpr std::uint64_t readback_address = 0x100000000ull;
constexpr std::uint64_t upload_address = 0x200000000ull;
constexpr std::uint64_t vertex_offset = 0u, index_offset = 0x10000u, instance_offset = 0x20000u, table_offset = 0x30000u;
constexpr std::uint64_t blas_address = 0x300000000ull;
constexpr std::uint64_t blas_scratch_address = 0x300100000ull;
constexpr std::uint64_t tlas_address = 0x300200000ull;
constexpr std::uint64_t tlas_scratch_address = 0x300300000ull;
constexpr std::uint64_t output_address = 0x300400000ull;
constexpr std::uint64_t sizes_address = 0x300500000ull;
constexpr std::uint64_t compact_address = 0x300600000ull;

enum Id : std::uint32_t {
    queue = 1, allocator, list, readback_heap, readback, upload_heap, vertices, indices, instances, table, blas,
    blas_scratch, tlas, tlas_scratch, output, sizes, compact, root, library_state, second_list, second_allocator
};

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
    const char *const wanted = std::getenv("AV_TEST_HOST_GPU");
    if (wanted == nullptr) {
        std::fprintf(stderr, "AV_TEST_HOST_GPU names no GPU: raytracing is judged only on the one it names\n");
        return false;
    }
    if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
    apis.open();
    for (std::uint32_t index = 0u; index < apis.gpu_count(); index++) {
        std::uint8_t candidate[16] = {};
        apis.gpu_uuid(index, candidate);
        char text[33] = {};
        for (std::uint32_t at = 0u; at < 16u; at++) std::snprintf(text + 2u * at, 3u, "%02x", candidate[at]);
        if (std::strcmp(text, wanted) != 0) continue;
        device11 = apis.d3d11_device(index);
        device12 = apis.d3d12_device(index);
        source = apis.memory_source(index);
        if (device11 != nullptr && device12 != nullptr && source != nullptr) return true;
    }
    std::fprintf(stderr, "the GPU AV_TEST_HOST_GPU names does not run vkd3d-proton here: nothing judged\n");
    return false;
}

bool imports(av::D3d12HostDevice &device, av::D3dLoopback &wire)
{
    const std::uint64_t           memory = wire.open_memory(readback_memory, readback_bytes);
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

std::uint64_t aligned(std::uint64_t bytes) { return (bytes + 255u) & ~std::uint64_t{255}; }

struct Rig {
    av::ExecutorLogRig        log;
    av::LoaderHostApis        apis{log.log};
    av::D3d11HostDevice      *device11 = nullptr;
    av::D3d12HostDevice      *device12 = nullptr;
    av::HostMemorySource     *source = nullptr;
    av::D3dBackend            backend{log.log};
    av::SessionShares         unshared;
    av::D3dLoopback          *wire = nullptr;
    Caps12                    caps = {};

    ~Rig()
    {
        delete wire;
        backend.shut_down();
    }

    bool open()
    {
        if (!host_devices(apis, device11, device12, source)) return false;
        CHECK(backend.initialize(unshared, log.imports, device11, device12));
        wire = new av::D3dLoopback(backend, log.imports, source, region_bytes, lent_bytes);
        if (!imports(*device12, *wire)) return false;
        std::uint32_t hresult = 1u;
        CHECK(av_d3d12_OpenDevice12(*wire, 0u, &hresult));
        CHECK_EQ(hresult, 0u);
        std::uint32_t caps_result = 1u;
        av::d3dproto::FormatCaps formats[av::DxgiFormat::count] = {};
        std::uint64_t formats_size = 0u;
        CHECK(av_d3d12_QueryCaps12(*wire, &caps_result, &caps, formats, sizeof formats, &formats_size));
        if (caps.raytracing_tier == 0u) {
            std::fprintf(stderr, "this host runs no raytracing: nothing judged\n");
            return false;
        }
        heap_memory.readback = wire->open_memory(readback_memory, readback_bytes);
        heap_memory.upload = wire->open_memory(upload_memory, upload_bytes);
        CHECK(heap_memory.readback != 0u && heap_memory.upload != 0u);
        CHECK(av_d3d12_CreateHeap12(*wire, readback_heap, heap_of(heap_readback), 0u, readback_bytes, 0u,
                                    heap_memory.readback, 0u));
        CHECK(av_d3d12_CreatePlacedResource12(*wire, readback, readback_heap, 0u, buffer_of(readback_bytes),
                                               0x2u, ClearValue{}, readback_address));
        CHECK(av_d3d12_CreateHeap12(*wire, upload_heap, heap_of(heap_upload), 0u, upload_bytes, 0u,
                                    heap_memory.upload, 0u));
        const std::uint32_t uploads[4] = {vertices, indices, instances, table};
        const std::uint64_t offsets[4] = {vertex_offset, index_offset, instance_offset, table_offset};
        for (std::uint32_t at = 0u; at < 4u; at++)
            CHECK(av_d3d12_CreatePlacedResource12(*wire, uploads[at], upload_heap, offsets[at], buffer_of(0x10000u),
                                                   state_generic_read, ClearValue{}, upload_address + offsets[at]));
        CHECK(av_d3d12_CreateCommandQueue12(*wire, queue, list_direct, 0u, 0u));
        CHECK(av_d3d12_CreateCommandAllocator12(*wire, allocator, list_direct));
        CHECK(av_d3d12_CreateCommandList12(*wire, list, allocator, list_direct, 0u));
        CHECK(av_d3d12_CreateCommandAllocator12(*wire, second_allocator, list_direct));
        CHECK(av_d3d12_CreateCommandList12(*wire, second_list, second_allocator, list_direct, 0u));
        const float triangle[9] = {-1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, -1.0f, 1.0f, 0.0f};
        std::memcpy(wire->memory(heap_memory.upload, vertex_offset), triangle, sizeof triangle);
        const std::uint16_t order[3] = {0u, 1u, 2u};
        std::memcpy(wire->memory(heap_memory.upload, index_offset), order, sizeof order);
        return true;
    }

    RaytracingGeometry geometry() const
    {
        RaytracingGeometry made = {};
        made.index_format = format_r16_uint;
        made.vertex_format = format_r32g32b32;
        made.index_count = 3u;
        made.vertex_count = 3u;
        made.index_buffer = upload_address + index_offset;
        made.start = upload_address + vertex_offset;
        made.stride = 12u;
        return made;
    }

    void structure_buffer(std::uint32_t id, std::uint64_t address, std::uint64_t bytes, std::uint32_t state)
    {
        CHECK(av_d3d12_CreateCommittedResource12(*wire, id, heap_of(heap_default), 0u,
                                                  buffer_of(aligned(bytes), flag_unordered), state, ClearValue{},
                                                  address));
    }

    void prebuild(std::uint32_t type, std::uint32_t flags, std::uint32_t count, const RaytracingGeometry *geometries,
                  std::uint64_t &result, std::uint64_t &scratch)
    {
        std::uint32_t hresult = 1u;
        std::uint64_t update = 0u;
        CHECK(av_d3d12_AccelerationStructurePrebuild12(*wire, type, flags, count, 0u, type == structure_top ? 0u : count,
                                                        geometries, &hresult, &result, &scratch, &update));
        CHECK_EQ(hresult, 0u);
        CHECK(result != 0u && scratch != 0u);
    }

    void uav_barrier(std::uint32_t id, std::uint32_t on)
    {
        const Barrier barrier = {barrier_uav, 0u, on, 0u, 0u, 0u, 0u, 0u};
        CHECK(av_d3d12_ResourceBarrier12(*wire, id, 1u, &barrier));
    }

    void make_instance(std::uint64_t referenced)
    {
        std::uint8_t record[64] = {};
        const float rows[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
        std::memcpy(record, rows, sizeof rows);
        const std::uint32_t id_and_mask = instance_id | (0xffu << 24u);
        std::memcpy(record + 48, &id_and_mask, sizeof id_and_mask);
        std::memcpy(record + 56, &referenced, sizeof referenced);
        std::memcpy(wire->memory(heap_memory.upload, instance_offset), record, sizeof record);
    }

    void execute(std::uint32_t on)
    {
        CHECK(av_d3d12_CloseCommandList12(*wire, on));
        const std::uint32_t lists[1] = {on};
        CHECK(av_d3d12_ExecuteCommandLists12(*wire, queue, 1u, lists));
        std::uint32_t drained = 1u;
        CHECK(av_d3d12_Finish12(*wire, &drained));
    }

    void make_pipeline()
    {
        const RootParameter parameters[2] = {{parameter_view_resource, visibility_all, 0u, 0u, 0u, 0u, 0u, 0u},
                                             {parameter_view_unordered, visibility_all, 0u, 0u, 0u, 0u, 0u, 0u}};
        CHECK(av_d3d12_CreateRootSignature12(*wire, root, 0u, 2u, parameters, 0u, nullptr, 0u, nullptr));
        std::uint8_t names[64] = {};
        std::uint32_t at = 0u;
        auto add = [&](const char *text) {
            const std::uint32_t start = at;
            for (const char *each = text; *each != '\0'; each++, at += 2u) names[at] = static_cast<std::uint8_t>(*each);
            at += 2u;
            return start;
        };
        StateSubobject subobjects[5] = {};
        subobjects[0] = StateSubobject{5u, 0u, 0u, 0u, 0u, static_cast<std::uint32_t>(trace_library::container_bytes), 0u, 0u};
        const std::uint32_t group = add("HG");
        const std::uint32_t closest = add("chit");
        subobjects[1] = StateSubobject{11u, 0u, group, 0u, 0u, 0xFFFFFFFFu, closest, 0xFFFFFFFFu};
        subobjects[2] = StateSubobject{9u, 0u, 0u, 0u, 4u, 8u, 0u, 0u};
        subobjects[3] = StateSubobject{10u, 0u, 0u, 0u, 1u, 0u, 0u, 0u};
        subobjects[4] = StateSubobject{1u, root, 0u, 0u, 0u, 0u, 0u, 0u};
        std::uint32_t hresult = 1u;
        CHECK(av_d3d12_CreateStateObject12(*wire, library_state, 0u, 3u, 5u, subobjects, 0u, nullptr, 0u, nullptr, names,
                                           at, trace_library::container, trace_library::container_bytes, &hresult));
        CHECK_EQ(hresult, 0u);
        const char *const exports[3] = {"raygen", "miss", "HG"};
        const std::uint64_t records[3] = {0u, 64u, 128u};
        for (std::uint32_t each = 0u; each < 3u; each++) {
            std::uint8_t wide[32] = {};
            std::uint64_t length = 0u;
            for (const char *letter = exports[each]; *letter != '\0'; letter++, length += 2u) wide[length] = static_cast<std::uint8_t>(*letter);
            std::uint32_t found = 0u;
            ShaderIdentifier identifier = {};
            CHECK(av_d3d12_StateObjectIdentifier12(*wire, library_state, wide, length, &found, &identifier));
            CHECK_EQ(found, 1u);
            std::memcpy(wire->memory(heap_memory.upload, table_offset + records[each]), identifier.value, sizeof identifier.value);
        }
    }

    void trace(std::uint32_t on, std::uint64_t tlas_view, std::uint64_t table_start)
    {
        CHECK(av_d3d12_SetRootSignature12(*wire, on, 1u, root));
        CHECK(av_d3d12_SetRootView12(*wire, on, 1u, 0u, view_resource, tlas_view));
        CHECK(av_d3d12_SetRootView12(*wire, on, 1u, 1u, view_unordered, output_address));
        CHECK(av_d3d12_SetStateObject12(*wire, on, library_state));
        const RayTable raygen = {table_start, 32u, 0u};
        const RayTable miss = {table_start + 64u, 32u, 32u};
        const RayTable hit = {table_start + 128u, 32u, 32u};
        CHECK(av_d3d12_DispatchRays12(*wire, on, raygen, miss, hit, RayTable{}, rays, rays, 1u));
    }

    void read_output(std::uint32_t on)
    {
        const Barrier to_copy = {0u, 0u, output, 0xffffffffu, state_unordered, state_copy_source, 0u, 0u};
        CHECK(av_d3d12_ResourceBarrier12(*wire, on, 1u, &to_copy));
        CHECK(av_d3d12_CopyBufferRegion12(*wire, on, readback, 0u, output, 0u, rays * rays * 4u));
    }

    std::uint32_t pixel(std::uint32_t x, std::uint32_t y, std::uint64_t offset = 0u) const
    {
        std::uint32_t value = 0u;
        std::memcpy(&value, wire->memory(heap_memory.readback, offset + (y * rays + x) * 4u), sizeof value);
        return value;
    }

    void judge_triangle() const
    {
        for (std::uint32_t y = 0u; y < rays; y++)
            for (std::uint32_t x = 0u; x < rays; x++) {
                if (x + y <= 6u) CHECK_EQ(pixel(x, y), instance_id + 1u);
                if (x + y >= 8u) CHECK_EQ(pixel(x, y), 0u);
            }
    }

    void build_bottom(std::uint32_t on, std::uint32_t flags, std::uint64_t scratch_address, std::uint64_t scratch_bytes,
                      std::uint64_t result_bytes)
    {
        const RaytracingGeometry geometries[1] = {geometry()};
        structure_buffer(blas, blas_address, result_bytes, state_structure);
        structure_buffer(blas_scratch, scratch_address, scratch_bytes, state_unordered);
        CHECK(av_d3d12_BuildAccelerationStructure12(*wire, on, structure_bottom, flags, 1u, 0u, blas_address, 0u,
                                                     scratch_address, 0u, 1u, geometries, 0u, nullptr));
        uav_barrier(on, blas);
    }

    void build_top(std::uint32_t on, std::uint64_t referenced)
    {
        std::uint64_t result = 0u, scratch = 0u;
        prebuild(structure_top, 0u, 1u, nullptr, result, scratch);
        structure_buffer(tlas, tlas_address, result, state_structure);
        structure_buffer(tlas_scratch, tlas_scratch_address, scratch, state_unordered);
        make_instance(referenced);
        CHECK(av_d3d12_BuildAccelerationStructure12(*wire, on, structure_top, 0u, 1u, 0u, tlas_address, 0u,
                                                     tlas_scratch_address, upload_address + instance_offset, 0u, nullptr,
                                                     0u, nullptr));
        uav_barrier(on, tlas);
    }

    void make_output()
    {
        CHECK(av_d3d12_CreateCommittedResource12(*wire, output, heap_of(heap_default), 0u,
                                                  buffer_of(aligned(rays * rays * 4u), flag_unordered), state_unordered,
                                                  ClearValue{}, output_address));
    }

    bool lost_after_refusal()
    {
        std::uint32_t removed = 0u;
        CHECK(av_d3d12_Finish12(*wire, &removed));
        return wire->refused() != 0u || removed != 0u;
    }
};

}

TEST(d3d12_session_raytracing_device, rays_hit_a_triangle_through_a_built_bottom_and_top_level_structure)
{
    Rig rig;
    if (!rig.open()) return;
    std::uint64_t result = 0u, scratch = 0u;
    const RaytracingGeometry geometries[1] = {rig.geometry()};
    rig.prebuild(structure_bottom, 0u, 1u, geometries, result, scratch);
    rig.make_output();
    rig.make_pipeline();
    rig.build_bottom(list, 0u, blas_scratch_address, scratch, result);
    rig.build_top(list, blas_address);
    rig.trace(list, tlas_address, upload_address + table_offset);
    rig.read_output(list);
    std::memset(rig.wire->memory(heap_memory.readback), 0xcd, rays * rays * 4u);
    rig.execute(list);
    rig.judge_triangle();
    CHECK_EQ(rig.wire->refused(), 0u);
    CHECK(rig.backend.invariants_hold());
}

TEST(d3d12_session_raytracing_device, an_instance_that_names_no_buffer_s_structure_is_inactive)
{
    Rig rig;
    if (!rig.open()) return;
    std::uint64_t result = 0u, scratch = 0u;
    const RaytracingGeometry geometries[1] = {rig.geometry()};
    rig.prebuild(structure_bottom, 0u, 1u, geometries, result, scratch);
    rig.make_output();
    rig.make_pipeline();
    rig.build_bottom(list, 0u, blas_scratch_address, scratch, result);
    rig.build_top(list, 0x7000000000ull);
    rig.trace(list, tlas_address, upload_address + table_offset);
    rig.read_output(list);
    std::memset(rig.wire->memory(heap_memory.readback), 0xcd, rays * rays * 4u);
    rig.execute(list);
    for (std::uint32_t y = 0u; y < rays; y++)
        for (std::uint32_t x = 0u; x < rays; x++) CHECK_EQ(rig.pixel(x, y), 0u);
    CHECK_EQ(rig.wire->refused(), 0u);
}

TEST(d3d12_session_raytracing_device, a_compacted_copy_of_a_bottom_level_structure_traces_like_the_original)
{
    Rig rig;
    if (!rig.open()) return;
    std::uint64_t result = 0u, scratch = 0u;
    const RaytracingGeometry geometries[1] = {rig.geometry()};
    rig.prebuild(structure_bottom, build_compaction, 1u, geometries, result, scratch);
    rig.make_output();
    rig.make_pipeline();
    rig.structure_buffer(sizes, sizes_address, 256u, state_unordered);
    rig.structure_buffer(blas, blas_address, result, state_structure);
    rig.structure_buffer(blas_scratch, blas_scratch_address, scratch, state_unordered);
    const RaytracingPostbuild wanted[1] = {{sizes_address, postbuild_compacted, 0u}};
    CHECK(av_d3d12_BuildAccelerationStructure12(*rig.wire, list, structure_bottom, build_compaction, 1u, 0u,
                                                 blas_address, 0u, blas_scratch_address, 0u, 1u, geometries, 1u, wanted));
    rig.uav_barrier(list, blas);
    const Barrier to_copy = {0u, 0u, sizes, 0xffffffffu, state_unordered, state_copy_source, 0u, 0u};
    CHECK(av_d3d12_ResourceBarrier12(*rig.wire, list, 1u, &to_copy));
    CHECK(av_d3d12_CopyBufferRegion12(*rig.wire, list, readback, 0x2000u, sizes, 0u, 8u));
    rig.execute(list);
    CHECK_EQ(rig.wire->refused(), 0u);
    std::uint64_t compacted = 0u;
    std::memcpy(&compacted, rig.wire->memory(heap_memory.readback, 0x2000u), sizeof compacted);
    CHECK(compacted != 0u && compacted <= result);
    rig.structure_buffer(compact, compact_address, compacted, state_structure);
    CHECK(av_d3d12_CopyAccelerationStructure12(*rig.wire, second_list, compact_address, blas_address, copy_compact));
    rig.uav_barrier(second_list, compact);
    rig.build_top(second_list, compact_address);
    rig.trace(second_list, tlas_address, upload_address + table_offset);
    rig.read_output(second_list);
    std::memset(rig.wire->memory(heap_memory.readback), 0xcd, rays * rays * 4u);
    rig.execute(second_list);
    rig.judge_triangle();
    CHECK_EQ(rig.wire->refused(), 0u);
}

TEST(d3d12_session_raytracing_device, a_build_into_an_address_no_buffer_holds_is_refused)
{
    Rig rig;
    if (!rig.open()) return;
    std::uint64_t result = 0u, scratch = 0u;
    const RaytracingGeometry geometries[1] = {rig.geometry()};
    rig.prebuild(structure_bottom, 0u, 1u, geometries, result, scratch);
    rig.structure_buffer(blas_scratch, blas_scratch_address, scratch, state_unordered);
    CHECK(av_d3d12_BuildAccelerationStructure12(*rig.wire, list, structure_bottom, 0u, 1u, 0u, 0x7000000000ull, 0u,
                                                 blas_scratch_address, 0u, 1u, geometries, 0u, nullptr));
    CHECK(rig.lost_after_refusal());
}

TEST(d3d12_session_raytracing_device, a_build_whose_buffers_are_smaller_than_the_hosts_sizes_is_refused)
{
    Rig rig;
    if (!rig.open()) return;
    std::uint64_t result = 0u, scratch = 0u;
    const RaytracingGeometry geometries[1] = {rig.geometry()};
    rig.prebuild(structure_bottom, 0u, 1u, geometries, result, scratch);
    rig.structure_buffer(blas, blas_address, 256u, state_structure);
    rig.structure_buffer(blas_scratch, blas_scratch_address, scratch, state_unordered);
    CHECK(result > 256u);
    CHECK(av_d3d12_BuildAccelerationStructure12(*rig.wire, list, structure_bottom, 0u, 1u, 0u, blas_address, 0u,
                                                 blas_scratch_address, 0u, 1u, geometries, 0u, nullptr));
    CHECK(rig.lost_after_refusal());
}

TEST(d3d12_session_raytracing_device, a_build_reading_vertices_from_an_address_no_buffer_holds_is_refused)
{
    Rig rig;
    if (!rig.open()) return;
    std::uint64_t result = 0u, scratch = 0u;
    RaytracingGeometry geometries[1] = {rig.geometry()};
    rig.prebuild(structure_bottom, 0u, 1u, geometries, result, scratch);
    rig.structure_buffer(blas, blas_address, result, state_structure);
    rig.structure_buffer(blas_scratch, blas_scratch_address, scratch, state_unordered);
    geometries[0].start = 0x7000000000ull;
    CHECK(av_d3d12_BuildAccelerationStructure12(*rig.wire, list, structure_bottom, 0u, 1u, 0u, blas_address, 0u,
                                                 blas_scratch_address, 0u, 1u, geometries, 0u, nullptr));
    CHECK(rig.lost_after_refusal());
}

TEST(d3d12_session_raytracing_device, rays_dispatched_with_a_shader_table_no_buffer_holds_are_refused)
{
    Rig rig;
    if (!rig.open()) return;
    rig.make_output();
    rig.make_pipeline();
    CHECK(av_d3d12_SetRootSignature12(*rig.wire, list, 1u, root));
    CHECK(av_d3d12_SetStateObject12(*rig.wire, list, library_state));
    const RayTable raygen = {0x7000000000ull, 32u, 0u};
    CHECK(av_d3d12_DispatchRays12(*rig.wire, list, raygen, RayTable{}, RayTable{}, RayTable{}, 1u, 1u, 1u));
    CHECK(rig.lost_after_refusal());
}

TEST(d3d12_session_raytracing_device, rays_dispatched_with_a_misaligned_or_oversized_table_are_refused)
{
    Rig rig;
    if (!rig.open()) return;
    rig.make_output();
    rig.make_pipeline();
    CHECK(av_d3d12_SetRootSignature12(*rig.wire, list, 1u, root));
    CHECK(av_d3d12_SetStateObject12(*rig.wire, list, library_state));
    const RayTable raygen = {upload_address + table_offset + 32u, 32u, 0u};
    CHECK(av_d3d12_DispatchRays12(*rig.wire, list, raygen, RayTable{}, RayTable{}, RayTable{}, 1u, 1u, 1u));
    CHECK(rig.lost_after_refusal());
}

TEST(d3d12_session_raytracing_device, a_copy_of_a_structure_that_was_never_built_is_refused)
{
    Rig rig;
    if (!rig.open()) return;
    rig.structure_buffer(blas, blas_address, 4096u, state_structure);
    rig.structure_buffer(compact, compact_address, 4096u, state_structure);
    CHECK(av_d3d12_CopyAccelerationStructure12(*rig.wire, list, compact_address, blas_address, copy_clone));
    CHECK(rig.lost_after_refusal());
}

TEST(d3d12_session_raytracing_device, a_state_object_naming_a_root_signature_that_does_not_exist_is_refused)
{
    Rig rig;
    if (!rig.open()) return;
    StateSubobject subobjects[1] = {StateSubobject{1u, 99u, 0u, 0u, 0u, 0u, 0u, 0u}};
    std::uint32_t hresult = 0u;
    const bool answered = av_d3d12_CreateStateObject12(*rig.wire, library_state, 0u, 3u, 1u, subobjects, 0u, nullptr, 0u,
                                                        nullptr, nullptr, 0u, nullptr, 0u, &hresult);
    CHECK(!answered || hresult != 0u);
    CHECK(rig.lost_after_refusal());
}

TEST(d3d12_session_raytracing_device, a_prebuild_of_a_shape_the_host_cannot_read_is_refused)
{
    Rig rig;
    if (!rig.open()) return;
    RaytracingGeometry geometries[1] = {rig.geometry()};
    geometries[0].vertex_format = 28u;
    std::uint32_t hresult = 0u;
    std::uint64_t result = 0u, scratch = 0u, update = 0u;
    const bool answered = av_d3d12_AccelerationStructurePrebuild12(*rig.wire, structure_bottom, 0u, 1u, 0u, 1u, geometries,
                                                                    &hresult, &result, &scratch, &update);
    CHECK(!answered || hresult != 0u);
    CHECK(rig.lost_after_refusal());
}
