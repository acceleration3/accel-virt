#include "core/support/object_storage.hpp"
#include "executor/d3d/d3d_backend.hpp"
#include "executor/host_memory_source.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "executor/sharing/session_shares.hpp"
#include "fixtures/d3d/triangle_ps.hpp"
#include "fixtures/d3d/triangle_vs.hpp"
#include "executor/import_table.hpp"
#include "executor/imported_blob.hpp"
#include "harness/executor/d3d_loopback.hpp"
#include "proto/d3d/buffer_slice_support.hpp"
#include "proto/submission/blob_role.hpp"
#include "harness/executor/executor_log_rig.hpp"
#include "harness/executor/local_import.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "proto/resources/host_memory_request.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace av::d3dproto;

namespace {

constexpr std::uint64_t region_bytes = std::uint64_t{1} << 20u;
constexpr std::uint64_t lent_bytes = region_bytes / 2u;
constexpr std::uint64_t store_bytes = std::uint64_t{1} << 20u;
constexpr std::uint32_t buffer_bytes = 4096u;
constexpr std::uint64_t first_slice = 64u * 1024u;
constexpr std::uint64_t second_slice = 128u * 1024u;
constexpr std::uint32_t usage_default = 0u, usage_dynamic = 2u;
constexpr std::uint32_t bind_vertex = 0x1u;
constexpr std::uint32_t cpu_write = 0x10000u;
constexpr std::uint32_t copy_no_overwrite = 0x1u;
constexpr std::uint32_t driver_internal_error = 0x887A0020u;

const av::HostMemoryRequest upload(av::HostMemoryRequest::Kind::upload, 0u);

bool slice_host(av::LoaderHostApis &apis, av::D3d11HostDevice *&device, av::HostMemorySource *&source)
{
    if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
    apis.open();
    for (std::uint32_t index = 0u; index < apis.gpu_count(); index++) {
        device = apis.d3d11_device(index);
        source = apis.memory_source(index);
        if (device != nullptr && source != nullptr && device->slices()) return true;
    }
    std::fprintf(stderr, "no DXVK here renames a buffer to imported storage: nothing judged\n");
    return false;
}

av::ObjectStorage<av::D3dBackend> backend_storage;

class SliceSession {
public:
    SliceSession(av::ExecutorLogRig &rig, av::D3d11HostDevice &device, av::HostMemorySource &source)
        : made_(backend_storage.construct(rig.log)),
          wire_(backend_storage.object(), rig.imports, &source, region_bytes, lent_bytes)
    {
        CHECK(backend().initialize(unshared_, rig.imports, &device));
        std::uint32_t hresult = 1u, level = 0u, supported = 0u;
        CHECK(av_d3d_QueryBufferSlices11(wire_, &supported));
        CHECK_EQ(supported, static_cast<std::uint32_t>(av::BufferSliceSupport::multi_block));
        CHECK(av_d3d_CreateDevice11(wire_, 1u, 0u, 0u, &hresult, &level));
        CHECK_EQ(hresult, 0u);
    }
    ~SliceSession() { backend().shut_down(); }
    SliceSession(const SliceSession &) = delete;
    SliceSession &operator=(const SliceSession &) = delete;

    av::D3dBackend  &backend() { return backend_storage.object(); }
    av::D3dLoopback &wire() { return wire_; }
    std::uint32_t    finish()
    {
        std::uint32_t hresult = 1u;
        CHECK(av_d3d_Finish11(wire_, &hresult));
        return hresult;
    }

private:
    class Made {
    public:
        explicit Made(av::Status status) { CHECK_STATUS(status, av::Status::ok); }
        ~Made() { backend_storage.destroy(); }
        Made(const Made &) = delete;
        Made &operator=(const Made &) = delete;
    };

    Made              made_;
    av::SessionShares unshared_;
    av::D3dLoopback   wire_;
};

ResourceDesc buffer_desc(std::uint32_t usage, std::uint32_t bind, std::uint32_t cpu)
{
    ResourceDesc desc = {};
    desc.dimension = 1u;
    desc.width = buffer_bytes;
    desc.usage = usage;
    desc.bind_flags = bind;
    desc.cpu_access = cpu;
    return desc;
}

void fill(std::uint8_t *at, std::uint32_t seed)
{
    for (std::uint32_t word = 0u; word < buffer_bytes / 4u; word++) {
        const std::uint32_t value = seed + word;
        std::memcpy(at + std::uint64_t{word} * 4u, &value, sizeof value);
    }
}

bool holds(av::D3dLoopback &wire, std::uint32_t resource, std::uint32_t seed)
{
    static std::uint32_t words[buffer_bytes / 4u];
    std::uint32_t hresult = 1u, row_pitch = 0u, depth_pitch = 0u;
    std::uint64_t read = 0u;
    if (!av_d3d_ReadSubresource(wire, resource, 0u, &hresult, &row_pitch, &depth_pitch,
                                reinterpret_cast<std::uint8_t *>(words), sizeof words, &read) ||
        hresult != 0u || read != sizeof words)
        return false;
    for (std::uint32_t at = 0u; at < buffer_bytes / 4u; at++)
        if (words[at] != seed + at) return false;
    return true;
}

}

TEST(buffer_slice_device, a_buffer_on_a_slice_reads_what_the_guest_wrote_when_the_gpu_runs)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice  *device = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!slice_host(apis, device, source)) return;
    SliceSession     session(rig, *device, *source);
    av::D3dLoopback &wire = session.wire();
    const std::uint64_t store = wire.open_memory(upload, store_bytes);
    CHECK(store != 0u);

    constexpr std::uint32_t mapped = 10u, early = 11u, late = 12u;
    CHECK(av_d3d_CreateResource(wire, mapped, buffer_desc(usage_dynamic, bind_vertex, cpu_write), 0u, 0u, 0u));
    CHECK(av_d3d_CreateResource(wire, early, buffer_desc(usage_default, 0u, 0u), 0u, 0u, 0u));
    CHECK(av_d3d_CreateResource(wire, late, buffer_desc(usage_default, 0u, 0u), 0u, 0u, 0u));

    fill(wire.memory(store, first_slice), 1000u);
    CHECK(av_d3d_BindBufferSlice(wire, mapped, store, first_slice, buffer_bytes));
    CHECK(av_d3d_CopyResource(wire, early, mapped));
    fill(wire.memory(store, first_slice), 5000u);
    fill(wire.memory(store, second_slice), 9000u);
    CHECK(av_d3d_BindBufferSlice(wire, mapped, store, second_slice, buffer_bytes));
    CHECK(av_d3d_CopyResource(wire, late, mapped));
    CHECK_EQ(session.finish(), 0u);
    CHECK(holds(wire, early, 5000u));
    CHECK(holds(wire, late, 9000u));
    fill(wire.memory(store, store_bytes - buffer_bytes), 3000u);
    CHECK(av_d3d_BindBufferSlice(wire, mapped, store, store_bytes - buffer_bytes, buffer_bytes));
    CHECK(av_d3d_CopyResource(wire, late, mapped));
    CHECK_EQ(session.finish(), 0u);
    CHECK(holds(wire, late, 3000u));
    CHECK_EQ(wire.refused(), 0u);
    CHECK(session.backend().invariants_hold());
}

TEST(buffer_slice_device, a_buffer_follows_its_slice_from_one_block_of_memory_to_another)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice  *device = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!slice_host(apis, device, source)) return;
    SliceSession     session(rig, *device, *source);
    av::D3dLoopback &wire = session.wire();
    const std::uint64_t first = wire.open_memory(upload, store_bytes);
    const std::uint64_t second = wire.open_memory(upload, store_bytes);
    CHECK(first != 0u);
    CHECK(second != 0u);
    CHECK(first != second);
    constexpr std::uint32_t mapped = 10u, from_first = 11u, from_second = 12u;
    CHECK(av_d3d_CreateResource(wire, mapped, buffer_desc(usage_dynamic, bind_vertex, cpu_write), 0u, 0u, 0u));
    CHECK(av_d3d_CreateResource(wire, from_first, buffer_desc(usage_default, 0u, 0u), 0u, 0u, 0u));
    CHECK(av_d3d_CreateResource(wire, from_second, buffer_desc(usage_default, 0u, 0u), 0u, 0u, 0u));
    fill(wire.memory(first, first_slice), 1000u);
    fill(wire.memory(second, 0u), 2000u);
    CHECK(av_d3d_BindBufferSlice(wire, mapped, first, first_slice, buffer_bytes));
    CHECK(av_d3d_CopyResource(wire, from_first, mapped));
    CHECK(av_d3d_BindBufferSlice(wire, mapped, second, 0u, buffer_bytes));
    CHECK(av_d3d_CopyResource(wire, from_second, mapped));
    CHECK_EQ(session.finish(), 0u);
    CHECK(holds(wire, from_first, 1000u));
    CHECK(holds(wire, from_second, 2000u));
    CHECK_EQ(wire.refused(), 0u);
    CHECK(session.backend().invariants_hold());
}

namespace {
constexpr std::uint32_t dynamic = 20u, fixed = 21u, texture = 22u;

using Misuse = void (*)(av::D3dLoopback &wire, std::uint64_t store);

const Misuse misuses[] = {
    [](av::D3dLoopback &wire, std::uint64_t) {
        CHECK(av_d3d_BindBufferSlice(wire, dynamic, 0x1234u, 0u, buffer_bytes));
    },
    [](av::D3dLoopback &wire, std::uint64_t) {
        CHECK(av_d3d_BindBufferSlice(wire, dynamic, 0u, 0u, buffer_bytes));
    },
    [](av::D3dLoopback &wire, std::uint64_t store) {
        CHECK(av_d3d_BindBufferSlice(wire, dynamic, store, store_bytes, buffer_bytes));
    },
    [](av::D3dLoopback &wire, std::uint64_t store) {
        CHECK(av_d3d_BindBufferSlice(wire, dynamic, store, store_bytes + 4096u, buffer_bytes));
    },
    [](av::D3dLoopback &wire, std::uint64_t store) {
        CHECK(av_d3d_BindBufferSlice(wire, dynamic, store, store_bytes - 1024u, buffer_bytes));
    },
    [](av::D3dLoopback &wire, std::uint64_t store) {
        CHECK(av_d3d_BindBufferSlice(wire, dynamic, store, 4096u, ~std::uint64_t{0} - 1024u));
    },
    [](av::D3dLoopback &wire, std::uint64_t store) {
        CHECK(av_d3d_BindBufferSlice(wire, dynamic, store, 0u, buffer_bytes / 2u));
    },
    [](av::D3dLoopback &wire, std::uint64_t store) {
        CHECK(av_d3d_BindBufferSlice(wire, 99u, store, 0u, buffer_bytes));
    },
    [](av::D3dLoopback &wire, std::uint64_t store) {
        CHECK(av_d3d_BindBufferSlice(wire, texture, store, 0u, buffer_bytes));
    },
    [](av::D3dLoopback &wire, std::uint64_t store) {
        CHECK(av_d3d_BindBufferSlice(wire, fixed, store, 0u, buffer_bytes));
    },
};

}

TEST(buffer_slice_device, each_misuse_of_a_bind_removes_a_healthy_device)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice  *device = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!slice_host(apis, device, source)) return;
    for (const Misuse misuse : misuses) {
        SliceSession     session(rig, *device, *source);
        av::D3dLoopback &wire = session.wire();
        const std::uint64_t store = wire.open_memory(upload, store_bytes);
        CHECK(store != 0u);
        CHECK(av_d3d_CreateResource(wire, dynamic, buffer_desc(usage_dynamic, bind_vertex, cpu_write), 0u, 0u, 0u));
        CHECK(av_d3d_CreateResource(wire, fixed, buffer_desc(usage_default, bind_vertex, 0u), 0u, 0u, 0u));
        ResourceDesc image = {};
        image.dimension = 3u;
        image.width = 64u;
        image.height = 64u;
        image.depth_or_array = 1u;
        image.mip_levels = 1u;
        image.format = 87u;
        image.sample_count = 1u;
        CHECK(av_d3d_CreateResource(wire, texture, image, 0u, 0u, 0u));
        CHECK(av_d3d_BindBufferSlice(wire, dynamic, store, first_slice, buffer_bytes));
        CHECK_EQ(session.finish(), 0u);
        misuse(wire, store);
        CHECK_EQ(session.finish(), driver_internal_error);
    }
}

TEST(buffer_slice_device, a_store_that_cannot_be_imported_refuses_its_own_binds_and_no_other_store_s)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice  *device = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!slice_host(apis, device, source)) return;
    alignas(4096) static std::uint8_t unexported[store_bytes];
    av::LocalImport lost(unexported, sizeof unexported, 99u);
    lost.memory().bind(*source);
    SliceSession     session(rig, *device, *source);
    av::D3dLoopback &wire = session.wire();
    std::uint64_t lost_handle = 0u;
    av::ImportedBlob *taken = nullptr;
    CHECK_STATUS(rig.imports.take(99u, lost.memory(), taken), av::Status::ok);
    CHECK(session.backend().adopt(lost.memory(), av::BlobRole(), lost_handle));
    CHECK_STATUS(rig.imports.name(*taken, lost_handle), av::Status::ok);
    CHECK(av_d3d_CreateResource(wire, dynamic, buffer_desc(usage_dynamic, bind_vertex, cpu_write), 0u, 0u, 0u));
    CHECK(av_d3d_BindBufferSlice(wire, dynamic, lost_handle, 0u, buffer_bytes));
    CHECK_EQ(session.finish(), driver_internal_error);
    const std::uint64_t store = wire.open_memory(upload, store_bytes);
    CHECK(store != 0u);
    std::uint32_t hresult = 1u, level = 0u;
    CHECK(av_d3d_CreateDevice11(wire, 2u, 0u, 0u, &hresult, &level));
    CHECK(av_d3d_SelectDevice11(wire, 2u));
    CHECK(av_d3d_CreateResource(wire, dynamic + 1u, buffer_desc(usage_dynamic, bind_vertex, cpu_write), 0u, 0u, 0u));
    CHECK_EQ(session.finish(), 0u);
    CHECK_EQ(rig.sink.holding("could not be imported to draw from"), 1u);
    CHECK(av_d3d_BindBufferSlice(wire, dynamic + 1u, store, 0u, buffer_bytes));
    CHECK_EQ(session.finish(), 0u);
    CHECK_EQ(rig.sink.holding("could not be imported to draw from"), 1u);
    session.backend().abandon(lost.memory(), lost_handle);
    CHECK(rig.imports.forget(99u) == &lost.memory());
}

TEST(buffer_slice_device, a_buffer_on_a_store_that_goes_is_given_storage_of_its_own)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice  *device = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!slice_host(apis, device, source)) return;
    SliceSession     session(rig, *device, *source);
    av::D3dLoopback &wire = session.wire();
    const std::uint64_t store = wire.open_memory(upload, store_bytes);
    CHECK(store != 0u);
    constexpr std::uint32_t mapped = 10u, copy = 11u, shader = 12u;
    CHECK(av_d3d_CreateResource(wire, mapped, buffer_desc(usage_dynamic, bind_vertex, cpu_write), 0u, 0u, 0u));
    CHECK(av_d3d_CreateResource(wire, copy, buffer_desc(usage_default, 0u, 0u), 0u, 0u, 0u));
    CHECK(av_d3d_CreateShader(wire, shader, 0u, triangle_vs::inputs_count, triangle_vs::inputs,
                              triangle_vs::outputs_count, triangle_vs::outputs, 0u, nullptr, 0u, nullptr, 0u, nullptr,
                              0xffffffffu, triangle_vs::tokens, sizeof(triangle_vs::tokens)));
    fill(wire.memory(store, first_slice), 1000u);
    CHECK(av_d3d_BindBufferSlice(wire, mapped, store, first_slice, buffer_bytes));
    CHECK(av_d3d_CopyResource(wire, copy, mapped));
    CHECK_EQ(session.finish(), 0u);
    CHECK(holds(wire, copy, 1000u));

    wire.close_memory(store);
    static std::uint8_t written[buffer_bytes];
    fill(written, 7000u);
    CHECK(av_d3d_UpdateSubresource(wire, mapped, 0u, 0u, Box{}, buffer_bytes, buffer_bytes, copy_no_overwrite, written,
                                   sizeof written));
    CHECK(av_d3d_CopyResource(wire, copy, mapped));
    CHECK_EQ(session.finish(), 0u);
    CHECK(holds(wire, copy, 7000u));
    CHECK(holds(wire, mapped, 7000u));
    CHECK(av_d3d_BindBufferSlice(wire, mapped, store, first_slice, buffer_bytes));
    CHECK_EQ(session.finish(), driver_internal_error);
    CHECK_EQ(wire.refused(), 0u);
}

namespace {

struct Vertex {
    float x, y;
    float r, g, b, a;
};

constexpr std::uint32_t target_size = 64u;

std::uint32_t pixel(const std::uint8_t *pixels, std::uint32_t x, std::uint32_t y)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, pixels + (y * target_size + x) * 4u, sizeof(value));
    return value;
}

}

TEST(buffer_slice_device, a_draw_reads_vertices_and_constants_from_slices)
{
    av::ExecutorLogRig rig;
    av::LoaderHostApis apis(rig.log);
    av::D3d11HostDevice  *device = nullptr;
    av::HostMemorySource *source = nullptr;
    if (!slice_host(apis, device, source)) return;
    SliceSession     session(rig, *device, *source);
    av::D3dLoopback &wire = session.wire();
    const std::uint64_t store = wire.open_memory(upload, store_bytes);
    CHECK(store != 0u);
    std::uint32_t hresult = 1u;

    ResourceDesc target = {};
    target.dimension = 3u;
    target.width = target_size;
    target.height = target_size;
    target.depth_or_array = 1u;
    target.mip_levels = 1u;
    target.format = 87u;
    target.sample_count = 1u;
    target.bind_flags = 0x20u;
    CHECK(av_d3d_CreateResource(wire, 1u, target, 0u, 0u, 0u));
    ViewDesc rtv = {};
    rtv.resource = 1u;
    rtv.format = 87u;
    rtv.dimension = 4u;
    CHECK(av_d3d_CreateRenderTargetView(wire, 2u, rtv));

    const Vertex vertices[3] = {{-1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
                                {-1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
                                {0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f}};
    const float tint[4] = {0.5f, 0.5f, 0.5f, 1.0f};
    ResourceDesc vb = {};
    vb.dimension = 1u;
    vb.width = sizeof vertices;
    vb.usage = usage_dynamic;
    vb.bind_flags = bind_vertex;
    vb.cpu_access = cpu_write;
    CHECK(av_d3d_CreateResource(wire, 3u, vb, 0u, 0u, 0u));
    ResourceDesc cb = vb;
    cb.width = 16u;
    cb.bind_flags = 0x4u;
    CHECK(av_d3d_CreateResource(wire, 4u, cb, 0u, 0u, 0u));
    std::memcpy(wire.memory(store, first_slice), vertices, sizeof vertices);
    std::memcpy(wire.memory(store, second_slice), tint, sizeof tint);
    CHECK(av_d3d_BindBufferSlice(wire, 3u, store, first_slice, 256u));
    CHECK(av_d3d_BindBufferSlice(wire, 4u, store, second_slice, 256u));

    CHECK(av_d3d_CreateShader(wire, 5u, 0u, triangle_vs::inputs_count, triangle_vs::inputs, triangle_vs::outputs_count,
                              triangle_vs::outputs, 0u, nullptr, 0u, nullptr, 0u, nullptr, 0xffffffffu,
                              triangle_vs::tokens, sizeof(triangle_vs::tokens)));
    CHECK(av_d3d_CreateShader(wire, 6u, 4u, triangle_ps::inputs_count, triangle_ps::inputs, triangle_ps::outputs_count,
                              triangle_ps::outputs, 0u, nullptr, 0u, nullptr, 0u, nullptr, 0xffffffffu,
                              triangle_ps::tokens, sizeof(triangle_ps::tokens)));
    const InputElement elements[2] = {{0u, 0u, 16u, 0u, 0u, 0u}, {0u, 8u, 2u, 0u, 0u, 1u}};
    CHECK(av_d3d_CreateInputLayout(wire, 7u, 2u, elements));
    const std::uint32_t targets[1] = {2u};
    CHECK(av_d3d_SetRenderTargets(wire, 1u, targets, 0u, 1u, 0u, nullptr));
    CHECK(av_d3d_ClearRenderTargetView(wire, 2u, Float4{{0.0f, 0.0f, 1.0f, 1.0f}}));
    const Viewport viewport = {0.0f, 0.0f, float(target_size), float(target_size), 0.0f, 1.0f};
    CHECK(av_d3d_SetViewports(wire, 1u, &viewport));
    CHECK(av_d3d_SetInputLayout(wire, 7u));
    const VertexBufferBinding binding = {3u, sizeof(Vertex), 0u};
    CHECK(av_d3d_SetVertexBuffers(wire, 0u, 1u, &binding));
    CHECK(av_d3d_SetTopology(wire, 4u));
    CHECK(av_d3d_SetShader(wire, 0u, 5u));
    CHECK(av_d3d_SetShader(wire, 4u, 6u));
    const ConstantBufferBinding constants = {4u, 0u, 0u};
    CHECK(av_d3d_SetConstantBuffers(wire, 4u, 0u, 1u, &constants));
    CHECK(av_d3d_Draw(wire, 3u, 0u));

    static std::uint8_t pixels[target_size * target_size * 4u];
    std::uint32_t row_pitch = 0u, depth_pitch = 0u;
    std::uint64_t read = 0u;
    CHECK(av_d3d_ReadSubresource(wire, 1u, 0u, &hresult, &row_pitch, &depth_pitch, pixels, sizeof(pixels), &read));
    CHECK_EQ(hresult, 0u);
    const std::uint32_t tinted = pixel(pixels, 4u, target_size / 2u);
    CHECK_EQ(tinted & 0xff00ffffu, 0xff000000u);
    CHECK((tinted >> 16u & 0xffu) >= 0x7fu && (tinted >> 16u & 0xffu) <= 0x80u);
    CHECK_EQ(pixel(pixels, target_size - 4u, target_size / 2u), 0xff0000ffu);

    constexpr std::uint64_t third_slice = 192u * 1024u;
    const float full[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    std::memcpy(wire.memory(store, third_slice), full, sizeof full);
    CHECK(av_d3d_BindBufferSlice(wire, 4u, store, third_slice, 256u));
    CHECK(av_d3d_Draw(wire, 3u, 0u));
    CHECK(av_d3d_ReadSubresource(wire, 1u, 0u, &hresult, &row_pitch, &depth_pitch, pixels, sizeof(pixels), &read));
    CHECK_EQ(pixel(pixels, 4u, target_size / 2u), 0xffff0000u);
    CHECK_EQ(wire.refused(), 0u);
}
