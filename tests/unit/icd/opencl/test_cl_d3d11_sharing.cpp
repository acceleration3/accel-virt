#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "harness/icd/fake_cl_loader.hpp"
#include "icd/opencl/cl_dispatch_table.hpp"
#include "icd/opencl/cl_icd_binding.hpp"
#include "icd/opencl/cl_icd_root.hpp"
#include "icd/opencl/d3d11_resource.hpp"
#include "proto/escape/av_context_record.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/shared_object_record.hpp"
#include "proto/resources/surface_layout.hpp"
#include "proto/submission/command_header.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ClIcdRig;
using av::FakeClLoader;
using av::clicd::ClIcdRoot;
using av::clicd::D3d11Resource;

namespace {

constexpr std::uint64_t host_platform = 0x6200u;
constexpr std::uint64_t host_device = 0x6300u;
constexpr std::uint64_t host_context = 0x6400u;
constexpr std::uint64_t host_queue = 0x6500u;
constexpr std::uint64_t host_buffer = 0x6600u;
constexpr std::uint64_t host_image = 0x6A00u;
constexpr std::uint64_t host_event = 0x6900u;

using DeviceIds = cl_int(CL_API_CALL *)(cl_platform_id, cl_uint, void *, cl_uint, cl_uint, cl_device_id *, cl_uint *);
using FromBuffer = cl_mem(CL_API_CALL *)(cl_context, cl_mem_flags, void *, cl_int *);
using FromTexture = cl_mem(CL_API_CALL *)(cl_context, cl_mem_flags, void *, cl_uint, cl_int *);
using Objects = cl_int(CL_API_CALL *)(cl_command_queue, cl_uint, const cl_mem *, cl_uint, const cl_event *, cl_event *);

constexpr std::uint32_t rgba8 = 28u;
const D3d11Resource     small_texture(D3d11Resource::Kind::texture2d, rgba8, 0u, 4u, 2u, 1u, 1u, 1u, 1u);
const D3d11Resource     small_buffer(D3d11Resource::Kind::buffer, 0u, 64u, 0u, 0u, 0u, 1u, 1u, 1u);

class Shared {
public:
    explicit Shared(ClIcdRig &rig) : rig_(rig), buffer_bytes_{}, texture_bytes_{}
    {
        for (std::size_t index = 0u; index < sizeof buffer_bytes_; ++index)
            buffer_bytes_[index] = static_cast<std::uint8_t>(0x40u + index);
        for (std::size_t index = 0u; index < sizeof texture_bytes_; ++index)
            texture_bytes_[index] = static_cast<std::uint8_t>(0x80u + index);
        rig_.d3d_adapters().name_device(&device_, ClIcdRig::adapter_luid);
        rig_.d3d11().name_device(&device_);
        rig_.d3d11().name_resource(&buffer_, &device_, small_buffer, buffer_bytes_, sizeof buffer_bytes_, 64u, 64u);
        rig_.d3d11().name_resource(&texture_, &device_, small_texture, texture_bytes_, sizeof texture_bytes_, 16u,
                                   32u);
        rig_.d3d11().name_resource(&strangers_, &stranger_device_, small_buffer, buffer_bytes_,
                                   sizeof buffer_bytes_, 64u, 64u);
        if (rig_.loader().load() != FakeClLoader::Load::kept) return;
        rig_.host().arena_import(0x77u, 3u);
        rig_.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
        rig_.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
        rig_.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
        rig_.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
        rig_.host().cl_info(CL_SUCCESS, "", 1u);
        cl_uint devices = 0u;
        if (rig_.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &cl_device_, &devices) != CL_SUCCESS) return;
        context_ = create(&device_, made_);
        if (context_ == nullptr) return;
        rig_.host().cl_handle(host_queue, CL_SUCCESS);
        queue_ = rig_.loader().create_queue(context_, cl_device_, nullptr);
    }

    bool             made() const { return queue_ != nullptr; }
    cl_device_id     device() const { return cl_device_; }
    cl_context       context() const { return context_; }
    cl_command_queue queue() const { return queue_; }
    void            *d3d_device() { return &device_; }
    void            *buffer() { return &buffer_; }
    void            *texture() { return &texture_; }
    void            *strangers() { return &strangers_; }
    std::uint8_t    *buffer_bytes() { return buffer_bytes_; }
    std::uint8_t    *texture_bytes() { return texture_bytes_; }

    cl_context create(void *d3d11_device, cl_int &error)
    {
        rig_.host().arena_import(0x78u, 3u);
        rig_.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
        rig_.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
        rig_.host().cl_handle(host_context, CL_SUCCESS);
        const cl_context_properties properties[] = {
            static_cast<cl_context_properties>(ClIcdRoot::d3d11_context_device),
            reinterpret_cast<cl_context_properties>(d3d11_device), 0};
        return rig_.loader().dispatch_of(cl_device_)->clCreateContext(properties, 1u, &cl_device_, nullptr, nullptr,
                                                                      &error);
    }

    cl_mem from_buffer(cl_mem_flags flags, void *resource, cl_int &error)
    {
        return reinterpret_cast<FromBuffer>(rig_.loader().dispatch_of(context_)->clCreateFromD3D11BufferKHR)(
            context_, flags, resource, &error);
    }
    cl_mem from_texture2d(cl_mem_flags flags, void *resource, cl_uint subresource, cl_int &error)
    {
        return reinterpret_cast<FromTexture>(rig_.loader().dispatch_of(context_)->clCreateFromD3D11Texture2DKHR)(
            context_, flags, resource, subresource, &error);
    }
    cl_mem from_texture3d(cl_mem_flags flags, void *resource, cl_uint subresource, cl_int &error)
    {
        return reinterpret_cast<FromTexture>(rig_.loader().dispatch_of(context_)->clCreateFromD3D11Texture3DKHR)(
            context_, flags, resource, subresource, &error);
    }
    cl_int acquire(cl_uint count, const cl_mem *objects, cl_uint waits = 0u, const cl_event *events = nullptr,
                   cl_event *event = nullptr)
    {
        return reinterpret_cast<Objects>(rig_.loader().dispatch_of(queue_)->clEnqueueAcquireD3D11ObjectsKHR)(
            queue_, count, objects, waits, events, event);
    }
    cl_int release(cl_uint count, const cl_mem *objects, cl_uint waits = 0u, const cl_event *events = nullptr,
                   cl_event *event = nullptr)
    {
        return reinterpret_cast<Objects>(rig_.loader().dispatch_of(queue_)->clEnqueueReleaseD3D11ObjectsKHR)(
            queue_, count, objects, waits, events, event);
    }

    void element_is(std::size_t bytes)
    {
        rig_.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&bytes), sizeof bytes);
    }

private:
    ClIcdRig        &rig_;
    int              device_ = 0, stranger_device_ = 0;
    int              buffer_ = 0, texture_ = 0, strangers_ = 0;
    std::uint8_t     buffer_bytes_[64];
    std::uint8_t     texture_bytes_[32];
    cl_device_id     cl_device_ = nullptr;
    cl_context       context_ = nullptr;
    cl_command_queue queue_ = nullptr;
    cl_int           made_ = CL_INVALID_VALUE;
};

bool carries(const av::MockKmt &kmt, std::uint32_t index, const std::uint8_t *bytes, std::size_t count)
{
    const std::uint8_t *const frame = kmt.command_bytes(index);
    if (frame == nullptr) return false;
    for (std::size_t at = av::CommandHeader::encoded_size; at + count <= av::MockKmt::journal_bytes; ++at)
        if (std::memcmp(frame + at, bytes, count) == 0) return true;
    return false;
}

std::uint64_t shared_handle(ClIcdRig &rig)
{
    std::uint32_t adapter = 0u;
    std::uint32_t found = 0u;
    CHECK_STATUS(rig.kmt().enumerate_adapters(&adapter, 1u, found), av::Status::ok);
    std::uint32_t elsewhere = 0u;
    CHECK_STATUS(rig.kmt().create_device(adapter, elsewhere), av::Status::ok);
    const av::AllocationDescriptor carrier(av::AllocationKind::client, true, av::SurfaceLayout(), 4096u,
                                           av::ScanoutTarget());
    std::uint8_t bytes[av::AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size] = {};
    CHECK_STATUS(carrier.encode(bytes, av::AllocationDescriptor::encoded_size), av::Status::ok);
    const av::SharedObjectRecord asking(0u, av::SharedObjectKind(av::SharedObjectKind::memory), 0u, 4096u);
    CHECK_STATUS(asking.encode(bytes + av::AllocationDescriptor::encoded_size, av::SharedObjectRecord::encoded_size),
                 av::Status::ok);
    std::uint32_t allocation = 0u;
    CHECK_STATUS(rig.kmt().create_shared_allocation(elsewhere, bytes, sizeof bytes, allocation), av::Status::ok);
    std::uint64_t named = 0u;
    CHECK_STATUS(rig.kmt().share_object(elsewhere, allocation, named), av::Status::ok);
    return named;
}

}

TEST(cl_d3d11_sharing, an_acquire_copies_each_resource_in_and_a_release_copies_it_back)
{ ClIcdRig rig;
  Shared   shared(rig);
  CHECK(shared.made());
  if (!shared.made()) return;
  cl_int error = CL_INVALID_VALUE;
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  const cl_mem buffer = shared.from_buffer(CL_MEM_READ_WRITE, shared.buffer(), error);
  CHECK_EQ(error, CL_SUCCESS);
  rig.host().cl_handle(host_image, CL_SUCCESS);
  const cl_mem image = shared.from_texture2d(CL_MEM_READ_WRITE, shared.texture(), 0u, error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(buffer != nullptr && image != nullptr);
  CHECK_EQ(rig.d3d11().references(shared.buffer()), 1);
  CHECK_EQ(rig.d3d11().references(shared.texture()), 1);
  const cl_mem both[2] = {buffer, image};

  shared.element_is(4u);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  const std::uint32_t sent = rig.kmt().commands();
  cl_event            acquired = nullptr;
  CHECK_EQ(shared.acquire(2u, both, 0u, nullptr, &acquired), CL_SUCCESS);
  CHECK(acquired != nullptr);
  CHECK_EQ(rig.d3d11().reads(), 2u);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueWriteBuffer);
  CHECK(carries(rig.kmt(), sent, shared.buffer_bytes(), 64u));
  CHECK_EQ(rig.kmt().command_id(sent + 1u), av::clproto::kCmd_clGetImageInfo);
  CHECK_EQ(rig.kmt().command_id(sent + 2u), av::clproto::kCmd_clEnqueueWriteImage);
  CHECK(carries(rig.kmt(), sent + 2u, shared.texture_bytes(), 32u));
  CHECK_EQ(rig.kmt().command_id(sent + 3u), av::clproto::kCmd_clEnqueueMarkerWithWaitList);

  std::uint8_t written[64] = {};
  for (std::size_t index = 0u; index < sizeof written; ++index) written[index] = static_cast<std::uint8_t>(index);
  rig.host().cl_bytes(CL_SUCCESS, written, 64u);
  shared.element_is(4u);
  rig.host().cl_bytes(CL_SUCCESS, written, 32u);
  CHECK_EQ(shared.release(2u, both), CL_SUCCESS);
  CHECK_EQ(rig.d3d11().writes(), 2u);
  CHECK_EQ(std::memcmp(shared.buffer_bytes(), written, 64u), 0);
  CHECK_EQ(std::memcmp(shared.texture_bytes(), written, 32u), 0);

  CHECK_EQ(rig.loader().release_mem(buffer), CL_SUCCESS);
  CHECK_EQ(rig.loader().release_mem(image), CL_SUCCESS);
  CHECK_EQ(rig.d3d11().references(shared.buffer()), 0);
  CHECK_EQ(rig.d3d11().references(shared.texture()), 0);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_d3d11_sharing, a_context_is_made_from_a_d3d11_device_on_this_adapter_and_held_for_its_life)
{ ClIcdRig rig;
  Shared   shared(rig);
  CHECK(shared.made());
  if (!shared.made()) return;
  CHECK_EQ(rig.d3d11().references(shared.d3d_device()), 1);
  int elsewhere = 0, not_a_device = 0;
  rig.d3d11().name_device(&elsewhere);
  rig.d3d_adapters().name_device(&elsewhere, ClIcdRig::adapter_luid + 1u);
  cl_int error = CL_SUCCESS;
  CHECK(shared.create(&elsewhere, error) == nullptr);
  CHECK_EQ(error, ClIcdRoot::invalid_d3d11_device);
  CHECK(shared.create(&not_a_device, error) == nullptr);
  CHECK_EQ(error, ClIcdRoot::invalid_d3d11_device);
  CHECK_EQ(rig.d3d11().references(&elsewhere), 0);
  const cl_context ordinary = shared.create(nullptr, error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(ordinary != nullptr);
  cl_bool      prefer = CL_TRUE;
  std::size_t  answered = 0u;
  CHECK_EQ(rig.loader().dispatch_of(ordinary)->clGetContextInfo(ordinary, ClIcdRoot::d3d11_prefer_shared,
                                                                sizeof prefer, &prefer, &answered),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.loader().dispatch_of(shared.context())->clGetContextInfo(shared.context(),
                                                                        ClIcdRoot::d3d11_prefer_shared,
                                                                        sizeof prefer, &prefer, &answered),
           CL_SUCCESS);
  CHECK_EQ(prefer, static_cast<cl_bool>(CL_FALSE));
  CHECK_EQ(answered, sizeof prefer);
  CHECK_EQ(rig.loader().release_queue(shared.queue()), CL_SUCCESS);
  CHECK_EQ(rig.loader().release_context(shared.context()), CL_SUCCESS);
  CHECK_EQ(rig.d3d11().references(shared.d3d_device()), 0);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_d3d11_sharing, a_context_may_share_with_opengl_and_direct3d_11_both)
{ ClIcdRig rig;
  Shared   shared(rig);
  CHECK(shared.made());
  if (!shared.made()) return;
  int gl_context = 0, window_dc = 0;
  constexpr std::uint32_t gl_share_name = 0x40000C42u;
  rig.gl_share().make_current(&gl_context, gl_share_name);
  const cl_context_properties properties[] = {
      CL_GL_CONTEXT_KHR, reinterpret_cast<cl_context_properties>(&gl_context), CL_WGL_HDC_KHR,
      reinterpret_cast<cl_context_properties>(&window_dc),
      static_cast<cl_context_properties>(ClIcdRoot::d3d11_context_device),
      reinterpret_cast<cl_context_properties>(shared.d3d_device()), 0};
  rig.host().arena_import(0x79u, 3u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(host_context + 1u, CL_SUCCESS);
  cl_int             error = CL_INVALID_VALUE;
  const cl_device_id device = shared.device();
  const cl_context   both =
      rig.loader().dispatch_of(device)->clCreateContext(properties, 1u, &device, nullptr, nullptr, &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(both != nullptr);
  av::AvContextRecord record;
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), av::Status::ok);
  CHECK_EQ(record.options().share_context(), gl_share_name);
  CHECK_EQ(rig.d3d11().references(shared.d3d_device()), 2);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_d3d11_sharing, a_resource_the_extension_does_not_take_is_refused_before_the_host)
{ ClIcdRig rig;
  Shared   shared(rig);
  CHECK(shared.made());
  if (!shared.made()) return;
  int multisampled = 0, typeless = 0, volume = 0;
  rig.d3d11().name_resource(&multisampled, shared.d3d_device(),
                            D3d11Resource(D3d11Resource::Kind::texture2d, rgba8, 0u, 4u, 4u, 1u, 1u, 1u, 4u), nullptr,
                            0u, 0u, 0u);
  rig.d3d11().name_resource(&typeless, shared.d3d_device(),
                            D3d11Resource(D3d11Resource::Kind::texture2d, 27u, 0u, 4u, 4u, 1u, 1u, 1u, 1u), nullptr,
                            0u, 0u, 0u);
  rig.d3d11().name_resource(&volume, shared.d3d_device(),
                            D3d11Resource(D3d11Resource::Kind::texture3d, rgba8, 0u, 4u, 4u, 4u, 2u, 1u, 1u), nullptr,
                            0u, 0u, 0u);
  const std::uint32_t sent = rig.kmt().commands();
  cl_int              error = CL_SUCCESS;
  CHECK(shared.from_buffer(CL_MEM_READ_WRITE, shared.texture(), error) == nullptr);
  CHECK_EQ(error, ClIcdRoot::invalid_d3d11_resource);
  CHECK(shared.from_texture2d(CL_MEM_READ_WRITE, shared.buffer(), 0u, error) == nullptr);
  CHECK_EQ(error, ClIcdRoot::invalid_d3d11_resource);
  CHECK(shared.from_texture3d(CL_MEM_READ_WRITE, shared.texture(), 0u, error) == nullptr);
  CHECK_EQ(error, ClIcdRoot::invalid_d3d11_resource);
  CHECK(shared.from_buffer(CL_MEM_READ_WRITE, shared.strangers(), error) == nullptr);
  CHECK_EQ(error, ClIcdRoot::invalid_d3d11_resource);
  CHECK(shared.from_texture2d(CL_MEM_READ_WRITE, &multisampled, 0u, error) == nullptr);
  CHECK_EQ(error, ClIcdRoot::invalid_d3d11_resource);
  CHECK(shared.from_texture2d(CL_MEM_READ_WRITE, shared.texture(), 1u, error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK(shared.from_texture3d(CL_MEM_READ_WRITE, &volume, 2u, error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK(shared.from_texture2d(CL_MEM_READ_WRITE, &typeless, 0u, error) == nullptr);
  CHECK_EQ(error, CL_INVALID_IMAGE_FORMAT_DESCRIPTOR);
  CHECK(shared.from_buffer(CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, shared.buffer(), error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent);
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  const cl_mem buffer = shared.from_buffer(CL_MEM_READ_ONLY, shared.buffer(), error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(shared.from_buffer(CL_MEM_READ_ONLY, shared.buffer(), error) == nullptr);
  CHECK_EQ(error, ClIcdRoot::invalid_d3d11_resource);
  rig.host().cl_handle(0u, CL_OUT_OF_RESOURCES);
  CHECK(shared.from_texture3d(CL_MEM_READ_WRITE, &volume, 1u, error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.d3d11().references(&volume), 0);
  const cl_context ordinary = shared.create(nullptr, error);
  CHECK(reinterpret_cast<FromBuffer>(rig.loader().dispatch_of(ordinary)->clCreateFromD3D11BufferKHR)(
            ordinary, CL_MEM_READ_WRITE, shared.buffer(), &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  CHECK(reinterpret_cast<FromBuffer>(rig.loader().dispatch_of(ordinary)->clCreateFromD3D11BufferKHR)(
            nullptr, CL_MEM_READ_WRITE, shared.buffer(), &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  CHECK(rig.root().create_from_d3d11_buffer(nullptr, CL_MEM_READ_WRITE, shared.buffer(), nullptr) == nullptr);
  CHECK(buffer != nullptr);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_d3d11_sharing, acquire_and_release_take_turns_and_copy_only_what_each_side_reads)
{ ClIcdRig rig;
  Shared   shared(rig);
  CHECK(shared.made());
  if (!shared.made()) return;
  cl_int error = CL_INVALID_VALUE;
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  const cl_mem written_only = shared.from_buffer(CL_MEM_WRITE_ONLY, shared.buffer(), error);
  rig.host().cl_handle(host_image, CL_SUCCESS);
  const cl_mem read_only = shared.from_texture2d(CL_MEM_READ_ONLY, shared.texture(), 0u, error);
  CHECK(written_only != nullptr && read_only != nullptr);
  CHECK_EQ(shared.release(1u, &written_only), -1009);
  shared.element_is(4u);
  rig.host().cl_result(CL_SUCCESS);
  const cl_mem both[2] = {written_only, read_only};
  CHECK_EQ(shared.acquire(2u, both), CL_SUCCESS);
  CHECK_EQ(rig.d3d11().reads(), 1u);
  CHECK_EQ(shared.acquire(1u, &read_only), -1008);
  rig.host().cl_bytes(CL_SUCCESS, shared.buffer_bytes(), 64u);
  CHECK_EQ(shared.release(2u, both), CL_SUCCESS);
  CHECK_EQ(rig.d3d11().writes(), 1u);
  rig.d3d11().refuse_next();
  CHECK_EQ(shared.acquire(1u, &read_only), CL_OUT_OF_RESOURCES);
  shared.element_is(4u);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(shared.acquire(1u, &read_only), CL_SUCCESS);
  CHECK_EQ(shared.release(1u, &written_only), -1009);
  CHECK_EQ(shared.release(1u, &read_only), CL_SUCCESS);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_d3d11_sharing, a_copy_back_direct3d_refuses_is_the_release_s_error)
{ ClIcdRig rig;
  Shared   shared(rig);
  CHECK(shared.made());
  if (!shared.made()) return;
  cl_int error = CL_INVALID_VALUE;
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  const cl_mem buffer = shared.from_buffer(CL_MEM_WRITE_ONLY, shared.buffer(), error);
  CHECK_EQ(shared.acquire(1u, &buffer), CL_SUCCESS);
  rig.d3d11().refuse_next();
  CHECK_EQ(shared.release(1u, &buffer), CL_OUT_OF_RESOURCES);
  CHECK_EQ(shared.acquire(1u, &buffer), -1008);
  rig.host().cl_bytes(CL_INVALID_OPERATION, shared.buffer_bytes(), 0u);
  CHECK_EQ(shared.release(1u, &buffer), CL_INVALID_OPERATION);
  rig.host().cl_bytes(CL_SUCCESS, shared.buffer_bytes(), 64u);
  CHECK_EQ(shared.release(1u, &buffer), CL_SUCCESS);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_d3d11_sharing, a_queue_or_wait_list_the_api_refuses_changes_no_hands)
{ ClIcdRig rig;
  Shared   shared(rig);
  CHECK(shared.made());
  if (!shared.made()) return;
  cl_int error = CL_INVALID_VALUE;
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  const cl_mem buffer = shared.from_buffer(CL_MEM_WRITE_ONLY, shared.buffer(), error);
  Objects const acquire =
      reinterpret_cast<Objects>(rig.loader().dispatch_of(shared.queue())->clEnqueueAcquireD3D11ObjectsKHR);
  Objects const release =
      reinterpret_cast<Objects>(rig.loader().dispatch_of(shared.queue())->clEnqueueReleaseD3D11ObjectsKHR);
  int not_a_queue = 0;
  CHECK_EQ(acquire(reinterpret_cast<cl_command_queue>(&not_a_queue), 1u, &buffer, 0u, nullptr, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(release(reinterpret_cast<cl_command_queue>(&not_a_queue), 1u, &buffer, 0u, nullptr, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(shared.acquire(1u, &buffer, 1u, nullptr), CL_INVALID_EVENT_WAIT_LIST);
  CHECK_EQ(shared.acquire(0u, &buffer), CL_INVALID_VALUE);
  int stranger = 0;
  const cl_mem not_ours = reinterpret_cast<cl_mem>(&stranger);
  CHECK_EQ(shared.acquire(1u, &not_ours), CL_INVALID_MEM_OBJECT);
  const cl_context ordinary = shared.create(nullptr, error);
  rig.host().cl_handle(host_queue + 1u, CL_SUCCESS);
  const cl_command_queue plain = rig.loader().create_queue(ordinary, shared.device(), nullptr);
  CHECK_EQ(acquire(plain, 1u, &buffer, 0u, nullptr, nullptr), CL_INVALID_CONTEXT);
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  cl_event acquired = nullptr;
  CHECK_EQ(shared.acquire(1u, &buffer, 0u, nullptr, &acquired), CL_SUCCESS);
  rig.host().cl_bytes(CL_SUCCESS, shared.buffer_bytes(), 64u);
  CHECK_EQ(shared.release(1u, &buffer), CL_SUCCESS);
  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(shared.acquire(1u, &buffer, 1u, &acquired), CL_SUCCESS);
  CHECK(rig.kmt().commands() > sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_d3d11_sharing, an_object_says_its_resource_and_subresource)
{ ClIcdRig rig;
  Shared   shared(rig);
  CHECK(shared.made());
  if (!shared.made()) return;
  int array = 0;
  std::uint8_t level[32] = {};
  rig.d3d11().name_resource(&array, shared.d3d_device(),
                            D3d11Resource(D3d11Resource::Kind::texture2d, rgba8, 0u, 8u, 4u, 1u, 2u, 3u, 1u), level,
                            sizeof level, 16u, 32u);
  cl_int error = CL_INVALID_VALUE;
  rig.host().cl_handle(host_image, CL_SUCCESS);
  const cl_mem image = shared.from_texture2d(CL_MEM_READ_WRITE, &array, 5u, error);
  CHECK_EQ(error, CL_SUCCESS);
  void       *resource = nullptr;
  cl_uint     subresource = 0u;
  std::size_t answered = 0u;
  CHECK_EQ(rig.loader().dispatch_of(image)->clGetMemObjectInfo(image, ClIcdRoot::d3d11_resource_info,
                                                               sizeof resource, &resource, &answered),
           CL_SUCCESS);
  CHECK(resource == &array);
  CHECK_EQ(rig.loader().dispatch_of(image)->clGetImageInfo(image, ClIcdRoot::d3d11_subresource_info,
                                                           sizeof subresource, &subresource, &answered),
           CL_SUCCESS);
  CHECK_EQ(subresource, 5u);
  shared.element_is(4u);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(shared.acquire(1u, &image), CL_SUCCESS);
  CHECK_EQ(rig.d3d11().subresource(), 5u);
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  const cl_mem plain = rig.loader().create_buffer(shared.context(), CL_MEM_READ_WRITE, 64u, nullptr);
  CHECK_EQ(rig.loader().dispatch_of(plain)->clGetMemObjectInfo(plain, ClIcdRoot::d3d11_resource_info,
                                                               sizeof resource, &resource, &answered),
           ClIcdRoot::invalid_d3d11_resource);
  CHECK_EQ(rig.loader().dispatch_of(plain)->clGetImageInfo(plain, ClIcdRoot::d3d11_subresource_info,
                                                           sizeof subresource, &subresource, &answered),
           ClIcdRoot::invalid_d3d11_resource); }

TEST(cl_d3d11_sharing, a_d3d11_device_or_dxgi_adapter_on_the_avgpu_adapter_is_this_device)
{ ClIcdRig rig;
  Shared   shared(rig);
  CHECK(shared.made());
  if (!shared.made()) return;
  int adapter = 0, elsewhere = 0;
  rig.d3d_adapters().name_adapter(&adapter, ClIcdRig::adapter_luid);
  const cl_platform_id platform = rig.loader().platform(0u);
  DeviceIds const      device_ids =
      reinterpret_cast<DeviceIds>(rig.loader().dispatch_of(platform)->clGetDeviceIDsFromD3D11KHR);
  cl_device_id found = nullptr;
  cl_uint      count = 0u;
  CHECK_EQ(device_ids(platform, ClIcdRoot::d3d11_device_source, shared.d3d_device(),
                      ClIcdRoot::d3d11_preferred_devices, 1u, &found, &count),
           CL_SUCCESS);
  CHECK(found == shared.device());
  CHECK_EQ(count, 1u);
  CHECK_EQ(device_ids(platform, ClIcdRoot::d3d11_adapter_source, &adapter, ClIcdRoot::d3d11_all_devices, 1u,
                      &found, &count),
           CL_SUCCESS);
  count = 0u;
  CHECK_EQ(device_ids(platform, ClIcdRoot::d3d11_adapter_source, &adapter, ClIcdRoot::d3d11_all_devices, 0u,
                      nullptr, &count),
           CL_SUCCESS);
  CHECK_EQ(count, 1u);
  CHECK_EQ(device_ids(platform, ClIcdRoot::d3d11_adapter_source, &elsewhere, ClIcdRoot::d3d11_all_devices, 1u,
                      &found, &count),
           CL_DEVICE_NOT_FOUND);
  CHECK_EQ(count, 0u);
  CHECK_EQ(device_ids(platform, 0x4010u, &adapter, ClIcdRoot::d3d11_all_devices, 1u, &found, &count),
           CL_INVALID_VALUE);
  CHECK_EQ(device_ids(platform, ClIcdRoot::d3d11_adapter_source, &adapter, 0x4013u, 1u, &found, &count),
           CL_INVALID_VALUE);
  CHECK_EQ(device_ids(platform, ClIcdRoot::d3d11_adapter_source, &adapter, ClIcdRoot::d3d11_all_devices, 1u,
                      nullptr, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(device_ids(platform, ClIcdRoot::d3d11_adapter_source, &adapter, ClIcdRoot::d3d11_all_devices, 0u,
                      &found, &count),
           CL_INVALID_VALUE);
  int not_a_platform = 0;
  CHECK_EQ(device_ids(reinterpret_cast<cl_platform_id>(&not_a_platform), ClIcdRoot::d3d11_adapter_source, &adapter,
                      ClIcdRoot::d3d11_all_devices, 1u, &found, &count),
           CL_INVALID_PLATFORM); }

TEST(cl_d3d11_sharing, the_one_device_is_a_root_device_its_retain_and_release_leave_alone)
{ ClIcdRig rig;
  Shared   shared(rig);
  CHECK(shared.made());
  if (!shared.made()) return;
  const cl_device_id device = shared.device();
  CHECK_EQ(rig.loader().dispatch_of(device)->clRetainDevice(device), CL_SUCCESS);
  CHECK_EQ(rig.loader().dispatch_of(device)->clReleaseDevice(device), CL_SUCCESS);
  CHECK_EQ(rig.loader().dispatch_of(device)->clReleaseDevice(device), CL_SUCCESS);
  CHECK_EQ(rig.root().device_reference(device), CL_SUCCESS);
  int not_a_device = 0;
  CHECK_EQ(rig.loader().dispatch_of(device)->clRetainDevice(reinterpret_cast<cl_device_id>(&not_a_device)),
           CL_INVALID_DEVICE);
  CHECK_EQ(rig.loader().dispatch_of(device)->clReleaseDevice(reinterpret_cast<cl_device_id>(&not_a_device)),
           CL_INVALID_DEVICE);
  cl_uint found = 0u;
  CHECK_EQ(rig.loader().device_ids(CL_DEVICE_TYPE_GPU, 0u, nullptr, &found), CL_SUCCESS);
  CHECK_EQ(found, 1u); }

TEST(cl_d3d11_sharing, an_acquire_or_release_that_fails_leaves_each_object_where_it_was)
{ ClIcdRig rig;
  Shared   shared(rig);
  CHECK(shared.made());
  if (!shared.made()) return;
  cl_int error = CL_INVALID_VALUE;
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  const cl_mem buffer = shared.from_buffer(CL_MEM_WRITE_ONLY, shared.buffer(), error);
  rig.host().cl_enqueued(CL_OUT_OF_RESOURCES, 0u);
  cl_event event = nullptr;
  CHECK_EQ(shared.acquire(1u, &buffer, 0u, nullptr, &event), CL_OUT_OF_RESOURCES);
  CHECK(event == nullptr);
  CHECK_EQ(shared.acquire(1u, &buffer), CL_SUCCESS);
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  cl_event marker = nullptr;
  CHECK_EQ(rig.loader().dispatch_of(shared.queue())->clEnqueueMarkerWithWaitList(shared.queue(), 0u, nullptr, &marker),
           CL_SUCCESS);
  rig.host().cl_enqueued(CL_OUT_OF_RESOURCES, 0u);
  CHECK_EQ(shared.release(1u, &buffer), CL_OUT_OF_RESOURCES);
  CHECK_EQ(shared.acquire(1u, &buffer), -1008);
  CHECK_EQ(rig.d3d11().writes(), 0u);
  int other_buffer = 0;
  rig.d3d11().name_resource(&other_buffer, shared.d3d_device(), small_buffer, shared.buffer_bytes(), 64u, 64u, 64u);
  rig.host().cl_handle(0u, CL_OUT_OF_RESOURCES);
  CHECK(shared.from_buffer(CL_MEM_READ_WRITE, &other_buffer, error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.d3d11().references(&other_buffer), 0);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_d3d11_sharing, a_lost_device_acquires_and_releases_nothing)
{ ClIcdRig rig;
  Shared   shared(rig);
  CHECK(shared.made());
  if (!shared.made()) return;
  cl_int error = CL_INVALID_VALUE;
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  const cl_mem buffer = shared.from_buffer(CL_MEM_WRITE_ONLY, shared.buffer(), error);
  CHECK_EQ(shared.acquire(1u, &buffer), CL_SUCCESS);
  rig.host().cl_handle(host_buffer + 1u, CL_SUCCESS);
  const cl_mem plain = rig.loader().create_buffer(shared.context(), CL_MEM_READ_WRITE, 64u, nullptr);
  CHECK_EQ(rig.loader().release_mem(plain), CL_SUCCESS);
  CHECK_EQ(shared.release(1u, &buffer), CL_OUT_OF_RESOURCES);
  CHECK_EQ(shared.acquire(1u, &buffer), CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.d3d11().writes(), 0u);
  CHECK_EQ(rig.loader().release_mem(buffer), CL_SUCCESS);
  CHECK_EQ(rig.d3d11().references(shared.buffer()), 0); }

TEST(cl_d3d11_sharing, an_object_no_row_can_be_kept_for_is_given_back)
{ ClIcdRig rig;
  Shared   shared(rig);
  CHECK(shared.made());
  if (!shared.made()) return;
  cl_int error = CL_SUCCESS;
  rig.platform().fail_allocation_at(0u);
  rig.host().cl_handle(host_buffer + 1u, CL_SUCCESS);
  CHECK(rig.loader().create_buffer(shared.context(), CL_MEM_READ_WRITE, 64u, nullptr) != nullptr);
  const std::uint32_t buffer_allocations = rig.platform().allocations_made();
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  rig.platform().fail_allocation_at(buffer_allocations + 1u);
  CHECK(shared.from_buffer(CL_MEM_READ_WRITE, shared.buffer(), error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);
  CHECK_EQ(rig.d3d11().references(shared.buffer()), 0);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_d3d11_sharing, its_entries_reached_before_a_root_exists_refuse)
{ CHECK(!av::clicd::cl_icd_binding.bound());
  av::clicd::ClDispatchTable table;
  table.fill();
  const cl_icd_dispatch &slots = table.table();
  cl_uint                count = 7u;
  CHECK_EQ(reinterpret_cast<DeviceIds>(slots.clGetDeviceIDsFromD3D11KHR)(
               nullptr, ClIcdRoot::d3d11_adapter_source, nullptr, ClIcdRoot::d3d11_all_devices, 0u, nullptr, &count),
           CL_INVALID_PLATFORM);
  cl_int error = 7;
  CHECK(reinterpret_cast<FromBuffer>(slots.clCreateFromD3D11BufferKHR)(nullptr, 0u, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  error = 7;
  CHECK(reinterpret_cast<FromTexture>(slots.clCreateFromD3D11Texture2DKHR)(nullptr, 0u, nullptr, 0u, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  error = 7;
  CHECK(reinterpret_cast<FromTexture>(slots.clCreateFromD3D11Texture3DKHR)(nullptr, 0u, nullptr, 0u, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  CHECK(reinterpret_cast<FromBuffer>(slots.clCreateFromD3D11BufferKHR)(nullptr, 0u, nullptr, nullptr) == nullptr);
  CHECK(reinterpret_cast<FromTexture>(slots.clCreateFromD3D11Texture2DKHR)(nullptr, 0u, nullptr, 0u, nullptr) ==
        nullptr);
  CHECK(reinterpret_cast<FromTexture>(slots.clCreateFromD3D11Texture3DKHR)(nullptr, 0u, nullptr, 0u, nullptr) ==
        nullptr);
  CHECK_EQ(reinterpret_cast<Objects>(slots.clEnqueueAcquireD3D11ObjectsKHR)(nullptr, 0u, nullptr, 0u, nullptr,
                                                                            nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(reinterpret_cast<Objects>(slots.clEnqueueReleaseD3D11ObjectsKHR)(nullptr, 0u, nullptr, 0u, nullptr,
                                                                            nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(slots.clRetainDevice(nullptr), CL_INVALID_DEVICE);
  CHECK_EQ(slots.clReleaseDevice(nullptr), CL_INVALID_DEVICE); }

TEST(cl_d3d11_sharing, its_numbers_are_the_specifications)
{ CHECK_EQ(ClIcdRoot::d3d11_device_source, 0x4019u);
  CHECK_EQ(ClIcdRoot::d3d11_adapter_source, 0x401Au);
  CHECK_EQ(ClIcdRoot::d3d11_preferred_devices, 0x401Bu);
  CHECK_EQ(ClIcdRoot::d3d11_all_devices, 0x401Cu);
  CHECK_EQ(ClIcdRoot::d3d11_context_device, 0x401Du);
  CHECK_EQ(ClIcdRoot::d3d11_resource_info, 0x401Eu);
  CHECK_EQ(ClIcdRoot::d3d11_subresource_info, 0x401Fu);
  CHECK_EQ(ClIcdRoot::d3d11_prefer_shared, 0x402Du);
  CHECK_EQ(ClIcdRoot::invalid_d3d11_device, -1006);
  CHECK_EQ(ClIcdRoot::invalid_d3d11_resource, -1007); }

TEST(cl_d3d11_sharing, a_refused_device_query_is_logged_with_its_number)
{ ClIcdRig rig;
  Shared   shared(rig);
  CHECK(shared.made());
  if (!shared.made()) return;
  const std::uint32_t before = rig.log().lines();
  rig.host().cl_info(CL_INVALID_VALUE, nullptr, 0u);
  rig.host().cl_info(CL_INVALID_VALUE, nullptr, 0u);
  std::uint64_t free_memory[2] = {};
  CHECK_EQ(rig.loader().device_info(shared.device(), 0x4039u, sizeof free_memory, free_memory, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.log().lines(), before + 1u);
  CHECK(std::strcmp(rig.log().line(before), "avcl: clGetDeviceInfo 0x4039 answered -30") == 0);
  static const char direct[] = "clGetDeviceInfo";
  CHECK_EQ(rig.root().noted_query(direct, 0x4039u, CL_INVALID_VALUE), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().noted_query(direct, 0x4039u, CL_INVALID_VALUE), CL_INVALID_VALUE);
  CHECK_EQ(rig.log().lines(), before + 2u);
  CHECK(std::strcmp(rig.log().line(before + 1u), "avcl: clGetDeviceInfo 0x4039 answered -30") == 0); }

TEST(cl_d3d11_sharing, a_texture_the_device_shares_a_twin_of_is_an_image_over_the_twin)
{ ClIcdRig rig;
  Shared   shared(rig);
  CHECK(shared.made());
  if (!shared.made()) return;
  int twin = 0;
  rig.d3d11().offer_twin(&twin, shared_handle(rig));
  rig.host().shared_object(0u, host_image);
  const std::uint32_t sent = rig.kmt().commands();
  cl_int              error = CL_INVALID_VALUE;
  const cl_mem        image = shared.from_texture2d(CL_MEM_READ_WRITE, shared.texture(), 0u, error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(image != nullptr);
  if (image == nullptr) return;
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK_EQ(rig.kmt().command_id(sent), av::CommandHeader::open_shared_object_id);
  CHECK_EQ(rig.d3d11().closed(), 0u);
  CHECK_EQ(rig.d3d11().references(&twin), 1);
  CHECK_EQ(rig.d3d11().references(shared.texture()), 1);

  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  CHECK_EQ(shared.acquire(1u, &image), CL_SUCCESS);
  CHECK_EQ(rig.d3d11().to_twins(), 1u);
  CHECK_EQ(rig.d3d11().reads(), 0u);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(shared.release(1u, &image), CL_SUCCESS);
  CHECK_EQ(rig.d3d11().from_twins(), 1u);
  CHECK_EQ(rig.d3d11().writes(), 0u);

  rig.host().shared_object(0u, 0u);
  const std::uint32_t before = rig.kmt().commands();
  CHECK_EQ(rig.loader().release_mem(image), CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(before), av::CommandHeader::release_shared_object_id);
  CHECK_EQ(rig.d3d11().references(&twin), 0);
  CHECK_EQ(rig.d3d11().references(shared.texture()), 0);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_d3d11_sharing, a_twin_that_cannot_be_opened_leaves_the_object_to_cross_through_memory)
{ ClIcdRig rig;
  Shared   shared(rig);
  CHECK(shared.made());
  if (!shared.made()) return;
  int twin = 0;
  rig.d3d11().offer_twin(&twin, 0x7777u);
  rig.host().cl_handle(host_image, CL_SUCCESS);
  cl_int       error = CL_INVALID_VALUE;
  const cl_mem image = shared.from_texture2d(CL_MEM_READ_WRITE, shared.texture(), 0u, error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(image != nullptr);
  CHECK_EQ(rig.d3d11().closed(), 1u);
  CHECK_EQ(rig.d3d11().references(&twin), 0);
  shared.element_is(4u);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  CHECK_EQ(shared.acquire(1u, &image), CL_SUCCESS);
  CHECK_EQ(rig.d3d11().reads(), 1u);
  CHECK_EQ(rig.d3d11().to_twins(), 0u);
  CHECK_EQ(rig.loader().release_mem(image), CL_SUCCESS);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_d3d11_sharing, an_srgb_texture_is_never_twinned_and_crosses_through_memory)
{ ClIcdRig rig;
  Shared   shared(rig);
  CHECK(shared.made());
  if (!shared.made()) return;
  constexpr std::uint32_t rgba8_srgb = 29u;
  const D3d11Resource     srgb_texture(D3d11Resource::Kind::texture2d, rgba8_srgb, 0u, 4u, 2u, 1u, 1u, 1u, 1u);
  int                     srgb = 0;
  std::uint8_t            texels[32] = {};
  rig.d3d11().name_resource(&srgb, shared.d3d_device(), srgb_texture, texels, sizeof texels, 16u, 32u);
  int twin = 0;
  rig.d3d11().offer_twin(&twin, shared_handle(rig));
  rig.host().cl_handle(host_image, CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  cl_int              error = CL_INVALID_VALUE;
  const cl_mem        image = shared.from_texture2d(CL_MEM_READ_WRITE, &srgb, 0u, error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(image != nullptr);
  if (image == nullptr) return;
  CHECK(rig.kmt().command_id(sent) != av::CommandHeader::open_shared_object_id);
  CHECK_EQ(rig.d3d11().references(&twin), 0);
  shared.element_is(4u);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  CHECK_EQ(shared.acquire(1u, &image), CL_SUCCESS);
  CHECK_EQ(rig.d3d11().reads(), 1u);
  CHECK_EQ(rig.d3d11().to_twins(), 0u);
  CHECK_EQ(rig.loader().release_mem(image), CL_SUCCESS);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_d3d11_sharing, a_twin_copy_direct3d_refuses_is_the_call_s_error)
{ ClIcdRig rig;
  Shared   shared(rig);
  CHECK(shared.made());
  if (!shared.made()) return;
  int twin = 0;
  rig.d3d11().offer_twin(&twin, shared_handle(rig));
  rig.host().shared_object(0u, host_image);
  cl_int       error = CL_INVALID_VALUE;
  const cl_mem image = shared.from_texture2d(CL_MEM_READ_WRITE, shared.texture(), 0u, error);
  CHECK(image != nullptr);
  if (image == nullptr) return;
  rig.d3d11().refuse_next();
  CHECK_EQ(shared.acquire(1u, &image), CL_OUT_OF_RESOURCES);
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  CHECK_EQ(shared.acquire(1u, &image), CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  rig.d3d11().refuse_next();
  CHECK_EQ(shared.release(1u, &image), CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.d3d11().from_twins(), 0u);
  rig.host().shared_object(0u, 0u);
  CHECK_EQ(rig.loader().release_mem(image), CL_SUCCESS);
  CHECK_EQ(rig.d3d11().references(&twin), 0);
  CHECK(rig.root().invariants_hold()); }
