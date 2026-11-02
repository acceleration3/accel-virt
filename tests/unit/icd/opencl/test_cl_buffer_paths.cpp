#include "harness/framework/test_registry.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "harness/icd/fake_cl_loader.hpp"
#include "harness/icd/recording_log.hpp"
#include "icd/opencl/cl_icd_root.hpp"
#include "icd/opencl/guest_cl_context.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/shared_object_record.hpp"
#include "proto/resources/surface_layout.hpp"
#include "proto/submission/command_header.hpp"

#include <CL/cl_ext.h>

#include <cstdint>
#include <cstring>

using av::ClIcdRig;
using av::FakeClLoader;


namespace av::clicd {
struct ClIcdRootProbe {
    static GuestClContext *context(ClIcdRoot &root, cl_context handle)
    { ClObject *const held = root.judged(handle, ClObject::Kind::context);
      return held != nullptr ? root.context_of(*held) : nullptr; }
};

}

namespace {
bool fill_mappings(ClIcdRig &rig, cl_context context)
{
    av::clicd::GuestClContext *const guest = av::clicd::ClIcdRootProbe::context(rig.root(), context);
    if (guest == nullptr) return false;
    static std::uint8_t somewhere[av::clicd::GuestClContext::max_mappings];
    for (std::uint32_t row = guest->mappings(); row < av::clicd::GuestClContext::max_mappings; ++row)
        if (guest->take_shared_mapping(0x5eedu, 0x5eedu, 0u, 1u, somewhere + row) == nullptr) return false;
    return true;
}

}

namespace {

constexpr std::uint64_t host_platform = 0x6200u;
constexpr std::uint64_t host_device = 0x6300u;
constexpr std::uint64_t host_context = 0x6400u;
constexpr std::uint64_t host_queue = 0x6500u;
constexpr std::uint64_t host_buffer = 0x6600u;
constexpr std::uint64_t host_event = 0x6700u;
constexpr std::uint64_t host_mapped = 0x7F0000008000ull;

constexpr std::size_t buffer_bytes = 256u;

void arena_answers(ClIcdRig &rig, bool arena, std::uint64_t handle)
{
    rig.host().blob_import(arena ? 0u : 1u, arena ? handle : 0u);
    if (!arena) rig.host().blob_import(1u, 0u);
}

void device_opens(ClIcdRig &rig, bool arena)
{
    arena_answers(rig, arena, 0x77u);
    rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
    rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
    rig.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
    rig.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
    rig.host().cl_info(CL_SUCCESS, "", 1u);
}

class Opened {
public:
    Opened(ClIcdRig &rig, bool arena) : rig_(rig), context_(nullptr), queue_(nullptr)
    {
        if (rig_.loader().load() != FakeClLoader::Load::kept) return;
        device_opens(rig_, arena);
        cl_device_id device = nullptr;
        cl_uint      devices = 0u;
        if (rig_.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device, &devices) != CL_SUCCESS)
            return;
        arena_answers(rig_, arena, 0x78u);
        rig_.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
        rig_.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
        rig_.host().cl_handle(host_context, CL_SUCCESS);
        context_ = rig_.loader().create_context(device, nullptr);
        if (context_ == nullptr) return;
        rig_.host().cl_handle(host_queue, CL_SUCCESS);
        queue_ = rig_.loader().create_queue(context_, device, nullptr);
    }

    bool             made() const { return context_ != nullptr && queue_ != nullptr; }
    cl_context       context() const { return context_; }
    cl_command_queue queue() const { return queue_; }

private:
    ClIcdRig        &rig_;
    cl_context       context_;
    cl_command_queue queue_;
};

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
    const av::SharedObjectRecord asking(0u, av::SharedObjectKind(av::SharedObjectKind::memory), 0u,
                                        4096u);
    CHECK_STATUS(asking.encode(bytes + av::AllocationDescriptor::encoded_size,
                               av::SharedObjectRecord::encoded_size),
                 av::Status::ok);
    std::uint32_t allocation = 0u;
    CHECK_STATUS(rig.kmt().create_shared_allocation(elsewhere, bytes, sizeof bytes, allocation),
                 av::Status::ok);
    std::uint64_t named = 0u;
    CHECK_STATUS(rig.kmt().share_object(elsewhere, allocation, named), av::Status::ok);
    return named;
}

}

TEST(cl_buffer_paths, a_buffer_over_a_shared_object_is_opened_and_released_as_one)
{ ClIcdRig rig;
  Opened   opened(rig, true);
  CHECK(opened.made());
  if (!opened.made()) return;
  const std::uint64_t handle = shared_handle(rig);
  const cl_mem_properties list[] = {CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR,
                                    static_cast<cl_mem_properties>(handle), 0u};
  rig.host().shared_object(0u, host_buffer);
  const std::uint32_t sent = rig.kmt().commands();
  cl_int              error = CL_INVALID_VALUE;
  cl_mem              buffer = rig.loader().create_buffer_with_properties(
      opened.context(), list, CL_MEM_READ_WRITE, buffer_bytes, &error);
  CHECK(buffer != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK_EQ(rig.kmt().command_id(sent), av::CommandHeader::open_shared_object_id);
  CHECK(rig.root().invariants_hold());

  rig.host().shared_object(0u, 0u);
  CHECK_EQ(rig.loader().release_mem(buffer), CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 2u);
  CHECK_EQ(rig.kmt().command_id(sent + 1u), av::CommandHeader::release_shared_object_id);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, the_acquire_and_release_of_an_external_object_are_answered_by_this_driver)
{ ClIcdRig rig;
  Opened   opened(rig, true);
  CHECK(opened.made());
  if (!opened.made()) return;
  const std::uint64_t     handle = shared_handle(rig);
  const cl_mem_properties list[] = {CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR,
                                    static_cast<cl_mem_properties>(handle), 0u};
  rig.host().shared_object(0u, host_buffer);
  cl_int error = CL_INVALID_VALUE;
  cl_mem shared = rig.loader().create_buffer_with_properties(opened.context(), list,
                                                             CL_MEM_READ_WRITE, buffer_bytes, &error);
  CHECK(shared != nullptr);
  if (shared == nullptr) return;

  auto *const acquire = reinterpret_cast<clEnqueueAcquireExternalMemObjectsKHR_fn>(
      rig.root().extension_function("clEnqueueAcquireExternalMemObjectsKHR"));
  auto *const release = reinterpret_cast<clEnqueueReleaseExternalMemObjectsKHR_fn>(
      rig.root().extension_function("clEnqueueReleaseExternalMemObjectsKHR"));
  CHECK(acquire != nullptr);
  CHECK(release != nullptr);
  CHECK(rig.root().extension_function("clEnqueueAcquireExternalMemObjects") == nullptr);
  CHECK(rig.root().extension_function("clEnqueueAcquireExternalMemObjectsKHR_and_more") == nullptr);
  if (acquire == nullptr || release == nullptr) return;

  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(acquire(opened.queue(), 1u, &shared, 0u, nullptr, nullptr), CL_SUCCESS);
  CHECK_EQ(release(opened.queue(), 1u, &shared, 0u, nullptr, nullptr), CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent);

  rig.host().cl_handle(host_buffer + 1u, CL_SUCCESS);
  cl_mem const ordinary = rig.loader().create_buffer(opened.context(), CL_MEM_READ_WRITE,
                                                     buffer_bytes, &error);
  CHECK(ordinary != nullptr);
  CHECK_EQ(acquire(opened.queue(), 1u, &ordinary, 0u, nullptr, nullptr), CL_INVALID_MEM_OBJECT);
  cl_mem nothing = nullptr;
  CHECK_EQ(acquire(opened.queue(), 1u, &nothing, 0u, nullptr, nullptr), CL_INVALID_MEM_OBJECT);
  CHECK_EQ(acquire(opened.queue(), 0u, &shared, 0u, nullptr, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(acquire(opened.queue(), 1u, nullptr, 0u, nullptr, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(acquire(nullptr, 1u, &shared, 0u, nullptr, nullptr), CL_INVALID_COMMAND_QUEUE);

  cl_device_id device = nullptr;
  cl_uint      devices = 0u;
  CHECK_EQ(rig.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device, &devices), CL_SUCCESS);
  rig.host().blob_import(0u, 0x79u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(host_context + 1u, CL_SUCCESS);
  cl_context const elsewhere = rig.loader().create_context(device, nullptr);
  CHECK(elsewhere != nullptr);
  if (elsewhere != nullptr) {
      rig.host().cl_handle(host_buffer + 2u, CL_SUCCESS);
      cl_mem const theirs = rig.loader().create_buffer(elsewhere, CL_MEM_READ_WRITE, buffer_bytes,
                                                       &error);
      CHECK(theirs != nullptr);
      CHECK_EQ(acquire(opened.queue(), 1u, &theirs, 0u, nullptr, nullptr), CL_INVALID_CONTEXT);
  }

  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  cl_event event = nullptr;
  const std::uint32_t before = rig.kmt().commands();
  CHECK_EQ(acquire(opened.queue(), 1u, &shared, 0u, nullptr, &event), CL_SUCCESS);
  CHECK(event != nullptr);
  CHECK_EQ(rig.kmt().commands(), before + 1u);
  CHECK_EQ(rig.kmt().command_id(before),
           static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueMarkerWithWaitList));

  rig.host().cl_enqueued(CL_OUT_OF_RESOURCES, 0u);
  cl_event refused = nullptr;
  CHECK_EQ(acquire(opened.queue(), 1u, &shared, 0u, nullptr, &refused), CL_OUT_OF_RESOURCES);
  CHECK(refused == nullptr);

  rig.kmt().withhold_answer_at(1u);
  CHECK_EQ(rig.loader().finish(opened.queue()), CL_OUT_OF_RESOURCES);
  CHECK_EQ(acquire(opened.queue(), 1u, &shared, 0u, nullptr, &refused), CL_OUT_OF_RESOURCES);
  CHECK_EQ(acquire(opened.queue(), 1u, &shared, 0u, nullptr, nullptr), CL_SUCCESS);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, an_object_the_guest_cannot_keep_is_given_back_to_the_device)
{ ClIcdRig rig;
  Opened   opened(rig, false);
  CHECK(opened.made());
  if (!opened.made()) return;
  const std::uint64_t handle = shared_handle(rig);

  const std::uint64_t earlier = shared_handle(rig);
  const cl_mem_properties first[] = {CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR,
                                     static_cast<cl_mem_properties>(earlier), 0u};
  rig.host().shared_object(0u, 0x9000u);
  cl_int imported = CL_OUT_OF_RESOURCES;
  CHECK(rig.loader().create_buffer_with_properties(opened.context(), first, CL_MEM_READ_WRITE, buffer_bytes,
                                                   &imported) != nullptr);
  CHECK_EQ(imported, CL_SUCCESS);

  const std::uint32_t before = rig.platform().live_allocations();
  std::uint32_t       made = 0u;
  for (; made < 256u; made++) {
      rig.host().cl_handle(host_buffer + made, CL_SUCCESS);
      CHECK(rig.loader().create_buffer(opened.context(), CL_MEM_READ_WRITE, buffer_bytes, nullptr) !=
            nullptr);
      if (rig.platform().live_allocations() != before) break;
  }
  CHECK(made < 256u);
  for (std::uint32_t index = 1u; index < 64u; index++) {
      rig.host().cl_handle(host_buffer + 256u + index, CL_SUCCESS);
      CHECK(rig.loader().create_buffer(opened.context(), CL_MEM_READ_WRITE, buffer_bytes, nullptr) !=
            nullptr);
  }

  const cl_mem_properties list[] = {CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR,
                                    static_cast<cl_mem_properties>(handle), 0u};
  rig.host().shared_object(0u, 0x9100u);
  rig.host().shared_object(0u, 0u);
  CHECK_EQ(rig.kmt().queued_answers(), 2u);
  rig.platform().fail_allocation_at(1u);
  cl_int error = CL_SUCCESS;
  CHECK(rig.loader().create_buffer_with_properties(opened.context(), list, CL_MEM_READ_WRITE,
                                                   buffer_bytes, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, an_external_buffer_this_driver_cannot_make_is_refused_before_the_device)
{ ClIcdRig rig;
  Opened   opened(rig, true);
  CHECK(opened.made());
  if (!opened.made()) return;
  const std::uint64_t handle = shared_handle(rig);
  const std::uint32_t sent = rig.kmt().commands();

  const cl_mem_properties refused[] = {CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KMT_KHR,
                                       CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_NAME_KHR,
                                       CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_FD_KHR,
                                       CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR};
  for (const cl_mem_properties name : refused) {
      const cl_mem_properties list[] = {name, static_cast<cl_mem_properties>(handle), 0u};
      cl_int                  error = CL_SUCCESS;
      CHECK(rig.loader().create_buffer_with_properties(opened.context(), list, CL_MEM_READ_WRITE,
                                                       buffer_bytes, &error) == nullptr);
      CHECK_EQ(error, CL_INVALID_PROPERTY);
  }

  const cl_mem_properties nowhere[] = {CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, 0x4004u, 0u};
  cl_int                  error = CL_SUCCESS;
  CHECK(rig.loader().create_buffer_with_properties(opened.context(), nowhere, CL_MEM_READ_WRITE,
                                                   buffer_bytes, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);

  const cl_mem_properties other[] = {CL_MEM_DEVICE_HANDLE_LIST_KHR, 0u, 0u};
  error = CL_SUCCESS;
  CHECK(rig.loader().create_buffer_with_properties(opened.context(), other, CL_MEM_READ_WRITE,
                                                   buffer_bytes, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PROPERTY);

  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, a_buffer_with_an_empty_property_list_is_an_ordinary_buffer)
{ ClIcdRig rig;
  Opened   opened(rig, true);
  CHECK(opened.made());
  if (!opened.made()) return;
  const cl_mem_properties empty[] = {0u};
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  cl_int              error = CL_INVALID_VALUE;
  cl_mem              buffer = rig.loader().create_buffer_with_properties(
      opened.context(), empty, CL_MEM_READ_WRITE, buffer_bytes, &error);
  CHECK(buffer != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(sent), static_cast<std::uint32_t>(av::clproto::kCmd_clCreateBuffer));
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, an_arena_backed_buffer_is_made_over_the_arena_and_the_log_says_so)
{ ClIcdRig rig;
  Opened opened(rig, true);
  CHECK(opened.made());
  if (!opened.made()) return;
  CHECK_EQ(rig.log().holding("buffers map through the blob arena"), 2u);
  CHECK_EQ(rig.log().holding("buffers are staged"), 0u);

  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  cl_int error = CL_INVALID_VALUE;
  cl_mem buffer = rig.loader().create_buffer(opened.context(),
                                             CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
                                             buffer_bytes, &error);
  CHECK(buffer != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(sent),
           static_cast<std::uint32_t>(av::clproto::kCmd_clCreateBufferWithProperties));
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, a_map_of_an_arena_backed_buffer_reads_nothing_back)
{ ClIcdRig rig;
  Opened opened(rig, true);
  CHECK(opened.made());
  if (!opened.made()) return;
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  cl_mem buffer = rig.loader().create_buffer(opened.context(),
                                             CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
                                             buffer_bytes, nullptr);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;

  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  cl_int error = CL_INVALID_VALUE;
  void *const mapped = rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_WRITE, 0u,
                                             buffer_bytes, nullptr, &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(mapped != nullptr);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK_EQ(rig.kmt().command_id(sent),
           static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueMapBuffer));

  const std::uint32_t before_unmap = rig.kmt().commands();
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().unmap(opened.queue(), buffer, mapped, nullptr), CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), before_unmap + 1u);
  CHECK_EQ(rig.kmt().command_id(before_unmap),
           static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueUnmapMemObject));
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, without_an_arena_a_buffer_is_the_hosts_own_and_a_map_stages)
{ ClIcdRig rig;
  Opened opened(rig, false);
  CHECK(opened.made());
  if (!opened.made()) return;
  CHECK_EQ(rig.log().holding("buffers are staged"), 2u);
  CHECK_EQ(rig.log().holding("buffers map through the blob arena"), 0u);

  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  cl_mem buffer = rig.loader().create_buffer(opened.context(),
                                             CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
                                             buffer_bytes, nullptr);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;
  CHECK_EQ(rig.kmt().command_id(sent),
           static_cast<std::uint32_t>(av::clproto::kCmd_clCreateBuffer));

  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  const unsigned char filled[buffer_bytes] = {};
  rig.host().cl_bytes(CL_SUCCESS, filled, buffer_bytes);
  const std::uint32_t mapping = rig.kmt().commands();
  cl_int error = CL_INVALID_VALUE;
  void *const mapped = rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_WRITE, 0u,
                                             buffer_bytes, nullptr, &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(mapped != nullptr);
  CHECK_EQ(rig.kmt().commands(), mapping + 2u);
  CHECK_EQ(rig.kmt().command_id(mapping),
           static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueMapBuffer));
  CHECK_EQ(rig.kmt().command_id(mapping + 1u),
           static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueReadBuffer));
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, a_buffer_the_application_brought_memory_for_is_never_arena_backed)
{ ClIcdRig rig;
  Opened opened(rig, true);
  CHECK(opened.made());
  if (!opened.made()) return;
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  cl_mem buffer = rig.loader().create_buffer(
      opened.context(), CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR,
      buffer_bytes, nullptr);
  CHECK(buffer != nullptr);
  CHECK_EQ(rig.kmt().command_id(sent),
           static_cast<std::uint32_t>(av::clproto::kCmd_clCreateBuffer));
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, a_buffer_that_did_not_ask_for_host_memory_is_not_given_the_arena)
{ ClIcdRig rig;
  Opened opened(rig, true);
  CHECK(opened.made());
  if (!opened.made()) return;
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  cl_mem buffer =
      rig.loader().create_buffer(opened.context(), CL_MEM_READ_WRITE, buffer_bytes, nullptr);
  CHECK(buffer != nullptr);
  CHECK_EQ(rig.kmt().command_id(sent),
           static_cast<std::uint32_t>(av::clproto::kCmd_clCreateBuffer));
  CHECK(rig.root().invariants_hold()); }

namespace {
cl_mem buffer_on(ClIcdRig &rig, const Opened &opened, cl_mem_flags flags,
                 std::uint64_t handle = host_buffer)
{
    rig.host().cl_handle(handle, CL_SUCCESS);
    return rig.loader().create_buffer(opened.context(), flags, buffer_bytes, nullptr);
}

}

TEST(cl_buffer_paths, a_map_of_something_that_is_not_a_queue_or_a_buffer_is_refused)
{ ClIcdRig rig;
  Opened   opened(rig, true);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_mem buffer = buffer_on(rig, opened, CL_MEM_READ_WRITE);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;

  const std::uint32_t sent = rig.kmt().commands();
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().map_buffer(reinterpret_cast<cl_command_queue>(opened.context()), buffer, CL_TRUE,
                              CL_MAP_READ, 0u, buffer_bytes, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_COMMAND_QUEUE);
  error = CL_SUCCESS;
  CHECK(rig.root().map_buffer(opened.queue(), reinterpret_cast<cl_mem>(opened.queue()), CL_TRUE,
                              CL_MAP_READ, 0u, buffer_bytes, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_MEM_OBJECT);
  error = CL_SUCCESS;
  CHECK(rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_READ, 0u, 0u, nullptr,
                              &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, a_host_that_will_not_map_gives_its_error_back_and_stages_nothing)
{ ClIcdRig rig;
  Opened   opened(rig, false);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_mem buffer = buffer_on(rig, opened, CL_MEM_READ_WRITE);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;

  rig.host().cl_handle(0u, CL_MEM_OBJECT_ALLOCATION_FAILURE);
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_READ, 0u, buffer_bytes,
                              nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_MEM_OBJECT_ALLOCATION_FAILURE);

  rig.host().cl_handle(0u, CL_SUCCESS);
  error = CL_SUCCESS;
  CHECK(rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_READ, 0u, buffer_bytes,
                              nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_MAP_FAILURE);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, a_staged_map_whose_fill_fails_keeps_no_mapping)
{ ClIcdRig rig;
  Opened   opened(rig, false);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_mem buffer = buffer_on(rig, opened, CL_MEM_READ_WRITE);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;

  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  rig.host().cl_bytes(CL_OUT_OF_RESOURCES, nullptr, 0u);
  rig.host().cl_enqueued(CL_SUCCESS, 0u);
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_READ, 0u, buffer_bytes,
                              nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK(rig.root().invariants_hold());
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u),
           static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueUnmapMemObject));

  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  const unsigned char filled[buffer_bytes] = {};
  rig.host().cl_bytes(CL_SUCCESS, filled, buffer_bytes);
  error = CL_INVALID_VALUE;
  void *const mapped = rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_READ, 0u,
                                             buffer_bytes, nullptr, &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(mapped != nullptr);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, a_buffer_released_while_mapped_takes_its_mappings_with_it)
{ ClIcdRig rig;
  Opened   opened(rig, false);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_mem kept = buffer_on(rig, opened, CL_MEM_READ_WRITE);
  cl_mem released = buffer_on(rig, opened, CL_MEM_READ_WRITE, host_buffer + 1u);
  CHECK(kept != nullptr && released != nullptr);
  if (kept == nullptr || released == nullptr) return;
  const cl_mem mapped_ones[] = {released, released, kept};
  for (cl_mem buffer : mapped_ones) {
      rig.host().cl_handle(host_mapped, CL_SUCCESS);
      CHECK(rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0u, buffer_bytes,
                                  nullptr, nullptr) != nullptr);
  }
  av::clicd::GuestClContext *const context = av::clicd::ClIcdRootProbe::context(rig.root(), opened.context());
  CHECK(context != nullptr);
  if (context == nullptr) return;
  CHECK_EQ(context->mappings(), 3u);
  CHECK_EQ(rig.root().release(released, av::clicd::ClObject::Kind::mem), CL_SUCCESS);
  CHECK_EQ(context->mappings(), 1u);
  CHECK_EQ(rig.log().holding("a buffer released while still mapped"), 1u);
  CHECK_EQ(rig.log().holding("2 mappings went with it"), 1u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, a_write_invalidate_map_reads_nothing_because_it_promises_nothing)
{ ClIcdRig rig;
  Opened   opened(rig, false);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_mem buffer = buffer_on(rig, opened, CL_MEM_READ_WRITE);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;

  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  cl_int error = CL_INVALID_VALUE;
  void *const mapped = rig.root().map_buffer(opened.queue(), buffer, CL_TRUE,
                                             CL_MAP_WRITE_INVALIDATE_REGION, 0u, buffer_bytes,
                                             nullptr, &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(mapped != nullptr);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, an_unmap_names_the_pointer_the_map_answered_and_the_buffer_it_was_of)
{ ClIcdRig rig;
  Opened   opened(rig, false);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_mem first = buffer_on(rig, opened, CL_MEM_READ_WRITE);
  cl_mem second = buffer_on(rig, opened, CL_MEM_READ_WRITE, host_buffer + 1u);
  CHECK(first != nullptr && second != nullptr);
  if (first == nullptr || second == nullptr) return;

  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  const unsigned char filled[buffer_bytes] = {};
  rig.host().cl_bytes(CL_SUCCESS, filled, buffer_bytes);
  void *const mapped = rig.root().map_buffer(opened.queue(), first, CL_TRUE, CL_MAP_READ, 0u,
                                             buffer_bytes, nullptr, nullptr);
  CHECK(mapped != nullptr);
  if (mapped == nullptr) return;

  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().unmap(reinterpret_cast<cl_command_queue>(opened.context()), first, mapped,
                            nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().unmap(opened.queue(), reinterpret_cast<cl_mem>(opened.queue()), mapped,
                            nullptr),
           CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().unmap(opened.queue(), second, mapped, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().unmap(opened.queue(), first, static_cast<std::uint8_t *>(mapped) + 1u,
                            nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().unmap(opened.queue(), first, mapped, nullptr), CL_SUCCESS);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, an_unmap_the_host_refuses_writes_nothing_back)
{ ClIcdRig rig;
  Opened   opened(rig, false);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_mem buffer = buffer_on(rig, opened, CL_MEM_READ_WRITE);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;

  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  const unsigned char filled[buffer_bytes] = {};
  rig.host().cl_bytes(CL_SUCCESS, filled, buffer_bytes);
  void *const mapped = rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_WRITE, 0u,
                                             buffer_bytes, nullptr, nullptr);
  CHECK(mapped != nullptr);
  if (mapped == nullptr) return;

  rig.host().cl_result(CL_OUT_OF_RESOURCES);
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().unmap(opened.queue(), buffer, mapped, nullptr), CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK_EQ(rig.kmt().command_id(sent),
           static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueUnmapMemObject));
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, a_write_back_goes_unanswered_after_the_unmap_it_follows)
{ ClIcdRig rig;
  Opened   opened(rig, false);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_mem buffer = buffer_on(rig, opened, CL_MEM_READ_WRITE);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;

  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  const unsigned char filled[buffer_bytes] = {};
  rig.host().cl_bytes(CL_SUCCESS, filled, buffer_bytes);
  void *const mapped = rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_WRITE, 0u,
                                             buffer_bytes, nullptr, nullptr);
  CHECK(mapped != nullptr);
  if (mapped == nullptr) return;

  rig.host().cl_result(CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().unmap(opened.queue(), buffer, mapped, nullptr), CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK_EQ(rig.kmt().command_id(sent), static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueUnmapMemObject));
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold());

  rig.host().cl_enqueued(CL_SUCCESS, 0x6900u);
  rig.host().cl_result(CL_SUCCESS);
  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().finish(opened.queue()), CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(sent + 1u), static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueWriteBuffer));
  CHECK_EQ(rig.kmt().command_id(sent + 2u),
           static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueMarkerWithWaitList));
  CHECK_EQ(rig.kmt().queued_answers(), 0u); }

TEST(cl_buffer_paths, a_buffer_over_the_applications_memory_maps_to_that_memory)
{ ClIcdRig rig;
  Opened   opened(rig, false);
  CHECK(opened.made());
  if (!opened.made()) return;
  static unsigned char own[buffer_bytes];
  std::memset(own, 0x5a, sizeof own);
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  const std::uint32_t created = rig.kmt().commands();
  cl_mem buffer = rig.root().create_buffer(opened.context(), CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, buffer_bytes,
                                           own, nullptr, nullptr);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;
  CHECK_EQ(rig.kmt().command_id(created), static_cast<std::uint32_t>(av::clproto::kCmd_clCreateBuffer));

  void *host_pointer = nullptr;
  CHECK_EQ(rig.root().mem_object_info(buffer, CL_MEM_HOST_PTR, sizeof host_pointer, &host_pointer, nullptr),
           CL_SUCCESS);
  CHECK(host_pointer == own);
  const cl_mem_flags host_flags = CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&host_flags), sizeof host_flags);
  cl_mem_flags flags = 0u;
  CHECK_EQ(rig.root().mem_object_info(buffer, CL_MEM_FLAGS, sizeof flags, &flags, nullptr), CL_SUCCESS);
  CHECK_EQ(flags, static_cast<cl_mem_flags>(CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR));
  const std::uint32_t asked = rig.kmt().commands();
  flags = 0u;
  CHECK_EQ(rig.root().mem_object_info(buffer, CL_MEM_FLAGS, sizeof flags, &flags, nullptr), CL_SUCCESS);
  CHECK_EQ(flags, static_cast<cl_mem_flags>(CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR));
  CHECK_EQ(rig.kmt().commands(), asked);

  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  unsigned char device[32];
  std::memset(device, 0xc3, sizeof device);
  rig.host().cl_bytes(CL_SUCCESS, device, sizeof device);
  cl_int error = CL_INVALID_VALUE;
  unsigned char *const mapped = static_cast<unsigned char *>(rig.root().map_buffer(
      opened.queue(), buffer, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 16u, sizeof device, nullptr, &error));
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(mapped == own + 16);
  CHECK_EQ(own[16], 0xc3u);
  CHECK_EQ(own[16 + sizeof device - 1u], 0xc3u);
  CHECK_EQ(own[15], 0x5au);
  CHECK_EQ(own[16 + sizeof device], 0x5au);
  if (mapped == nullptr) return;
  mapped[4] = 0x01u;

  rig.host().cl_result(CL_SUCCESS);
  const std::uint32_t unmapping = rig.kmt().commands();
  CHECK_EQ(rig.root().unmap(opened.queue(), buffer, mapped, nullptr), CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(unmapping), static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueUnmapMemObject));
  CHECK_EQ(own[20], 0x01u);
  rig.host().cl_enqueued(CL_SUCCESS, 0x6900u);
  rig.host().cl_result(CL_SUCCESS);
  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().finish(opened.queue()), CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(unmapping + 1u), static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueWriteBuffer));
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, a_buffer_over_no_memory_or_over_memory_it_also_copies_is_refused)
{ ClIcdRig rig;
  Opened   opened(rig, false);
  CHECK(opened.made());
  if (!opened.made()) return;
  static unsigned char own[buffer_bytes];
  const std::uint32_t sent = rig.kmt().commands();
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().create_buffer(opened.context(), CL_MEM_USE_HOST_PTR, buffer_bytes, nullptr, nullptr, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_HOST_PTR);
  CHECK(rig.root().create_buffer(opened.context(), CL_MEM_USE_HOST_PTR | CL_MEM_COPY_HOST_PTR, buffer_bytes, own,
                                 nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK(rig.root().create_buffer(opened.context(), CL_MEM_USE_HOST_PTR | CL_MEM_ALLOC_HOST_PTR, buffer_bytes, own,
                                 nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent); }

TEST(cl_buffer_paths, an_arena_backed_map_the_host_refuses_hands_back_nothing)
{ ClIcdRig rig;
  Opened   opened(rig, true);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_mem buffer = buffer_on(rig, opened, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;

  rig.host().cl_handle(0u, CL_MEM_OBJECT_ALLOCATION_FAILURE);
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_READ, 0u, buffer_bytes,
                              nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_MEM_OBJECT_ALLOCATION_FAILURE);

  rig.host().cl_handle(0u, CL_SUCCESS);
  error = CL_SUCCESS;
  CHECK(rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_READ, 0u, buffer_bytes,
                              nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_MAP_FAILURE);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, a_map_past_the_end_of_its_backing_is_staged_rather_than_shared)
{ ClIcdRig rig;
  Opened   opened(rig, true);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_mem buffer = buffer_on(rig, opened, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;

  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  const unsigned char filled[buffer_bytes] = {};
  rig.host().cl_bytes(CL_SUCCESS, filled, buffer_bytes);
  const std::uint32_t sent = rig.kmt().commands();
  cl_int error = CL_INVALID_VALUE;
  void *const mapped = rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_READ,
                                             buffer_bytes - 8u, buffer_bytes, nullptr, &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(mapped != nullptr);
  CHECK_EQ(rig.kmt().commands(), sent + 2u);
  CHECK_EQ(rig.kmt().command_id(sent + 1u),
           static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueReadBuffer));
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, a_map_with_more_mappings_than_the_context_holds_is_refused_on_both_paths)
{ ClIcdRig rig;
  Opened   opened(rig, true);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_mem buffer = buffer_on(rig, opened, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;

  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  CHECK(rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_READ, 0u, 1u, nullptr, nullptr) != nullptr);
  CHECK(fill_mappings(rig, opened.context()));
  rig.host().cl_handle(host_mapped + 1u, CL_SUCCESS);
  rig.host().cl_enqueued(CL_SUCCESS, 0u);
  const std::uint32_t sent = rig.kmt().commands();
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_READ, 0u, 1u, nullptr,
                              &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);
  CHECK_EQ(rig.kmt().command_id(sent + 1u), static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueUnmapMemObject));
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, a_map_asked_for_an_event_hands_one_back_on_either_path)
{ { ClIcdRig rig;
    Opened   opened(rig, true);
    CHECK(opened.made());
    if (!opened.made()) return;
    cl_mem buffer = buffer_on(rig, opened, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR);
    CHECK(buffer != nullptr);
    if (buffer == nullptr) return;
    rig.host().cl_mapped(host_mapped, 0x6900u, CL_SUCCESS);
    cl_event    event = nullptr;
    cl_int      error = CL_INVALID_VALUE;
    void *const mapped = rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_READ, 0u,
                                               buffer_bytes, &event, &error);
    CHECK_EQ(error, CL_SUCCESS);
    CHECK(mapped != nullptr);
    CHECK(event != nullptr);
    rig.host().cl_enqueued(CL_SUCCESS, 0x6901u);
    cl_event given_back = nullptr;
    CHECK_EQ(rig.root().unmap(opened.queue(), buffer, mapped, &given_back), CL_SUCCESS);
    CHECK(given_back != nullptr);
    CHECK(rig.root().invariants_hold()); }
  { ClIcdRig rig;
    Opened   opened(rig, false);
    CHECK(opened.made());
    if (!opened.made()) return;
    cl_mem buffer = buffer_on(rig, opened, CL_MEM_READ_WRITE);
    CHECK(buffer != nullptr);
    if (buffer == nullptr) return;
    rig.host().cl_mapped(host_mapped, 0x6900u, CL_SUCCESS);
    const unsigned char filled[buffer_bytes] = {};
    rig.host().cl_bytes(CL_SUCCESS, filled, buffer_bytes);
    cl_event    event = nullptr;
    cl_int      error = CL_INVALID_VALUE;
    void *const mapped = rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_WRITE, 0u,
                                               buffer_bytes, &event, &error);
    CHECK_EQ(error, CL_SUCCESS);
    CHECK(mapped != nullptr);
    CHECK(event != nullptr);
    rig.host().cl_enqueued(CL_SUCCESS, 0x6901u);
    cl_event given_back = nullptr;
    CHECK_EQ(rig.root().unmap(opened.queue(), buffer, mapped, &given_back), CL_SUCCESS);
    CHECK(given_back != nullptr);
    CHECK(rig.root().invariants_hold()); } }

TEST(cl_buffer_paths, an_unmap_of_a_live_mapping_on_a_lost_context_refuses)
{ ClIcdRig rig;
  Opened   opened(rig, false);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_mem buffer = buffer_on(rig, opened, CL_MEM_READ_WRITE);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;
  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  const unsigned char filled[buffer_bytes] = {};
  rig.host().cl_bytes(CL_SUCCESS, filled, buffer_bytes);
  void *const mapped = rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_WRITE, 0u,
                                             buffer_bytes, nullptr, nullptr);
  CHECK(mapped != nullptr);
  if (mapped == nullptr) return;

  rig.kmt().withhold_answer_at(1u);
  CHECK_EQ(rig.root().flush(opened.queue()), CL_OUT_OF_RESOURCES);
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().unmap(opened.queue(), buffer, mapped, nullptr), CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, a_map_beginning_past_its_backing_is_staged)
{ ClIcdRig rig;
  Opened   opened(rig, true);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_mem buffer = buffer_on(rig, opened, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;

  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  const unsigned char filled[8] = {};
  rig.host().cl_bytes(CL_SUCCESS, filled, sizeof filled);
  const std::uint32_t sent = rig.kmt().commands();
  cl_int error = CL_INVALID_VALUE;
  void *const mapped = rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_READ,
                                             buffer_bytes + 8u, sizeof filled, nullptr, &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(mapped != nullptr);
  CHECK_EQ(rig.kmt().commands(), sent + 2u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_buffer_paths, a_staged_map_with_no_row_left_gives_its_block_back)
{ ClIcdRig rig;
  Opened   opened(rig, false);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_mem buffer = buffer_on(rig, opened, CL_MEM_READ_WRITE);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;

  CHECK(av::clicd::GuestClContext::max_mappings > 64u);
  CHECK(fill_mappings(rig, opened.context()));
  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  rig.host().cl_enqueued(CL_SUCCESS, 0u);
  const std::uint32_t sent = rig.kmt().commands();
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().map_buffer(opened.queue(), buffer, CL_TRUE, CL_MAP_READ, 0u, 1u, nullptr,
                              &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);
  CHECK_EQ(rig.kmt().commands(), sent + 2u);
  CHECK_EQ(rig.kmt().command_id(sent + 1u), static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueUnmapMemObject));
  CHECK(rig.root().invariants_hold()); }
