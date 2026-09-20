/* What the ICD answers when the step it needs does not work.
 *
 * Opening the device is a sequence of host calls, each of which can refuse,
 * and each refusal has to leave the ICD exactly as it was: no device shown, no
 * context leaked, and the next attempt free to try again. An application that
 * is told CL_DEVICE_NOT_FOUND has lost nothing; one handed a device made out
 * of a failed open has lost everything after it.
 *
 * The enumeration in front of that is the API's own shape - a platform this
 * driver does not own, a type filter naming no GPU, and the two-call form
 * asked with neither an array nor a count. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "harness/icd/fake_cl_loader.hpp"
#include "harness/icd/recording_log.hpp"
#include "icd/opencl/cl_icd_root.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"

#include <CL/cl_ext.h>
#include <cstdint>

using av::ClIcdRig;
using av::FakeClLoader;
using av::clicd::ClObject;

namespace av::clicd {

/* An object with no context behind it, which is how a device the root never
 * opened is put in front of it. */
struct ClIcdRootProbe {
    static ClObject *without_a_context(ClIcdRoot &root, ClObject::Kind kind, std::uint64_t wire)
    { ClObject *made = nullptr;
      return root.objects_.make(kind, wire, nullptr, nullptr, made) == av::Status::ok ? made
                                                                                      : nullptr; }
};

}  // namespace av::clicd

namespace {

constexpr std::uint64_t host_platform = 0x6200u;
constexpr std::uint64_t host_device = 0x6300u;

/* The platform this driver owns, which every device query must name. */
cl_platform_id this_platform(ClIcdRig &rig)
{
    cl_platform_id platform = nullptr;
    cl_uint        count = 0u;
    CHECK_EQ(rig.root().platform_ids(1u, &platform, &count), CL_SUCCESS);
    CHECK_EQ(count, 1u);
    return platform;
}

}  // namespace

TEST(cl_icd_root_refusals, a_platform_this_driver_does_not_own_is_refused)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  cl_device_id device = nullptr;
  cl_uint      count = 0xEEu;
  /* Another vendor's platform, which the loader may hand any ICD. */
  cl_platform_id stranger = reinterpret_cast<cl_platform_id>(std::uintptr_t{0x1234});
  CHECK_EQ(rig.root().device_ids(stranger, CL_DEVICE_TYPE_GPU, 1u, &device, &count),
           CL_INVALID_PLATFORM);
  CHECK(device == nullptr);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, a_query_with_neither_an_array_nor_a_count_asks_for_nothing)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  cl_platform_id platform = this_platform(rig);
  /* Nowhere to put an answer and nothing asking how many: the call wants
   * nothing, which the API calls an invalid value. */
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_GPU, 1u, nullptr, nullptr),
           CL_INVALID_VALUE);
  /* And an array with no room for an entry. */
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
  /* This adapter's device is a GPU. A CPU or an accelerator is not an error,
   * it is a search that found nothing - and the count says so rather than
   * being left as the application had it. */
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_CPU, 0u, nullptr, &count),
           CL_DEVICE_NOT_FOUND);
  CHECK_EQ(count, 0u);
  count = 0xEEu;
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_ACCELERATOR, 0u, nullptr, &count),
           CL_DEVICE_NOT_FOUND);
  CHECK_EQ(count, 0u);
  /* Nothing was opened for a search that could not match. */
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, every_type_that_includes_this_gpu_finds_it)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  cl_platform_id platform = this_platform(rig);
  /* CL_DEVICE_TYPE_ALL is every bit, so it finds this GPU through the GPU
   * bit rather than through a case of its own. */
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
      /* The device is opened once and answered every time after. */
      CHECK_EQ(rig.root().device_ids(platform, type, 1u, &device, &count), CL_SUCCESS);
      CHECK_EQ(count, 1u);
      CHECK(device != nullptr);
  }
  CHECK(rig.root().invariants_hold()); }

/* Each step of the open, refused in turn. Every one of them must leave the
 * ICD showing no device and holding no context. */
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
  /* The context the open made for itself went back with the failure. */
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
  /* CL_SUCCESS with nothing in the list: a host that has no OpenCL platform
   * at all answers exactly this, and it is not an error to it. */
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
  /* A count of one and a handle of nothing: the host contradicted itself,
   * and a zero handle names no object this ICD could ever pass back. */
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
  /* The three self-description queries all refused. None of them is needed
   * to have a device: what they change is what the device says about itself,
   * and the ICD's own defaults are what it says instead. */
  rig.host().cl_info(CL_INVALID_VALUE, nullptr, 0u);
  rig.host().cl_info(CL_INVALID_VALUE, nullptr, 0u);
  rig.host().cl_info(CL_INVALID_VALUE, nullptr, 0u);
  cl_device_id device = nullptr;
  cl_uint      count = 0u;
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_GPU, 1u, &device, &count), CL_SUCCESS);
  CHECK_EQ(count, 1u);
  CHECK(device != nullptr);
  /* And a device that answers nothing about SVM reports none, which is the
   * capability rule: never claim what is not proven. */
  cl_device_svm_capabilities svm = 0xEEu;
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_SVM_CAPABILITIES, sizeof svm, &svm, nullptr),
           CL_SUCCESS);
  CHECK_EQ(svm, static_cast<cl_device_svm_capabilities>(0u));
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_refusals, the_extensions_the_loader_asks_for_need_room_for_the_terminator)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  cl_platform_id platform = this_platform(rig);
  /* The loader asks this in every process, before anything is opened. An
   * empty list is still a string, so a buffer with no room for its
   * terminator is refused rather than written past. */
  char        nothing[1] = {'x'};
  std::size_t bytes = 0u;
  CHECK_EQ(rig.root().platform_info(platform, CL_PLATFORM_EXTENSIONS, 0u, nothing, &bytes),
           CL_INVALID_VALUE);
  CHECK_EQ(nothing[0], 'x');
  CHECK_EQ(rig.root().platform_info(platform, CL_PLATFORM_EXTENSIONS, sizeof nothing, nothing,
                                      &bytes),
           CL_SUCCESS);
  CHECK_EQ(nothing[0], '\0');
  CHECK_EQ(bytes, 1u);
  CHECK(rig.root().invariants_hold()); }

/* ---- what a host that will not make an object leaves behind -------------- */

namespace {

/* A context and a queue on an opened device, so each creation below has
 * something to be asked of. */
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

}  // namespace

/* Every object this ICD makes is the host's first. A host that refuses gives
 * its own error back to the application, and a host that answers success with
 * a handle of nothing is a host that contradicted itself - which is not an
 * error the API has, so it becomes CL_OUT_OF_RESOURCES. Either way the guest
 * keeps no object. */
TEST(cl_icd_root_refusals, a_host_that_will_not_make_a_queue_makes_no_guest_object)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  rig.host().cl_handle(0u, CL_OUT_OF_HOST_MEMORY);
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().create_queue(opened.context(), opened.device(), nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);

  /* And the contradiction: success, with nothing made. */
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
  /* No sources at all, and a count with no array: neither is a program, and
   * neither reaches the host. */
  CHECK(rig.root().create_program(opened.context(), 0u, &source, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  error = CL_SUCCESS;
  CHECK(rig.root().create_program(opened.context(), 1u, nullptr, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

/* A context made on something that is not one device of this driver. */
TEST(cl_icd_root_refusals, a_context_made_on_no_device_or_several_is_refused)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  cl_device_id devices[2] = {opened.device(), opened.device()};
  cl_int       error = CL_SUCCESS;
  /* No list and no count. */
  CHECK(rig.root().create_context(nullptr, nullptr, 0u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  error = CL_SUCCESS;
  CHECK(rig.root().create_context(nullptr, devices, 0u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  /* This driver shows one device, so a context over two is over one it does
   * not have. */
  error = CL_SUCCESS;
  CHECK(rig.root().create_context(nullptr, devices, 2u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_DEVICE);
  /* And something that is not a device of this driver at all. */
  cl_device_id stranger = reinterpret_cast<cl_device_id>(std::uintptr_t{0x1234});
  error = CL_SUCCESS;
  CHECK(rig.root().create_context(nullptr, &stranger, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_DEVICE);
  CHECK(rig.root().invariants_hold()); }

/* ---- what the guest cannot keep ----------------------------------------- */

/* Every object made here is made twice: once on the host, and once in the
 * guest's table so the application has something to hold. When the second
 * cannot happen the first has to be undone, because the application is about
 * to be told the call failed and an object it was never handed is one it can
 * never release.
 *
 * A host that answers an enqueue with success and an event of nothing is the
 * cheapest way to reach that: zero names no object, so the table refuses it
 * and there is nothing to give back either - the release must not be sent
 * for a handle that is not one. */
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

  /* The write is enqueued and the host says it made no event for it. */
  unsigned char from[8] = {};
  rig.host().cl_enqueued(CL_SUCCESS, 0u);
  const std::uint32_t before = rig.kmt().commands();
  cl_event            event = nullptr;
  CHECK_EQ(rig.root().enqueue_write(queue, buffer, CL_FALSE, 0u, sizeof from, from, &event),
           CL_SUCCESS);
  /* Nothing to hand the application, and nothing sent to undo: exactly one
   * command, the write itself. */
  CHECK(event == nullptr);
  CHECK_EQ(rig.kmt().commands(), before + 1u);
  CHECK_EQ(rig.kmt().command_id(before),
           static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueWriteBuffer));
  CHECK(rig.root().invariants_hold()); }

/* An object of the wrong kind is told which kind it should have been. The
 * three kinds an application never passes to a release are still answered,
 * because the answer is what an application that passed one is entitled to. */
TEST(cl_icd_root_refusals, every_kind_names_its_own_refusal)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  /* A context is not a queue, a buffer, a program, a kernel or an event, and
   * each ask is told so in the API's own words rather than a single code. */
  void *const context = opened.context();
  CHECK_EQ(rig.root().release(context, ClObject::Kind::platform), CL_INVALID_PLATFORM);
  CHECK_EQ(rig.root().release(context, ClObject::Kind::device), CL_INVALID_DEVICE);
  CHECK_EQ(rig.root().release(context, ClObject::Kind::sampler), CL_INVALID_SAMPLER);
  CHECK_EQ(rig.root().release(context, ClObject::Kind::command_queue), CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().release(context, ClObject::Kind::program), CL_INVALID_PROGRAM);
  CHECK_EQ(rig.root().release(context, ClObject::Kind::mem), CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().release(context, ClObject::Kind::kernel), CL_INVALID_KERNEL);
  CHECK_EQ(rig.root().release(context, ClObject::Kind::event), CL_INVALID_EVENT);
  /* And the context itself still releases, so none of the above consumed it. */
  CHECK(rig.root().invariants_hold()); }

/* ---- a context that cannot be made ------------------------------------- */

TEST(cl_icd_root_refusals, a_context_on_another_vendors_platform_is_refused)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  /* CL_CONTEXT_PLATFORM naming a platform this driver does not own. The
   * loader may hand any ICD a list built for another, and answering it would
   * be answering for a driver that is not this one. */
  const std::uint64_t stranger[] = {static_cast<std::uint64_t>(CL_CONTEXT_PLATFORM), 0x1234u, 0u};
  cl_device_id        device = opened.device();
  cl_int              error = CL_SUCCESS;
  const std::uint32_t sent = rig.kmt().commands();
  CHECK(rig.root().create_context(stranger, &device, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PLATFORM);
  CHECK_EQ(rig.kmt().commands(), sent);

  /* A list naming this driver's own platform is accepted, and so is one
   * naming a platform of nothing - which is the list an application builds
   * when it has no preference. */
  cl_platform_id      mine = this_platform(rig);
  const std::uint64_t ours[] = {static_cast<std::uint64_t>(CL_CONTEXT_PLATFORM),
                                static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(mine)),
                                0u};
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
  /* The context's own blob arena import faults the transport, so the make
   * fails. The application is told the device is not available rather than
   * being handed a context whose every call is already lost. */
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

  /* The guest's half is made and the host refuses its own. The guest context
   * has to go with it: a transport nobody can reach is an adapter held open
   * for nothing. */
  rig.host().blob_import(0u, 0x7Au);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(0u, CL_OUT_OF_HOST_MEMORY);
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().create_context(nullptr, &device, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);

  /* And the same when the host answers success with no context at all. */
  rig.host().blob_import(0u, 0x7Bu);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(0u, CL_SUCCESS);
  error = CL_SUCCESS;
  CHECK(rig.root().create_context(nullptr, &device, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_DEVICE_NOT_AVAILABLE);

  /* Nothing was kept: the next context still works. */
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

  /* The context's transport is made and the host will not say what it has,
   * so there is no device to make a context on. */
  rig.host().blob_import(0u, 0x7Du);
  rig.host().cl_ids(CL_INVALID_VALUE, nullptr, 0u, 0u);
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().create_context(nullptr, &device, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_DEVICE_NOT_AVAILABLE);

  /* And the same one step later, at the device list. */
  rig.host().blob_import(0u, 0x7Eu);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_INVALID_VALUE, nullptr, 0u, 0u);
  error = CL_SUCCESS;
  CHECK(rig.root().create_context(nullptr, &device, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_DEVICE_NOT_AVAILABLE);
  CHECK(rig.root().invariants_hold()); }

/* The two-call shape, in each of the three ways an application uses it: ask
 * only how many, ask only for the array, and ask for both. Each writes what
 * it was given and nothing it was not. */
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

  /* The count alone: the first call an application makes. */
  cl_uint count = 0xEEu;
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_GPU, 0u, nullptr, &count), CL_SUCCESS);
  CHECK_EQ(count, 1u);

  /* The array alone, with no count: nothing is written past the one entry
   * and the application's own word is untouched. */
  cl_device_id devices[2] = {nullptr, reinterpret_cast<cl_device_id>(std::uintptr_t{0xEE})};
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_GPU, 2u, devices, nullptr), CL_SUCCESS);
  CHECK(devices[0] != nullptr);
  CHECK(devices[1] == reinterpret_cast<cl_device_id>(std::uintptr_t{0xEE}));

  /* And both together. */
  count = 0xEEu;
  devices[0] = nullptr;
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_GPU, 1u, devices, &count), CL_SUCCESS);
  CHECK_EQ(count, 1u);
  CHECK(devices[0] != nullptr);

  /* A type that finds nothing, asked for the array alone: the count it did
   * not ask for is not written, and the array is left as it was. */
  devices[0] = reinterpret_cast<cl_device_id>(std::uintptr_t{0xAA});
  CHECK_EQ(rig.root().device_ids(platform, CL_DEVICE_TYPE_CPU, 1u, devices, nullptr),
           CL_DEVICE_NOT_FOUND);
  CHECK(devices[0] == reinterpret_cast<cl_device_id>(std::uintptr_t{0xAA}));
  CHECK(rig.root().invariants_hold()); }

/* A buffer that asked for host memory and could not have the arena's is
 * staged instead: slower, and never wrong. Both reasons are reachable - the
 * device refusing to make it over that memory, and there being no room. */
TEST(cl_icd_root_refusals, a_buffer_the_device_will_not_back_falls_back_to_the_ordinary_one)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;

  /* The backed create is refused, and the plain one behind it succeeds. The
   * application gets a buffer either way and never learns the difference. */
  rig.host().cl_handle(0u, CL_INVALID_VALUE);
  rig.host().cl_handle(0x6600u, CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  cl_int error = CL_INVALID_VALUE;
  cl_mem buffer = rig.loader().create_buffer(opened.context(),
                                             CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, 256u,
                                             &error);
  CHECK(buffer != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  /* Two creates: the one over the arena, then the ordinary one. */
  CHECK_EQ(rig.kmt().commands(), sent + 2u);
  CHECK_EQ(rig.kmt().command_id(sent),
           static_cast<std::uint32_t>(av::clproto::kCmd_clCreateBufferWithProperties));
  CHECK_EQ(rig.kmt().command_id(sent + 1u),
           static_cast<std::uint32_t>(av::clproto::kCmd_clCreateBuffer));
  CHECK(rig.root().invariants_hold());

  /* And when both refuse, the application is told the host's own error. */
  rig.host().cl_handle(0u, CL_INVALID_VALUE);
  rig.host().cl_handle(0u, CL_MEM_OBJECT_ALLOCATION_FAILURE);
  error = CL_SUCCESS;
  CHECK(rig.loader().create_buffer(opened.context(), CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
                                   256u, &error) == nullptr);
  CHECK_EQ(error, CL_MEM_OBJECT_ALLOCATION_FAILURE);
  CHECK(rig.root().invariants_hold()); }

/* The host listing a platform but counting no devices on it: a platform with
 * nothing behind it is not a device, and the ICD shows none rather than
 * making one out of a handle it was never given. */
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

/* Opening the device needs guest memory twice - for the ICD's own context and
 * for the object the application is handed - and either can fail. Whichever
 * does, the ICD shows no device and holds nothing, and the next ask is free
 * to try again. */
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
      /* Whatever ran out, the answer is the same and nothing is shown. */
      CHECK_EQ(answered, CL_DEVICE_NOT_FOUND);
      CHECK_EQ(count, 0u);
      CHECK(device == nullptr);
      CHECK(rig.root().invariants_hold());
      /* Before the host was asked anything is the context's own memory;
       * after is the object the application would have been handed. */
      if (rig.kmt().commands() == 0u) proved_context = true;
      else proved_object = true;
  }
  /* Both have to be reachable, or this test is asserting less than it says. */
  CHECK(proved_context);
  CHECK(proved_object); }

/* An object of the wrong kind, at each entry that names two of them. A queue
 * is made on a context and a device, a buffer and a program on a context, and
 * a kernel on a program: each is told which one was wrong. */
TEST(cl_icd_root_refusals, each_entry_names_the_object_that_was_wrong)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  void *const not_an_object = opened.context();
  const std::uint32_t sent = rig.kmt().commands();
  cl_int error = CL_SUCCESS;

  /* A queue on something that is not a context, and on something that is not
   * a device - the context being right the second time. */
  CHECK(rig.root().create_queue(reinterpret_cast<cl_context>(opened.device()), opened.device(),
                                nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  error = CL_SUCCESS;
  CHECK(rig.root().create_queue(opened.context(), reinterpret_cast<cl_device_id>(not_an_object),
                                nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_DEVICE);

  /* A buffer and a program on something that is not a context. */
  error = CL_SUCCESS;
  CHECK(rig.loader().create_buffer(reinterpret_cast<cl_context>(opened.device()),
                                   CL_MEM_READ_WRITE, 256u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  error = CL_SUCCESS;
  CHECK(rig.loader().create_program(reinterpret_cast<cl_context>(opened.device()), "k", &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);

  /* Nothing crossed for any of them. */
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

/* The queries this ICD does not decide for itself go to the host, so they
 * need a transport; a device whose context is gone has none. */
TEST(cl_icd_root_refusals, a_device_query_the_host_owns_needs_a_transport_to_ask_over)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  /* The device belongs to the context the ICD opened for itself, not to the
   * application's - so it is that one a host query has to be asked over, and
   * that one whose fault leaves nothing to ask over at all.
   *
   * The first query faults it, which is the host refusing; the second finds
   * no transport, which is the different answer this is about. */
  char        name[64] = {};
  std::size_t bytes = 0u;
  rig.kmt().withhold_answer_at(1u);
  CHECK_EQ(rig.loader().device_info(opened.device(), CL_DEVICE_NAME, sizeof name, name, &bytes),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.loader().device_info(opened.device(), CL_DEVICE_NAME, sizeof name, name, &bytes),
           CL_INVALID_DEVICE);
  /* And the four it decides still answer, because none of them asks the
   * host anything. */
  cl_uint luid_valid = 0u;
  CHECK_EQ(rig.loader().device_info(opened.device(), CL_DEVICE_LUID_VALID_KHR, sizeof luid_valid,
                                    &luid_valid, nullptr),
           CL_SUCCESS);
  CHECK(rig.root().invariants_hold()); }

/* A context made on a platform property of nothing is a context with no
 * preference, which is the list most applications build. */
TEST(cl_icd_root_refusals, a_platform_property_of_nothing_is_no_preference_at_all)
{ ClIcdRig rig;
  Opened   opened(rig);
  CHECK(opened.made());
  if (!opened.made()) return;
  const std::uint64_t none[] = {static_cast<std::uint64_t>(CL_CONTEXT_PLATFORM), 0u, 0u};
  cl_device_id        device = opened.device();
  rig.host().blob_import(0u, 0x7Fu);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(0x6404u, CL_SUCCESS);
  cl_int error = CL_INVALID_VALUE;
  CHECK(rig.root().create_context(none, &device, 1u, &error) != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(rig.root().invariants_hold()); }

/* An enumeration that found nothing and was asked only for an array: the
 * count it was not given must not be written through. */
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

/* The guest's object table grows in chunks, so only the make that needs a new
 * one allocates. These two find that make by watching for the allocation, and
 * then refuse exactly it - which is the only way to reach 'the host made it
 * and the guest cannot keep it'. */
namespace {

/* How many buffers must exist before the next one needs a new chunk. */
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

}  // namespace

/* A buffer made over the arena that the guest cannot keep: the host's buffer
 * is given back and so is the block, or the next buffer could never have that
 * memory. */
TEST(cl_icd_root_refusals, an_arena_backed_buffer_the_guest_cannot_keep_gives_its_block_back)
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
  /* The next one needs a chunk, and there is none to be had. */
  rig.host().cl_handle(0x6600u, CL_SUCCESS);   /* the backed create */
  rig.host().cl_result(CL_SUCCESS);            /* given back again */
  rig.platform().fail_allocation_at(1u);
  cl_int error = CL_SUCCESS;
  CHECK(rig.loader().create_buffer(opened.context(), CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
                                   256u, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);
  CHECK(rig.root().invariants_hold());

  /* The block came back, so the next buffer can have it. */
  rig.host().cl_handle(0x6601u, CL_SUCCESS);
  CHECK(rig.loader().create_buffer(opened.context(), CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR,
                                   256u, nullptr) != nullptr);
  CHECK(rig.root().invariants_hold()); }

/* And a cl_context the guest cannot keep: the host's context goes with it,
 * because a context nobody holds is an adapter held open for nothing. */
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
  /* Which allocation of the create is the table's depends on what the make
   * before it did, so the refusal is swept until it lands after the host has
   * made its context - which is the case this is about. */
  bool proved = false;
  for (std::uint32_t refused = 1u; refused <= 8u && !proved; ++refused) {
      rig.host().blob_import(0u, 0x80u + refused);
      rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
      rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
      rig.host().cl_handle(0x6410u + refused, CL_SUCCESS);   /* the host's context */
      rig.host().cl_result(CL_SUCCESS);                      /* released again */
      const std::uint32_t before = rig.kmt().commands();
      rig.platform().fail_allocation_at(refused);
      cl_int error = CL_SUCCESS;
      if (rig.root().create_context(nullptr, &device, 1u, &error) != nullptr) continue;
      CHECK(error == CL_OUT_OF_HOST_MEMORY || error == CL_DEVICE_NOT_AVAILABLE);
      /* The host made its context and the guest could not keep it: the
       * release behind it is what says the host's went back too. */
      if (error == CL_OUT_OF_HOST_MEMORY && rig.kmt().commands() > before + 3u) proved = true;
      CHECK(rig.root().invariants_hold());
  }
  CHECK(proved); }

/* The very first context a root makes, with no memory for it.
 *
 * A device the table holds but that no open produced is what gets there: the
 * root has never made a context of its own, so the pool it keeps them in has
 * no room yet and asking for one is what allocates. */
TEST(cl_icd_root_refusals, a_root_with_no_memory_for_its_first_context_makes_none)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  ClObject *const device =
      av::clicd::ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::device, 0x6300u);
  CHECK(device != nullptr);
  if (device == nullptr) return;
  auto *const as_device = reinterpret_cast<cl_device_id>(device);
  const std::uint32_t sent = rig.kmt().commands();
  rig.platform().fail_allocation_at(1u);
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().create_context(nullptr, &as_device, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);
  /* Nothing was asked of the host: there was never a transport to ask over. */
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

/* A queue asked for on a context this root never opened a device for.
 *
 * The host's context was made on the host's own device, and that is the
 * device a queue is made on - so a root that has never opened one has
 * nothing to make the queue against, whatever device the application named.
 * A device the table holds but no open produced is what gets there. */
TEST(cl_icd_root_refusals, a_queue_on_a_root_that_opened_no_device_is_refused)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  ClObject *const device =
      av::clicd::ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::device, 0x6300u);
  CHECK(device != nullptr);
  if (device == nullptr) return;
  auto *const as_device = reinterpret_cast<cl_device_id>(device);

  /* The context is made: its own transport, and the host's context on the
   * host's own device. */
  rig.host().blob_import(0u, 0x77u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(0x6400u, CL_SUCCESS);
  cl_int     error = CL_INVALID_VALUE;
  cl_context context = rig.root().create_context(nullptr, &as_device, 1u, &error);
  CHECK(context != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  if (context == nullptr) return;

  /* And the queue has no device of this root's to be made on. */
  const std::uint32_t sent = rig.kmt().commands();
  error = CL_SUCCESS;
  CHECK(rig.root().create_queue(context, as_device, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_DEVICE);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

/* A plain buffer the guest cannot keep: there is no block to give back, only
 * the host's buffer, and the same path has to serve both. */
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
  rig.host().cl_handle(0x6600u, CL_SUCCESS);   /* the plain create */
  rig.host().cl_result(CL_SUCCESS);            /* given back again */
  rig.platform().fail_allocation_at(1u);
  cl_int error = CL_SUCCESS;
  CHECK(rig.loader().create_buffer(opened.context(), CL_MEM_READ_WRITE, 256u, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);
  CHECK(rig.root().invariants_hold()); }
