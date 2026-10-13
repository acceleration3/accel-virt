/* A Direct3D 11 buffer bound to a slice of its session's region
 * (docs/mapped-buffers-plan.md, 3.2): the host GPU reads guest memory in
 * place, when the work runs - so bytes the guest writes after a command that
 * reads the buffer was recorded, but before the GPU ran it, are the bytes it
 * reads, as on a real driver whose mapped pointer is the GPU's memory. A
 * buffer bound again reads its new slice in the commands after, and its old
 * one in the commands before. The region is memory shaped as QEMU's guest RAM:
 * a sealed memfd, mapped shared, made a dma-buf by udmabuf. A host without
 * DXVK, the patched DXVK or udmabuf returns without judging. */
#include "core/support/object_storage.hpp"
#include "executor/d3d/d3d_backend.hpp"
#include "executor/imported_blob.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "executor/sharing/session_shares.hpp"
#include "fixtures/d3d/triangle_ps.hpp"
#include "fixtures/d3d/triangle_vs.hpp"
#include "harness/executor/d3d_loopback.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <linux/udmabuf.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

using namespace av::d3dproto;

namespace {

constexpr std::uint64_t region_bytes = std::uint64_t{1} << 20u;
constexpr std::uint32_t buffer_bytes = 4096u;
constexpr std::uint64_t first_slice = 64u * 1024u;
constexpr std::uint64_t second_slice = 128u * 1024u;
constexpr std::uint32_t usage_default = 0u, usage_dynamic = 2u;
constexpr std::uint32_t bind_vertex = 0x1u;
constexpr std::uint32_t cpu_write = 0x10000u;
constexpr std::uint32_t driver_internal_error = 0x887A0020u;   /* DXGI_ERROR_DRIVER_INTERNAL_ERROR */

/* Guest RAM as QEMU makes it, and a dma-buf of it from udmabuf. */
class GuestMemory {
public:
    GuestMemory() : memfd_(-1), dmabuf_(-1), bytes_(nullptr)
    {
        memfd_ = memfd_create("guest ram", MFD_CLOEXEC | MFD_ALLOW_SEALING);
        if (memfd_ < 0 || ftruncate(memfd_, region_bytes) != 0 || fcntl(memfd_, F_ADD_SEALS, F_SEAL_SHRINK) != 0)
            return;
        void *const mapped = mmap(nullptr, region_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, memfd_, 0);
        if (mapped == MAP_FAILED) return;
        bytes_ = static_cast<std::uint8_t *>(mapped);
        const int device = open("/dev/udmabuf", O_RDWR | O_CLOEXEC);
        if (device < 0) return;
        udmabuf_create request = {};
        request.memfd = static_cast<std::uint32_t>(memfd_);
        request.flags = UDMABUF_FLAGS_CLOEXEC;
        request.size = region_bytes;
        dmabuf_ = ioctl(device, UDMABUF_CREATE, &request);
        close(device);
    }
    ~GuestMemory()
    {
        if (bytes_ != nullptr) munmap(bytes_, region_bytes);
        if (dmabuf_ >= 0) close(dmabuf_);
        if (memfd_ >= 0) close(memfd_);
    }
    bool          made() const { return dmabuf_ >= 0 && bytes_ != nullptr; }
    int           dmabuf() const { return dmabuf_; }
    std::uint8_t *at(std::uint64_t offset) { return bytes_ + offset; }
    /* A buffer's words at `offset`, each `seed` plus its index. */
    void          fill(std::uint64_t offset, std::uint32_t seed)
    {
        for (std::uint32_t at = 0u; at < buffer_bytes / 4u; at++) {
            const std::uint32_t value = seed + at;
            std::memcpy(bytes_ + offset + std::uint64_t{at} * 4u, &value, sizeof value);
        }
    }

private:

    int           memfd_;
    int           dmabuf_;
    std::uint8_t *bytes_;
};

av::ObjectStorage<av::D3dBackend> backend_storage;

class HeldBackend {
public:
    HeldBackend() { static_cast<void>(backend_storage.construct()); }
    ~HeldBackend()
    {
        backend_storage.object().shut_down();
        backend_storage.destroy();
    }
    HeldBackend(const HeldBackend &) = delete;
    HeldBackend &operator=(const HeldBackend &) = delete;
    av::D3dBackend &backend() { return backend_storage.object(); }
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

/* Whether `resource` holds `seed` plus each word's index, read back. */
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

}  // namespace

TEST(buffer_slice_device, a_buffer_on_a_slice_reads_what_the_guest_wrote_when_the_gpu_runs)
{
    if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
    av::LoaderHostApis   apis;
    apis.open();
    av::D3d11HostDevice *device = nullptr;
    for (std::uint32_t index = 0u; index < apis.gpu_count() && device == nullptr; index++)
        device = apis.d3d11_device(index);
    GuestMemory memory;
    if (device == nullptr || !memory.made() || !device->slices()) {
        std::fprintf(stderr, "no DXVK that renames to imported storage, or no udmabuf, here: nothing judged\n");
        return;
    }
    HeldBackend       held;
    av::D3dBackend   &backend = held.backend();
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, device));
    av::ImportedBlob region;
    AvBlobHandle     handle = {};
    handle.kind = AV_BLOB_DMA_BUF;
    handle.fd = dup(memory.dmabuf());
    handle.pointer = memory.at(0u);   /* the device's own mapping, which the GPU imports */
    handle.size = region_bytes;
    CHECK_STATUS(region.take(1u, handle), av::Status::ok);
    CHECK(backend.adopt(region));
    av::D3dLoopback wire(backend);
    std::uint32_t   hresult = 1u, level = 0u, supported = 0u;
    /* Asked before the session has a device, as the guest asks. */
    CHECK(av_d3d_QueryBufferSlices11(wire, &supported));
    CHECK_EQ(supported, 1u);
    CHECK(av_d3d_CreateDevice11(wire, 1u, 0u, 0u, &hresult, &level));

    constexpr std::uint32_t mapped = 10u, early = 11u, late = 12u;
    CHECK(av_d3d_CreateResource(wire, mapped, buffer_desc(usage_dynamic, bind_vertex, cpu_write), 0u));
    CHECK(av_d3d_CreateResource(wire, early, buffer_desc(usage_default, 0u, 0u), 0u));
    CHECK(av_d3d_CreateResource(wire, late, buffer_desc(usage_default, 0u, 0u), 0u));

    /* Recorded first, written after: the copy reads what is there when the
     * GPU runs it. */
    memory.fill(first_slice, 1000u);
    CHECK(av_d3d_BindBufferSlice(wire, mapped, first_slice, buffer_bytes));
    CHECK(av_d3d_CopyResource(wire, early, mapped));
    memory.fill(first_slice, 5000u);
    /* Bound again: the copy before read the first slice, the one after reads
     * the second. */
    memory.fill(second_slice, 9000u);
    CHECK(av_d3d_BindBufferSlice(wire, mapped, second_slice, buffer_bytes));
    CHECK(av_d3d_CopyResource(wire, late, mapped));
    CHECK(av_d3d_Finish11(wire, &hresult));
    CHECK(holds(wire, early, 5000u));
    CHECK(holds(wire, late, 9000u));
    CHECK_EQ(wire.refused(), 0u);

    /* A slice running past the region is refused: the device that asked is
     * removed, and the next Finish11 says so. */
    CHECK(av_d3d_BindBufferSlice(wire, mapped, region_bytes - 1024u, buffer_bytes));
    CHECK(av_d3d_Finish11(wire, &hresult));
    CHECK_EQ(hresult, driver_internal_error);
}

/* A region handed over as a dma-buf with no host mapping beside it - an
 * older device's - has no pages a GPU can import coherently: the session says
 * no, and refuses a bind; the guest then copies what it writes, as before. */
TEST(buffer_slice_device, a_region_with_no_host_pages_has_no_slices)
{
    if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
    av::LoaderHostApis   apis;
    apis.open();
    av::D3d11HostDevice *device = nullptr;
    for (std::uint32_t index = 0u; index < apis.gpu_count() && device == nullptr; index++)
        device = apis.d3d11_device(index);
    GuestMemory memory;
    if (device == nullptr || !memory.made()) {
        std::fprintf(stderr, "no DXVK or no udmabuf here: nothing judged\n");
        return;
    }
    HeldBackend       held;
    av::D3dBackend   &backend = held.backend();
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, device));
    av::ImportedBlob region;
    AvBlobHandle     handle = {};
    handle.kind = AV_BLOB_DMA_BUF;
    handle.fd = dup(memory.dmabuf());
    handle.size = region_bytes;
    CHECK_STATUS(region.take(1u, handle), av::Status::ok);
    CHECK(backend.adopt(region));
    av::D3dLoopback wire(backend);
    std::uint32_t   hresult = 1u, level = 0u, supported = 1u;
    CHECK(av_d3d_CreateDevice11(wire, 1u, 0u, 0u, &hresult, &level));
    CHECK(av_d3d_QueryBufferSlices11(wire, &supported));
    CHECK_EQ(supported, 0u);
    /* The same, asked of the backend itself: answered, and no bind taken. */
    supported = 1u;
    CHECK(backend.on_QueryBufferSlices11(supported));
    CHECK_EQ(supported, 0u);
    CHECK(!backend.on_BindBufferSlice(10u, 0u, buffer_bytes));   /* not made yet */
    CHECK(av_d3d_CreateResource(wire, 10u, buffer_desc(usage_dynamic, bind_vertex, cpu_write), 0u));
    CHECK(av_d3d_Finish11(wire, &hresult));
    CHECK_EQ(hresult, 0u);
    CHECK(av_d3d_BindBufferSlice(wire, 10u, 0u, buffer_bytes));
    CHECK(av_d3d_Finish11(wire, &hresult));
    CHECK_EQ(hresult, driver_internal_error);
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

}  // namespace

/* A draw reads its vertices and its constants from buffers on slices: the
 * triangle test_d3d_backend_device draws from default buffers, here from
 * dynamic ones whose bytes the guest wrote into the region, as avumd does. */
TEST(buffer_slice_device, a_draw_reads_vertices_and_constants_from_slices)
{
    if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
    av::LoaderHostApis   apis;
    apis.open();
    av::D3d11HostDevice *device = nullptr;
    for (std::uint32_t index = 0u; index < apis.gpu_count() && device == nullptr; index++)
        device = apis.d3d11_device(index);
    GuestMemory memory;
    if (device == nullptr || !memory.made() || !device->slices()) {
        std::fprintf(stderr, "no DXVK that renames to imported storage, or no udmabuf, here: nothing judged\n");
        return;
    }
    HeldBackend       held;
    av::D3dBackend   &backend = held.backend();
    av::SessionShares unshared;
    CHECK(backend.initialize(unshared, device));
    av::ImportedBlob region;
    AvBlobHandle     handle = {};
    handle.kind = AV_BLOB_DMA_BUF;
    handle.fd = dup(memory.dmabuf());
    handle.pointer = memory.at(0u);   /* the device's own mapping, which the GPU imports */
    handle.size = region_bytes;
    CHECK_STATUS(region.take(1u, handle), av::Status::ok);
    CHECK(backend.adopt(region));
    av::D3dLoopback wire(backend);
    std::uint32_t   hresult = 1u, level = 0u, supported = 0u;
    CHECK(av_d3d_QueryBufferSlices11(wire, &supported));
    CHECK_EQ(supported, 1u);
    CHECK(av_d3d_CreateDevice11(wire, 1u, 0u, 0u, &hresult, &level));

    ResourceDesc target = {};
    target.dimension = 3u;
    target.width = target_size;
    target.height = target_size;
    target.depth_or_array = 1u;
    target.mip_levels = 1u;
    target.format = 87u;
    target.sample_count = 1u;
    target.bind_flags = 0x20u;
    CHECK(av_d3d_CreateResource(wire, 1u, target, 0u));
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
    CHECK(av_d3d_CreateResource(wire, 3u, vb, 0u));
    ResourceDesc cb = vb;
    cb.width = 16u;
    cb.bind_flags = 0x4u;
    CHECK(av_d3d_CreateResource(wire, 4u, cb, 0u));
    std::memcpy(memory.at(first_slice), vertices, sizeof vertices);
    std::memcpy(memory.at(second_slice), tint, sizeof tint);
    CHECK(av_d3d_BindBufferSlice(wire, 3u, first_slice, 256u));
    CHECK(av_d3d_BindBufferSlice(wire, 4u, second_slice, 256u));

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

    /* Renamed, as a discarding map renames it, and drawn again: the draw
     * after reads the new slice, with the buffer still bound from before. */
    constexpr std::uint64_t third_slice = 192u * 1024u;
    const float full[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    std::memcpy(memory.at(third_slice), full, sizeof full);
    CHECK(av_d3d_BindBufferSlice(wire, 4u, third_slice, 256u));
    CHECK(av_d3d_Draw(wire, 3u, 0u));
    CHECK(av_d3d_ReadSubresource(wire, 1u, 0u, &hresult, &row_pitch, &depth_pitch, pixels, sizeof(pixels), &read));
    CHECK_EQ(pixel(pixels, 4u, target_size / 2u), 0xffff0000u);
    CHECK_EQ(wire.refused(), 0u);
}
