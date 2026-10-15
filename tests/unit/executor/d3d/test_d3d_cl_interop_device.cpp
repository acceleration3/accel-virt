/* A Direct3D 11 texture a guest shares, opened by OpenCL on the same GPU as
 * the image it is (docs/d3d11-sharing.md, stage 2): Direct3D 11 makes the
 * texture over exportable memory and says what image it is
 * (SharedImageShape), the session publishes the memory, and an OpenCL
 * session's open of it is an image over that memory - read back texel for
 * texel where Direct3D wrote it, with nothing copied. An OpenCL without
 * cl_khr_image2d_from_buffer (rusticl on radeonsi) can name no image over a
 * buffer, and the open is the bytes themselves, read at the pitch Direct3D
 * laid them out with. A host without DXVK, OpenGL or OpenCL on one GPU
 * returns without judging. */
#include "proto/d3d/resource_misc.hpp"
#include "executor/d3d/d3d_backend.hpp"
#include "executor/opencl/opencl_backend.hpp"
#include "executor/sharing/host_shares.hpp"
#include "executor/sharing/session_shares.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "executor/shim/opencl_device.hpp"
#include "harness/executor/d3d_loopback.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"

#include <CL/cl.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <unistd.h>

namespace {

using namespace av::d3dproto;

constexpr std::uint32_t size = 64u;
constexpr std::uint32_t format_b8g8r8a8 = 87u;
constexpr std::uint32_t bind_shader_resource = 0x8u;
constexpr std::uint32_t bind_render_target = 0x20u;
/* Shared and asked for as rows, as the code of ours that makes a texture
 * another API reads asks for it (SharedRowsScope). */
constexpr std::uint32_t misc_shared = av::ResourceMisc::shared_bit | av::ResourceMisc::rows_bit;
constexpr std::uint64_t key = 0x5100000000000041ull;
constexpr std::uint32_t domain = 1u;

cl_context guest_context(av::OpenClBackend &backend, const av::OpenClDevice &device)
{
    cl_device_id          named = static_cast<cl_device_id>(device.device());
    cl_context_properties props[] = {CL_CONTEXT_PLATFORM,
                                     reinterpret_cast<cl_context_properties>(device.platform()), 0};
    cl_int                made = CL_SUCCESS;
    cl_context const      context = backend.on_clCreateContext(props, 1u, &named, &made);
    if (context != nullptr) backend.made(av::clproto::HandleKind::context, context);
    return context;
}

/* Whether `device` makes an image over a buffer's memory, an optional
 * feature of OpenCL 3.0. */
bool images_over_buffers(cl_device_id device)
{
    std::size_t length = 0u;
    if (::clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, 0u, nullptr, &length) != CL_SUCCESS || length == 0u)
        return false;
    static char names[16384];
    if (length > sizeof names ||
        ::clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, length, names, nullptr) != CL_SUCCESS)
        return false;
    return std::strstr(names, "cl_khr_image2d_from_buffer") != nullptr;
}

/* The pitch Direct3D laid the shared texture's rows out with. */
std::uint32_t texture_pitch(av::HostShares &shares, av::D3d11HostDevice &device)
{
    IUnknown *const held = shares.resource(domain, key);
    ID3D11Texture2D *texture = nullptr;
    if (held == nullptr || FAILED(held->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&texture)))) {
        if (held != nullptr) held->Release();
        return 0u;
    }
    std::uint32_t pitch = 0u;
    device.lock(nullptr);
    if (!device.exportable_pitch(texture, pitch)) pitch = 0u;
    device.unlock();
    texture->Release();
    held->Release();
    return pitch;
}

}  // namespace

TEST(d3d_cl_interop_device, a_shared_texture_opened_by_opencl_is_the_image_direct3d_wrote)
{
    if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
    av::LoaderHostApis apis;
    apis.open();
    std::uint32_t gpu = apis.gpu_count();
    for (std::uint32_t index = 0u; index < apis.gpu_count() && gpu == apis.gpu_count(); index++)
        if (apis.d3d11_device(index) != nullptr && apis.gl_display(index) != nullptr &&
            apis.opencl_device(index) != nullptr)
            gpu = index;
    if (gpu == apis.gpu_count()) {
        std::fprintf(stderr, "no GPU here runs DXVK, OpenGL and OpenCL: nothing judged\n");
        return;
    }
    av::HostShares *const shares = apis.shares(gpu);
    av::D3dBackend        direct3d;
    av::SessionShares     d3d_session;
    d3d_session.bind(*shares, domain, direct3d);
    CHECK(direct3d.initialize(d3d_session, apis.d3d11_device(gpu)));
    av::D3dLoopback wire(direct3d);
    std::uint32_t   hresult = 1u, level = 0u;
    CHECK(av_d3d_CreateDevice11(wire, 1u, 0u, 0u, &hresult, &level));
    ResourceDesc shared = {};
    shared.dimension = 3u;
    shared.width = size;
    shared.height = size;
    shared.depth_or_array = 1u;
    shared.mip_levels = 1u;
    shared.format = format_b8g8r8a8;
    shared.sample_count = 1u;
    shared.bind_flags = bind_shader_resource | bind_render_target;
    shared.misc_flags = misc_shared;
    CHECK(av_d3d_CreateResource(wire, 1u, shared, key));
    /* Every texel its own bytes, so a pitch or an order OpenCL read the
     * memory with that Direct3D did not lay it out by moves them. */
    static std::uint8_t pattern[size * size * 4u];
    for (std::uint32_t at = 0u; at < size * size; at++) {
        pattern[at * 4u] = static_cast<std::uint8_t>(at % size);
        pattern[at * 4u + 1u] = static_cast<std::uint8_t>(at / size);
        pattern[at * 4u + 2u] = static_cast<std::uint8_t>(at * 7u);
        pattern[at * 4u + 3u] = 0xffu;
    }
    const Box whole = {0u, 0u, 0u, size, size, 1u};
    CHECK(av_d3d_UpdateSubresource(wire, 1u, 0u, 1u, whole, size * 4u, 0u, 0u, pattern, sizeof pattern));
    CHECK(av_d3d_Finish11(wire, &hresult));
    /* The memory is published as the guest's SHARE_OBJECT would: by the
     * session that made it, its size what the dma-buf is. */
    std::uint32_t handle_kind = 0u;
    int           fd = -1;
    CHECK(direct3d.export_object(AV_SHARED_MEMORY, 1u, handle_kind, fd));
    const off_t bytes = fd >= 0 ? lseek(fd, 0, SEEK_END) : 0;
    if (fd >= 0) close(fd);
    CHECK(bytes > 0);
    CHECK(d3d_session.publish(key, AV_SHARED_MEMORY, static_cast<std::uint64_t>(bytes), 1u) == av::Status::ok);

    av::OpenClBackend opencl;
    CHECK(opencl.initialize(apis.opencl_device(gpu), apis.gl_display(gpu), nullptr));
    cl_context const context = guest_context(opencl, *apis.opencl_device(gpu));
    CHECK(context != nullptr);
    av::SessionShares cl_session;
    cl_session.bind(*shares, domain, opencl);
    std::uint64_t handle = 0u;
    CHECK(cl_session.open(key, AV_SHARED_MEMORY, static_cast<std::uint64_t>(bytes), handle) == av::Status::ok);
    cl_mem const image = reinterpret_cast<cl_mem>(handle);
    cl_device_id const device = static_cast<cl_device_id>(apis.opencl_device(gpu)->device());
    const bool         as_image = images_over_buffers(device);
    cl_mem_object_type type = 0u;
    CHECK_EQ(::clGetMemObjectInfo(image, CL_MEM_TYPE, sizeof type, &type, nullptr), CL_SUCCESS);
    CHECK_EQ(type, static_cast<cl_mem_object_type>(as_image ? CL_MEM_OBJECT_IMAGE2D : CL_MEM_OBJECT_BUFFER));

    cl_int                 made = CL_SUCCESS;
    cl_command_queue const queue = ::clCreateCommandQueueWithProperties(context, device, nullptr, &made);
    CHECK_EQ(made, CL_SUCCESS);
    static std::uint8_t read[size * size * 4u];
    const std::size_t   origin[3] = {0u, 0u, 0u};
    if (as_image) {
        cl_image_format format = {};
        CHECK_EQ(::clGetImageInfo(image, CL_IMAGE_FORMAT, sizeof format, &format, nullptr), CL_SUCCESS);
        CHECK_EQ(format.image_channel_order, static_cast<cl_channel_order>(CL_BGRA));
        const std::size_t region[3] = {size, size, 1u};
        CHECK_EQ(::clEnqueueReadImage(queue, image, CL_TRUE, origin, region, size * 4u, 0u, read, 0u, nullptr,
                                      nullptr),
                 CL_SUCCESS);
    } else {
        const std::uint32_t pitch = texture_pitch(*shares, *apis.d3d11_device(gpu));
        CHECK(pitch >= size * 4u);
        const std::size_t region[3] = {size * 4u, size, 1u};
        CHECK_EQ(::clEnqueueReadBufferRect(queue, image, CL_TRUE, origin, origin, region, pitch, 0u, size * 4u, 0u,
                                           read, 0u, nullptr, nullptr),
                 CL_SUCCESS);
    }
    CHECK(std::memcmp(read, pattern, sizeof read) == 0);
    ::clReleaseCommandQueue(queue);
    cl_session.close(key);
    cl_session.end();
    opencl.shut_down();
    d3d_session.end();
    direct3d.shut_down();
    shares->release(domain, 0u);
}
