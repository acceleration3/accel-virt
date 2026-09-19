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
