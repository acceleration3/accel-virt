#include "harness/framework/test_registry.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "harness/icd/fake_cl_loader.hpp"
#include "harness/icd/recording_log.hpp"
#include "icd/opencl/cl_icd_root.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"

#include <CL/cl_ext.h>
#include <cstdint>
#include <cstring>

using av::ClIcdRig;
using av::FakeClLoader;
using av::clicd::ClObject;

namespace av::clicd {
struct ClIcdRootProbe {
    static ClObject *without_a_context(ClIcdRoot &root, ClObject::Kind kind, std::uint64_t wire)
    { ClObject *made = nullptr;
      return root.objects_.make(kind, wire, nullptr, nullptr, made) == av::Status::ok ? made
                                                                                      : nullptr; }
};

}

namespace {

constexpr std::uint64_t host_platform = 0x6200u;
constexpr std::uint64_t host_device = 0x6300u;

cl_platform_id this_platform(ClIcdRig &rig)
{
    cl_platform_id platform = nullptr;
    cl_uint        count = 0u;
    CHECK_EQ(rig.root().platform_ids(1u, &platform, &count), CL_SUCCESS);
    CHECK_EQ(count, 1u);
    return platform;
}

}

TEST(cl_icd_root_refusals, a_platform_this_driver_does_not_own_is_refused)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  cl_device_id device = nullptr;
  cl_uint      count = 0xEEu;
  cl_platform_id stranger = reinterpret_cast<cl_platform_id>(std::uintptr_t{0x1234});
  CHECK_EQ(rig.root().device_ids(stranger, CL_DEVICE_TYPE_GPU, 1u, &device, &count),
           CL_INVALID_PLATFORM);
  CHECK(device == nullptr);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_query_with_neither_an_array_nor_a_count_asks_for_nothing)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  cl_platform_id platform = this_platform(rig);
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_GPU, 1u, nullptr, nullptr),
           CL_INVALID_VALUE);
  cl_device_id device = nullptr;
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_GPU, 0u, &device, nullptr),
           CL_INVALID_VALUE);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_type_naming_no_kind_of_device_is_an_invalid_type)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  cl_platform_id platform = this_platform(rig);
  cl_uint        count = 0xEEu;
  CHECK_EQ(rig.root().device_ids(platform, 0u, 0u, nullptr, &count), CL_INVALID_DEVICE_TYPE);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_type_this_adapter_has_no_device_of_finds_none)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  cl_platform_id platform = this_platform(rig);
  cl_uint        count = 0xEEu;
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_CPU, 0u, nullptr, &count),
           CL_DEVICE_NOT_FOUND);
  CHECK_EQ(count, 0u);
  count = 0xEEu;
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_ACCELERATOR, 0u, nullptr, &count),
           CL_DEVICE_NOT_FOUND);
  CHECK_EQ(count, 0u);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, every_type_that_includes_this_gpu_finds_it)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  cl_platform_id platform = this_platform(rig);
  CHECK((CL_DEVICE_TYPE_ALL & CL_DEVICE_TYPE_GPU) != 0u);
  const cl_device_type accepted[] = {CL_DEVICE_TYPE_GPU, CL_DEVICE_TYPE_DEFAULT,
                                     CL_DEVICE_TYPE_ALL,
                                     CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_CPU};
  bool first = true;
  for (const cl_device_type type : accepted) {
      if (first) {
          rig.host().blob_import(0u, 0x77u);
          rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
          rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
          rig.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
          rig.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
          rig.host().cl_info(CL_SUCCESS, "", 1u);
          first = false;
      }
      cl_device_id device = nullptr;
      cl_uint      count = 0u;
      CHECK_EQ(rig.root().device_ids(platform, type, 1u, &device, &count), CL_SUCCESS);
      CHECK_EQ(count, 1u);
      CHECK(device != nullptr);
  }
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_host_that_lists_no_platform_opens_no_device)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  cl_platform_id platform = this_platform(rig);
  rig.host().blob_import(0u, 0x77u);
  rig.host().cl_ids(CL_INVALID_VALUE, nullptr, 0u, 0u);
  cl_device_id device = nullptr;
  cl_uint      count = 0xEEu;
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_GPU, 1u, &device, &count),
           CL_DEVICE_NOT_FOUND);
  CHECK_EQ(count, 0u);
  CHECK(device == nullptr);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_host_that_lists_a_platform_but_no_device_opens_none)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  cl_platform_id platform = this_platform(rig);
  rig.host().blob_import(0u, 0x77u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_INVALID_VALUE, nullptr, 0u, 0u);
  cl_uint count = 0xEEu;
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_GPU, 0u, nullptr, &count),
           CL_DEVICE_NOT_FOUND);
  CHECK_EQ(count, 0u);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_host_that_answers_a_count_of_none_opens_no_device)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  cl_platform_id platform = this_platform(rig);
  rig.host().blob_import(0u, 0x77u);
  rig.host().cl_ids(CL_SUCCESS, nullptr, 0u, 0u);
  cl_uint count = 0xEEu;
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_GPU, 0u, nullptr, &count),
           CL_DEVICE_NOT_FOUND);
  CHECK_EQ(count, 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_host_that_names_a_platform_of_zero_opens_no_device)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  cl_platform_id platform = this_platform(rig);
  const std::uint64_t nothing = 0u;
  rig.host().blob_import(0u, 0x77u);
  rig.host().cl_ids(CL_SUCCESS, &nothing, 1u, 1u);
  cl_uint count = 0xEEu;
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_GPU, 0u, nullptr, &count),
           CL_DEVICE_NOT_FOUND);
  CHECK_EQ(count, 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_host_that_names_a_device_of_zero_opens_no_device)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  cl_platform_id platform = this_platform(rig);
  const std::uint64_t nothing = 0u;
  rig.host().blob_import(0u, 0x77u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &nothing, 1u, 1u);
  cl_uint count = 0xEEu;
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_GPU, 0u, nullptr, &count),
           CL_DEVICE_NOT_FOUND);
  CHECK_EQ(count, 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_host_that_will_not_say_what_it_is_still_opens_the_device)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  cl_platform_id platform = this_platform(rig);
  rig.host().blob_import(0u, 0x77u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_info(CL_INVALID_VALUE, nullptr, 0u);
  rig.host().cl_info(CL_INVALID_VALUE, nullptr, 0u);
  rig.host().cl_info(CL_INVALID_VALUE, nullptr, 0u);
  cl_device_id device = nullptr;
  cl_uint      count = 0u;
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_GPU, 1u, &device, &count), CL_SUCCESS);
  CHECK_EQ(count, 1u);
  CHECK(device != nullptr);
  cl_device_svm_capabilities svm = 0xEEu;
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_SVM_CAPABILITIES, sizeof svm, &svm, nullptr),
           CL_SUCCESS);
  CHECK_EQ(svm, static_cast<cl_device_svm_capabilities>(0u));
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, the_extensions_the_loader_asks_for_need_room_for_the_terminator)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  cl_platform_id platform = this_platform(rig);
  static const char expected[] = "cl_khr_d3d10_sharing cl_khr_d3d11_sharing";
  char        short_of[sizeof expected - 1u];
  std::memset(short_of, 'x', sizeof short_of);
  std::size_t bytes = 0u;
  CHECK_EQ(rig.root().platform_info(platform, CL_PLATFORM_EXTENSIONS, 0u, nullptr, &bytes), CL_SUCCESS);
  CHECK_EQ(bytes, sizeof expected);
  CHECK_EQ(rig.root().platform_info(platform, CL_PLATFORM_EXTENSIONS, sizeof short_of, short_of, &bytes),
           CL_INVALID_VALUE);
  CHECK_EQ(short_of[0], 'x');
  char listed[sizeof expected] = {};
  CHECK_EQ(rig.root().platform_info(platform, CL_PLATFORM_EXTENSIONS, sizeof listed, listed, &bytes), CL_SUCCESS);
  CHECK(std::strcmp(listed, expected) == 0);
  CHECK(rig.root().invariants_hold()); }

namespace {
class Opened {
public:
    explicit Opened(ClIcdRig &rig) : rig_(rig), device_(nullptr), context_(nullptr)
    {
        if (rig_.loader().load() != FakeClLoader::Load::kept) return;
        rig_.host().blob_import(0u, 0x77u);
        rig_.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
        rig_.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
        rig_.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
        rig_.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
        rig_.host().cl_info(CL_SUCCESS, "", 1u);
        cl_uint devices = 0u;
        if (rig_.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device_, &devices) != CL_SUCCESS)
            return;
        rig_.host().blob_import(0u, 0x78u);
        rig_.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
        rig_.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
        rig_.host().cl_handle(0x6400u, CL_SUCCESS);
        context_ = rig_.loader().create_context(device_, nullptr);
    }

    bool         made() const { return device_ != nullptr && context_ != nullptr; }
    cl_device_id device() const { return device_; }
    cl_context   context() const { return context_; }

private:
    ClIcdRig    &rig_;
    cl_device_id device_;
    cl_context   context_;
};

}

TEST(cl_icd_root_refusals, a_host_that_will_not_make_a_queue_makes_no_guest_object)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  rig.host().cl_handle(0u, CL_OUT_OF_HOST_MEMORY);
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().create_queue(opened.context(), opened.device(), nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);

  rig.host().cl_handle(0u, CL_SUCCESS);
  error = CL_SUCCESS;
  CHECK(rig.root().create_queue(opened.context(), opened.device(), nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_host_that_will_not_make_a_buffer_or_a_program_gives_its_error_back)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;

  rig.host().cl_handle(0u, CL_INVALID_BUFFER_SIZE);
  cl_int error = CL_SUCCESS;
  CHECK(rig.loader().create_buffer(opened.context(), CL_MEM_READ_WRITE, 256u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_BUFFER_SIZE);

  rig.host().cl_handle(0u, CL_SUCCESS);
  error = CL_SUCCESS;
  CHECK(rig.loader().create_buffer(opened.context(), CL_MEM_READ_WRITE, 256u, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);

  rig.host().cl_handle(0u, CL_OUT_OF_HOST_MEMORY);
  error = CL_SUCCESS;
  CHECK(rig.loader().create_program(opened.context(), "kernel void k(){}", &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);

  rig.host().cl_handle(0u, CL_SUCCESS);
  error = CL_SUCCESS;
  CHECK(rig.loader().create_program(opened.context(), "kernel void k(){}", &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_program_made_of_nothing_is_the_callers_mistake)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_int      error = CL_SUCCESS;
  const char *source = "kernel void k(){}";
  const std::uint32_t sent = rig.kmt().commands();
  CHECK(rig.root().create_program(opened.context(), 0u, &source, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  error = CL_SUCCESS;
  CHECK(rig.root().create_program(opened.context(), 1u, nullptr, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_context_made_on_no_device_or_several_is_refused)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_device_id devices[2] = {opened.device(), opened.device()};
  cl_int       error = CL_SUCCESS;
  CHECK(rig.root().create_context(nullptr, nullptr, 0u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  error = CL_SUCCESS;
  CHECK(rig.root().create_context(nullptr, devices, 0u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  error = CL_SUCCESS;
  CHECK(rig.root().create_context(nullptr, devices, 2u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_DEVICE);
  cl_device_id stranger = reinterpret_cast<cl_device_id>(std::uintptr_t{0x1234});
  error = CL_SUCCESS;
  CHECK(rig.root().create_context(nullptr, &stranger, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_DEVICE);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, an_event_of_nothing_is_kept_by_nobody_and_released_by_nobody)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  rig.host().cl_handle(0x6500u, CL_SUCCESS);
  cl_command_queue queue = rig.root().create_queue(opened.context(), opened.device(), nullptr,
                                                   nullptr);
  CHECK(queue != nullptr);
  if (queue == nullptr) return;
  rig.host().cl_handle(0x6600u, CL_SUCCESS);
  cl_mem buffer = rig.loader().create_buffer(opened.context(), CL_MEM_READ_WRITE, 256u, nullptr);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;

  unsigned char from[8] = {};
  rig.host().cl_enqueued(CL_SUCCESS, 0u);
  const std::uint32_t before = rig.kmt().commands();
  cl_event            event = nullptr;
  CHECK_EQ(rig.root().enqueue_write(queue, buffer, CL_FALSE, 0u, sizeof from, from, &event),
           CL_SUCCESS);
  CHECK(event == nullptr);
  CHECK_EQ(rig.kmt().commands(), before + 2u);
  CHECK_EQ(rig.kmt().command_id(before),
           static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueWriteBuffer));
  CHECK_EQ(rig.kmt().command_id(before + 1u),
           static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueMarkerWithWaitList));
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, every_kind_names_its_own_refusal)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  void *const context = opened.context();
  CHECK_EQ(rig.root().release(context, ClObject::Kind::platform), CL_INVALID_PLATFORM);
  CHECK_EQ(rig.root().release(context, ClObject::Kind::device), CL_INVALID_DEVICE);
  CHECK_EQ(rig.root().release(context, ClObject::Kind::sampler), CL_INVALID_SAMPLER);
  CHECK_EQ(rig.root().release(context, ClObject::Kind::command_queue), CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().release(context, ClObject::Kind::program), CL_INVALID_PROGRAM);
  CHECK_EQ(rig.root().release(context, ClObject::Kind::mem), CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().release(context, ClObject::Kind::kernel), CL_INVALID_KERNEL);
  CHECK_EQ(rig.root().release(context, ClObject::Kind::event), CL_INVALID_EVENT);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_context_on_another_vendors_platform_is_refused)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  const cl_context_properties stranger[] = {CL_CONTEXT_PLATFORM, 0x1234, 0};
  cl_device_id        device = opened.device();
  cl_int              error = CL_SUCCESS;
  const std::uint32_t sent = rig.kmt().commands();
  CHECK(rig.root().create_context(stranger, &device, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PLATFORM);
  CHECK_EQ(rig.kmt().commands(), sent);

  cl_platform_id      mine = this_platform(rig);
  const cl_context_properties ours[] = {CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(mine), 0};
  rig.host().blob_import(0u, 0x79u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(0x6402u, CL_SUCCESS);
  error = CL_INVALID_VALUE;
  CHECK(rig.root().create_context(ours, &device, 1u, &error) != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_context_whose_own_transport_cannot_be_made_is_not_available)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_device_id device = opened.device();
  cl_int       error = CL_SUCCESS;
  rig.kmt().withhold_answer_at(1u);
  CHECK(rig.root().create_context(nullptr, &device, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_DEVICE_NOT_AVAILABLE);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_host_that_will_not_make_the_context_takes_the_guests_with_it)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_device_id device = opened.device();

  rig.host().blob_import(0u, 0x7Au);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(0u, CL_OUT_OF_HOST_MEMORY);
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().create_context(nullptr, &device, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);

  rig.host().blob_import(0u, 0x7Bu);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(0u, CL_SUCCESS);
  error = CL_SUCCESS;
  CHECK(rig.root().create_context(nullptr, &device, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_DEVICE_NOT_AVAILABLE);

  rig.host().blob_import(0u, 0x7Cu);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(0x6403u, CL_SUCCESS);
  CHECK(rig.root().create_context(nullptr, &device, 1u, nullptr) != nullptr);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_host_that_will_not_list_its_own_platform_makes_no_context)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_device_id device = opened.device();

  rig.host().blob_import(0u, 0x7Du);
  rig.host().cl_ids(CL_INVALID_VALUE, nullptr, 0u, 0u);
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().create_context(nullptr, &device, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_DEVICE_NOT_AVAILABLE);

  rig.host().blob_import(0u, 0x7Eu);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_INVALID_VALUE, nullptr, 0u, 0u);
  error = CL_SUCCESS;
  CHECK(rig.root().create_context(nullptr, &device, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_DEVICE_NOT_AVAILABLE);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, the_two_call_shape_writes_only_what_it_was_asked_for)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  cl_platform_id platform = this_platform(rig);
  rig.host().blob_import(0u, 0x77u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
  rig.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
  rig.host().cl_info(CL_SUCCESS, "", 1u);

  cl_uint count = 0xEEu;
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_GPU, 0u, nullptr, &count), CL_SUCCESS);
  CHECK_EQ(count, 1u);

  cl_device_id devices[2] = {nullptr, reinterpret_cast<cl_device_id>(std::uintptr_t{0xEE})};
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_GPU, 2u, devices, nullptr), CL_SUCCESS);
  CHECK(devices[0] != nullptr);
  CHECK(devices[1] == reinterpret_cast<cl_device_id>(std::uintptr_t{0xEE}));

  count = 0xEEu;
  devices[0] = nullptr;
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_GPU, 1u, devices, &count), CL_SUCCESS);
  CHECK_EQ(count, 1u);
  CHECK(devices[0] != nullptr);

  devices[0] = reinterpret_cast<cl_device_id>(std::uintptr_t{0xAA});
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_CPU, 1u, devices, nullptr),
           CL_DEVICE_NOT_FOUND);
  CHECK(devices[0] == reinterpret_cast<cl_device_id>(std::uintptr_t{0xAA}));
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_buffer_the_device_will_not_back_falls_back_to_the_ordinary_one)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;

  rig.host().cl_handle(0u, CL_INVALID_VALUE);
  rig.host().cl_handle(0x6600u, CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  cl_int error = CL_INVALID_VALUE;
  cl_mem buffer = rig.loader().create_buffer(opened.context(),
                                             CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, 256u,
                                             &error);
  CHECK(buffer != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 2u);
  CHECK_EQ(rig.kmt().command_id(sent),
           static_cast<std::uint32_t>(av::clproto::kCmd_clCreateBufferWithProperties));
  CHECK_EQ(rig.kmt().command_id(sent + 1u),
           static_cast<std::uint32_t>(av::clproto::kCmd_clCreateBuffer));
  CHECK(rig.root().invariants_hold());

  rig.host().cl_handle(0u, CL_INVALID_VALUE);
  rig.host().cl_handle(0u, CL_MEM_OBJECT_ALLOCATION_FAILURE);
  error = CL_SUCCESS;
  CHECK(rig.loader().create_buffer(opened.context(), CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
                                   256u, &error) == nullptr);
  CHECK_EQ(error, CL_MEM_OBJECT_ALLOCATION_FAILURE);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_host_that_counts_no_devices_on_its_platform_opens_none)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  cl_platform_id platform = this_platform(rig);
  rig.host().blob_import(0u, 0x77u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, nullptr, 0u, 0u);
  cl_uint count = 0xEEu;
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_GPU, 0u, nullptr, &count),
           CL_DEVICE_NOT_FOUND);
  CHECK_EQ(count, 0u);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_guest_with_no_memory_for_the_open_shows_no_device)
{ bool proved_context = false;
  bool proved_object = false;
  for (std::uint32_t refused = 1u; refused <= 24u; ++refused) {
      ClIcdRig rig;
      if (rig.loader().load() != FakeClLoader::Load::kept) continue;
      cl_platform_id platform = this_platform(rig);
      rig.host().blob_import(0u, 0x77u);
      rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
      rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
      rig.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
      rig.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
      rig.host().cl_info(CL_SUCCESS, "", 1u);
      rig.platform().fail_allocation_at(refused);
      cl_device_id device = nullptr;
      cl_uint      count = 0xEEu;
      const cl_int answered = rig.root().device_ids(platform, CL_DEVICE_TYPE_GPU, 1u, &device,
                                                    &count);
      if (answered == CL_SUCCESS) continue;
      CHECK_EQ(answered, CL_DEVICE_NOT_FOUND);
      CHECK_EQ(count, 0u);
      CHECK(device == nullptr);
      CHECK(rig.root().invariants_hold());
      if (rig.kmt().commands() == 0u) proved_context = true;
      else proved_object = true;
  }
  CHECK(proved_context);
  CHECK(proved_object); }

TEST(cl_icd_root_refusals, each_entry_names_the_object_that_was_wrong)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  void *const not_an_object = opened.context();
  const std::uint32_t sent = rig.kmt().commands();
  cl_int error = CL_SUCCESS;

  CHECK(rig.root().create_queue(reinterpret_cast<cl_context>(opened.device()), opened.device(),
                                nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  error = CL_SUCCESS;
  CHECK(rig.root().create_queue(opened.context(), reinterpret_cast<cl_device_id>(not_an_object),
                                nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_DEVICE);

  error = CL_SUCCESS;
  CHECK(rig.loader().create_buffer(reinterpret_cast<cl_context>(opened.device()),
                                   CL_MEM_READ_WRITE, 256u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  error = CL_SUCCESS;
  CHECK(rig.loader().create_program(reinterpret_cast<cl_context>(opened.device()), "k", &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);

  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_device_query_the_host_owns_needs_a_transport_to_ask_over)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  char        name[64] = {};
  std::size_t bytes = 0u;
  rig.kmt().withhold_answer_at(1u);
  CHECK_EQ(rig.loader().device_info(opened.device(), CL_DEVICE_NAME, sizeof name, name, &bytes),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.loader().device_info(opened.device(), CL_DEVICE_NAME, sizeof name, name, &bytes),
           CL_INVALID_DEVICE);
  cl_uint luid_valid = 0u;
  CHECK_EQ(rig.loader().device_info(opened.device(), CL_DEVICE_LUID_VALID_KHR, sizeof luid_valid,
                                    &luid_valid, nullptr),
           CL_SUCCESS);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_platform_property_of_nothing_is_no_preference_at_all)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  const cl_context_properties none[] = {CL_CONTEXT_PLATFORM, 0, 0};
  cl_device_id        device = opened.device();
  rig.host().blob_import(0u, 0x7Fu);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(0x6404u, CL_SUCCESS);
  cl_int error = CL_INVALID_VALUE;
  CHECK(rig.root().create_context(none, &device, 1u, &error) != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, an_open_that_fails_writes_no_count_it_was_not_given)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  cl_platform_id platform = this_platform(rig);
  rig.host().blob_import(0u, 0x77u);
  rig.host().cl_ids(CL_INVALID_VALUE, nullptr, 0u, 0u);
  cl_device_id devices[1] = {reinterpret_cast<cl_device_id>(std::uintptr_t{0xAA})};
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_GPU, 1u, devices, nullptr),
           CL_DEVICE_NOT_FOUND);
  CHECK(devices[0] == reinterpret_cast<cl_device_id>(std::uintptr_t{0xAA}));
  CHECK(rig.root().invariants_hold()); }

namespace {
bool back_one(ClIcdRig &rig, const Opened &opened)
{
    rig.host().cl_handle(0x6500u, CL_SUCCESS);
    return rig.loader().create_buffer(opened.context(), CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, 256u, nullptr) !=
           nullptr;
}

std::uint32_t buffers_until_the_table_grows(ClIcdRig &rig, const Opened &opened)
{
    for (std::uint32_t made = 0u; made < 200u; ++made) {
        const std::uint32_t before = rig.platform().live_allocations();
        rig.host().cl_handle(0x7000u + made, CL_SUCCESS);
        if (rig.loader().create_buffer(opened.context(), CL_MEM_READ_WRITE, 256u, nullptr) ==
            nullptr)
            return 0u;
        if (rig.platform().live_allocations() != before) return made;
    }
    return 0u;
}

}

TEST(cl_icd_root_refusals, an_arena_backed_buffer_the_guest_cannot_keep_gives_its_block_back)
{ std::uint32_t until = 0u;
  { ClIcdRig rig;
    Opened   opened(rig);
    CHECK(opened.made());
    if (!opened.made()) return;
    CHECK(back_one(rig, opened));
    until = buffers_until_the_table_grows(rig, opened);
    CHECK(until != 0u); }
  if (until == 0u) return;

  ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  CHECK(back_one(rig, opened));
  for (std::uint32_t made = 0u; made < until; ++made) {
      rig.host().cl_handle(0x7000u + made, CL_SUCCESS);
      CHECK(rig.loader().create_buffer(opened.context(), CL_MEM_READ_WRITE, 256u, nullptr) !=
            nullptr);
  }
  rig.host().cl_handle(0x6600u, CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  rig.platform().fail_allocation_at(1u);
  cl_int error = CL_SUCCESS;
  CHECK(rig.loader().create_buffer(opened.context(), CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
                                   256u, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);
  CHECK(rig.root().invariants_hold());

  rig.host().cl_handle(0x6601u, CL_SUCCESS);
  CHECK(rig.loader().create_buffer(opened.context(), CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
                                   256u, nullptr) != nullptr);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_context_the_guest_cannot_keep_gives_the_hosts_back)
{ std::uint32_t until = 0u;
  { ClIcdRig rig;
    Opened   opened(rig);
    CHECK(opened.made());
    if (!opened.made()) return;
    until = buffers_until_the_table_grows(rig, opened);
    CHECK(until != 0u); }
  if (until == 0u) return;

  ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  for (std::uint32_t made = 0u; made < until; ++made) {
      rig.host().cl_handle(0x7000u + made, CL_SUCCESS);
      CHECK(rig.loader().create_buffer(opened.context(), CL_MEM_READ_WRITE, 256u, nullptr) !=
            nullptr);
  }
  cl_device_id device = opened.device();
  bool proved = false;
  for (std::uint32_t refused = 1u; refused <= 8u && !proved; ++refused) {
      rig.host().blob_import(0u, 0x80u + refused);
      rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
      rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
      rig.host().cl_handle(0x6410u + refused, CL_SUCCESS);
      rig.host().cl_result(CL_SUCCESS);
      const std::uint32_t before = rig.kmt().commands();
      rig.platform().fail_allocation_at(refused);
      cl_int error = CL_SUCCESS;
      if (rig.root().create_context(nullptr, &device, 1u, &error) != nullptr) continue;
      CHECK(error == CL_OUT_OF_HOST_MEMORY || error == CL_DEVICE_NOT_AVAILABLE);
      if (error == CL_OUT_OF_HOST_MEMORY && rig.kmt().commands() > before + 3u) proved = true;
      CHECK(rig.root().invariants_hold());
  }
  CHECK(proved); }

TEST(cl_icd_root_refusals, an_external_acquire_on_a_queue_with_no_context_is_refused)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  ClObject *const queue =
      av::clicd::ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::command_queue,
                                                   0x6500u);
  CHECK(queue != nullptr);
  if (queue == nullptr) return;
  const std::uint32_t sent = rig.kmt().commands();
  cl_mem              named = nullptr;
  CHECK_EQ(rig.root().enqueue_external(reinterpret_cast<cl_command_queue>(queue), 1u, &named,
                                       nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_root_with_no_memory_for_its_first_context_makes_none)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  rig.host().blob_import(0u, 0x77u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
  rig.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
  rig.host().cl_info(CL_SUCCESS, "", 1u);
  cl_device_id device = nullptr;
  cl_uint      devices = 0u;
  CHECK_EQ(rig.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device, &devices), CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  rig.platform().fail_allocation_at(1u);
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().create_context(nullptr, &device, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_device_no_adapter_opened_makes_no_context)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  ClObject *const device =
      av::clicd::ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::device, 0x6300u);
  CHECK(device != nullptr);
  if (device == nullptr) return;
  auto *const         as_device = reinterpret_cast<cl_device_id>(device);
  const std::uint32_t sent = rig.kmt().commands();
  cl_int              error = CL_SUCCESS;
  CHECK(rig.root().create_context(nullptr, &as_device, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_DEVICE);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_plain_buffer_the_guest_cannot_keep_has_no_block_to_give_back)
{ std::uint32_t until = 0u;
  { ClIcdRig rig;
    Opened   opened(rig);
    CHECK(opened.made());
    if (!opened.made()) return;
    until = buffers_until_the_table_grows(rig, opened);
    CHECK(until != 0u); }
  if (until == 0u) return;

  ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  for (std::uint32_t made = 0u; made < until; ++made) {
      rig.host().cl_handle(0x7000u + made, CL_SUCCESS);
      CHECK(rig.loader().create_buffer(opened.context(), CL_MEM_READ_WRITE, 256u, nullptr) !=
            nullptr);
  }
  rig.host().cl_handle(0x6600u, CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  rig.platform().fail_allocation_at(1u);
  cl_int error = CL_SUCCESS;
  CHECK(rig.loader().create_buffer(opened.context(), CL_MEM_READ_WRITE, 256u, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);
  CHECK(rig.root().invariants_hold()); }
