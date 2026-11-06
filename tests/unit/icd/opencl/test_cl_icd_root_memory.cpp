#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/cl_context_rig.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "harness/icd/cl_running_rig.hpp"
#include "harness/icd/recording_log.hpp"
#include "icd/opencl/cl_icd_root.hpp"
#include "icd/opencl/cl_wait_list.hpp"
#include "icd/opencl/guest_cl_context.hpp"

#include <CL/cl_ext.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::ClIcdRig;
using av::ClRunningRig;
using av::clicd::ClObject;
using av::clicd::ClWaitList;
using av::clicd::GuestClContext;

namespace av::clicd {
struct ClIcdRootProbe {
    static ClObject *with(ClIcdRoot &root, ClObject::Kind kind, std::uint64_t wire, GuestClContext *context)
    { ClObject *made = nullptr;
      return root.objects_.make(kind, wire, context, nullptr, made) == av::Status::ok ? made : nullptr; }
    static void fill_the_object_pool(ClIcdRig &rig)
    { ClObject *made = nullptr;
      rig.platform().fail_allocation_at(1u);
      for (std::uint64_t wire = 0x9000u; wire < 0x9400u; ++wire)
          if (rig.root().objects_.make(ClObject::Kind::event, wire, nullptr, nullptr, made) != av::Status::ok) break;
      rig.platform().fail_allocation_at(0u); }
    static GuestClContext *context(ClIcdRoot &root, cl_context handle)
    { ClObject *const held = root.judged(handle, ClObject::Kind::context);
      return held != nullptr ? root.context_of(*held) : nullptr; }
};
}

using av::clicd::ClIcdRootProbe;

namespace {

class Strangers {
public:
    Strangers(ClIcdRig &rig, ClRunningRig &run)
        : crig_(), guest_(crig_.connection(), crig_.platform(), crig_.send_lock(), crig_.blocks_lock(), crig_.lines(),
                          crig_.handles())
    {
        program_ = ClIcdRootProbe::with(rig.root(), ClObject::Kind::program, 0x6720u, &guest_);
        context_ = ClIcdRootProbe::with(rig.root(), ClObject::Kind::context, 0x6420u, &guest_);
        queue_ = ClIcdRootProbe::with(rig.root(), ClObject::Kind::command_queue, 0x6520u, &guest_);
        other_wire_device_ = ClIcdRootProbe::with(rig.root(), ClObject::Kind::device, 0x6320u, nullptr);
        same_wire_device_ = ClIcdRootProbe::with(rig.root(), ClObject::Kind::device, ClRunningRig::host_device, nullptr);
        static_cast<void>(run);
    }

    bool made() const
    {
        return program_ != nullptr && context_ != nullptr && queue_ != nullptr && other_wire_device_ != nullptr &&
               same_wire_device_ != nullptr;
    }
    cl_program       program() const { return reinterpret_cast<cl_program>(program_); }
    cl_context       context() const { return reinterpret_cast<cl_context>(context_); }
    cl_command_queue queue() const { return reinterpret_cast<cl_command_queue>(queue_); }
    cl_device_id     other_wire() const { return reinterpret_cast<cl_device_id>(other_wire_device_); }
    cl_device_id     same_wire() const { return reinterpret_cast<cl_device_id>(same_wire_device_); }

private:
    av::ClContextRig crig_;
    GuestClContext   guest_;
    ClObject        *program_ = nullptr;
    ClObject        *context_ = nullptr;
    ClObject        *queue_ = nullptr;
    ClObject        *other_wire_device_ = nullptr;
    ClObject        *same_wire_device_ = nullptr;
};

}

TEST(cl_icd_root_memory, an_object_of_a_context_no_device_runs_has_no_device_and_no_device_is_its_device)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  Strangers strangers(rig, run);
  CHECK(strangers.made());
  if (!strangers.made()) return;
  const std::uint32_t sent = rig.kmt().commands();
  std::size_t         wide = 0u;

  CHECK_EQ(rig.root().program_build_info(strangers.program(), run.device(), CL_PROGRAM_BUILD_STATUS, sizeof wide, &wide,
                                         nullptr),
           CL_INVALID_DEVICE);
  CHECK_EQ(rig.root().program_build_info(run.program(), strangers.other_wire(), CL_PROGRAM_BUILD_STATUS, sizeof wide,
                                         &wide, nullptr),
           CL_INVALID_DEVICE);
  CHECK_EQ(rig.root().program_build_info(run.program(), strangers.same_wire(), CL_PROGRAM_BUILD_STATUS, sizeof wide,
                                         &wide, nullptr),
           CL_INVALID_DEVICE);
  CHECK_EQ(rig.root().kernel_work_group_info(run.kernel(), strangers.same_wire(), CL_KERNEL_WORK_GROUP_SIZE,
                                             sizeof wide, &wide, nullptr),
           CL_INVALID_DEVICE);
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().create_queue(strangers.context(), run.device(), nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_DEVICE);
  CHECK(rig.root().create_queue(run.context(), strangers.other_wire(), nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_DEVICE);
  CHECK(rig.root().create_queue(run.context(), strangers.same_wire(), nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_DEVICE);

  cl_device_id shown = reinterpret_cast<cl_device_id>(&wide);
  CHECK_EQ(rig.root().context_info(strangers.context(), CL_CONTEXT_DEVICES, sizeof shown, &shown, nullptr), CL_SUCCESS);
  CHECK(shown == nullptr);
  shown = reinterpret_cast<cl_device_id>(&wide);
  CHECK_EQ(rig.root().queue_info(strangers.queue(), CL_QUEUE_DEVICE, sizeof shown, &shown, nullptr), CL_SUCCESS);
  CHECK(shown == nullptr);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_memory, a_buffers_flags_are_the_hosts_and_a_host_that_will_not_say_leaves_the_query_unanswered)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  cl_mem_flags flags = 0u;
  const cl_mem_flags host_flags = CL_MEM_READ_WRITE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&host_flags), sizeof host_flags);
  CHECK_EQ(rig.root().mem_object_info(run.buffer(), CL_MEM_FLAGS, sizeof flags, &flags, nullptr), CL_SUCCESS);
  CHECK_EQ(flags, host_flags);
  rig.host().cl_handle(ClRunningRig::host_buffer + 1u, CL_SUCCESS);
  const cl_mem fresh = rig.loader().create_buffer(run.context(), CL_MEM_READ_WRITE, 64u, nullptr);
  CHECK(fresh != nullptr);
  rig.host().cl_info(CL_INVALID_OPERATION, nullptr, 0u);
  CHECK_EQ(rig.root().mem_object_info(fresh, CL_MEM_FLAGS, sizeof flags, &flags, nullptr), CL_INVALID_OPERATION);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_memory, an_adapter_windows_gave_no_luid_is_left_out_and_logged)
{ ClIcdRig rig;
  rig.kmt().fail_at(av::MockKmt::Call::adapter_luid, 1u, 0xC0000001u);
  CHECK(rig.loader().load() == av::FakeClLoader::Load::kept);
  cl_device_id device = nullptr;
  cl_uint      found = 0u;
  CHECK(rig.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device, &found) != CL_SUCCESS);
  CHECK(device == nullptr);
  CHECK_EQ(rig.log().holding("an avgpu adapter is left out: Windows gave it no LUID"), 1u);
  CHECK(rig.root().invariants_hold()); }

namespace {

alignas(4096) std::uint8_t lent_block[av::clicd::ClIcdRoot::lent_minimum];
std::uint8_t               big_guest[64u * 1024u];

constexpr std::uint64_t host_lent = 0x6E00u;
constexpr std::uint64_t host_mapped = 0x0000'7F00'0000'8000ull;

cl_mem lent_buffer(ClIcdRig &rig, ClRunningRig &run, std::uint64_t handle)
{
    rig.host().blob_import(0u, 0x0000700000000001ull + handle);
    rig.host().cl_handle(handle, CL_SUCCESS);
    cl_int error = CL_SUCCESS;
    return rig.root().create_buffer(run.context(), CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, sizeof lent_block,
                                    lent_block, nullptr, &error);
}

}

TEST(cl_icd_root_memory, a_buffer_lent_to_the_host_maps_in_place_inside_its_bounds_and_nowhere_else)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_mem lent = lent_buffer(rig, run, host_lent);
  CHECK(lent != nullptr);
  if (lent == nullptr) return;
  cl_int      error = CL_SUCCESS;
  const std::uint32_t sent = rig.kmt().commands();
  CHECK(rig.root().map_buffer(run.queue(), lent, CL_TRUE, CL_MAP_READ, 1u << 20u, 16u, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK(rig.root().map_buffer(run.queue(), lent, CL_TRUE, CL_MAP_READ, 0u, sizeof lent_block + 1u, nullptr, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent);
  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  void *const mapped = rig.root().map_buffer(run.queue(), lent, CL_TRUE, CL_MAP_READ, 4096u, 4096u, nullptr, &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(mapped == lent_block + 4096);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_memory, a_lent_buffer_is_given_back_at_once_when_every_queue_is_idle_and_kept_until_they_are_when_not)
{ { ClIcdRig     rig;
    ClRunningRig run(rig);
    CHECK(run.made());
    if (!run.made()) return;
    const cl_mem lent = lent_buffer(rig, run, host_lent);
    CHECK(lent != nullptr);
    if (lent == nullptr) return;
    rig.host().cl_result(CL_SUCCESS);
    rig.host().nop();
    CHECK_EQ(rig.root().release(lent, ClObject::Kind::mem), CL_SUCCESS);
    CHECK_EQ(rig.kmt().queued_answers(), 0u);
    CHECK(rig.root().invariants_hold()); }
  { ClIcdRig     rig;
    ClRunningRig run(rig);
    CHECK(run.made());
    if (!run.made()) return;
    const cl_mem lent = lent_buffer(rig, run, host_lent);
    CHECK(lent != nullptr);
    CHECK(run.queue_event() != nullptr);
    if (lent == nullptr) return;
    rig.host().cl_result(CL_SUCCESS);
    CHECK_EQ(rig.root().release(lent, ClObject::Kind::mem), CL_SUCCESS);
    CHECK(rig.root().invariants_hold()); } }

TEST(cl_icd_root_memory, a_blocking_write_the_host_refuses_is_the_writes_error)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  rig.host().cl_result(CL_INVALID_OPERATION);
  CHECK_EQ(rig.root().enqueue_write(run.queue(), run.buffer(), CL_TRUE, 0u, sizeof big_guest, big_guest, nullptr),
           CL_INVALID_OPERATION);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_memory, a_map_behind_a_bad_wait_list_or_whose_write_back_the_host_refuses_is_the_calls_error)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  int            not_an_event = 0;
  const cl_event stray = reinterpret_cast<cl_event>(&not_an_event);
  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  unsigned char device[64] = {};
  rig.host().cl_bytes(CL_SUCCESS, device, sizeof device);
  cl_int error = CL_SUCCESS;
  void *const mapped = rig.root().map_buffer(run.queue(), run.buffer(), CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 0u,
                                             sizeof device, nullptr, &error);
  CHECK(mapped != nullptr);
  if (mapped == nullptr) return;
  CHECK_EQ(rig.root().unmap(run.queue(), run.buffer(), mapped, nullptr, ClWaitList(1u, &stray)),
           CL_INVALID_EVENT_WAIT_LIST);
  CHECK(rig.root().map_buffer(run.queue(), run.buffer(), CL_TRUE, CL_MAP_READ, 0u, 16u, nullptr, &error,
                              ClWaitList(1u, &stray)) == nullptr);
  CHECK_EQ(error, CL_INVALID_EVENT_WAIT_LIST);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().unmap(run.queue(), run.buffer(), mapped, nullptr), CL_SUCCESS);

  rig.host().cl_handle(host_mapped + 0x1000u, CL_SUCCESS);
  void *const big = rig.root().map_buffer(run.queue(), run.buffer(), CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0u,
                                          sizeof big_guest, nullptr, &error);
  CHECK(big != nullptr);
  if (big == nullptr) return;
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_result(CL_INVALID_OPERATION);
  CHECK_EQ(rig.root().unmap(run.queue(), run.buffer(), big, nullptr), CL_INVALID_OPERATION);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_memory, shared_memory_maps_and_unmaps_behind_a_wait_list_the_host_may_run_ahead_of_or_settle_here)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  rig.host().cl_handle(0x0000'7F55'0000'1000ull, CL_SUCCESS);
  void *const mirror = rig.root().svm_alloc(run.context(), CL_MEM_READ_WRITE, 256u, 0u);
  CHECK(mirror != nullptr);
  if (mirror == nullptr) return;
  const cl_event gate = run.gate();
  int            not_an_event = 0;
  const cl_event stray = reinterpret_cast<cl_event>(&not_an_event);

  CHECK_EQ(rig.root().svm_map(run.queue(), CL_TRUE, CL_MAP_READ, mirror, 16u, nullptr, ClWaitList(1u, &stray)),
           CL_INVALID_EVENT_WAIT_LIST);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().svm_map(run.queue(), CL_FALSE, CL_MAP_READ, mirror, 16u, nullptr, ClWaitList(1u, &gate)),
           CL_SUCCESS);
  rig.host().cl_enqueued(CL_SUCCESS, ClRunningRig::host_event + 1u);
  rig.host().cl_result(CL_SUCCESS);
  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().svm_map(run.queue(), CL_TRUE, CL_MAP_READ, mirror, 16u, nullptr,
                              ClWaitList(1u, &gate, CL_TRUE)),
           CL_SUCCESS);
  rig.host().cl_enqueued(CL_INVALID_OPERATION, 0u);
  CHECK_EQ(rig.root().svm_map(run.queue(), CL_TRUE, CL_MAP_READ, mirror, 16u, nullptr,
                              ClWaitList(1u, &gate, CL_TRUE)),
           CL_INVALID_OPERATION);

  CHECK_EQ(rig.root().svm_unmap(run.queue(), mirror, nullptr, ClWaitList(1u, &stray)), CL_INVALID_EVENT_WAIT_LIST);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().svm_unmap(run.queue(), mirror, nullptr, ClWaitList(1u, &gate)), CL_SUCCESS);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_memory, a_program_that_belongs_to_no_context_has_no_device_to_build_for)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  ClObject *const bare = ClIcdRootProbe::with(rig.root(), ClObject::Kind::program, 0x6721u, nullptr);
  CHECK(bare != nullptr);
  if (bare == nullptr) return;
  std::size_t wide = 0u;
  CHECK_EQ(rig.root().program_build_info(reinterpret_cast<cl_program>(bare), run.device(), CL_PROGRAM_BUILD_STATUS,
                                         sizeof wide, &wide, nullptr),
           CL_INVALID_DEVICE);
  ClObject *const bare_queue = ClIcdRootProbe::with(rig.root(), ClObject::Kind::command_queue, 0x6521u, nullptr);
  CHECK(bare_queue != nullptr);
  cl_event event = nullptr;
  CHECK_EQ(rig.root().enqueue_marker(reinterpret_cast<cl_command_queue>(bare_queue), 0u, nullptr, &event),
           CL_OUT_OF_RESOURCES);
  CHECK(event == nullptr);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_memory, the_extensions_of_a_lost_device_are_unanswerable_and_a_list_without_memory_is_refused)
{ { ClIcdRig     rig;
    ClRunningRig run(rig);
    CHECK(run.made());
    if (!run.made()) return;
    run.lose();
    cl_name_version listed[8] = {};
    CHECK_EQ(rig.root().device_info(run.device(), CL_DEVICE_OPENCL_C_FEATURES, sizeof listed, listed, nullptr),
             CL_OUT_OF_RESOURCES);
    CHECK_EQ(rig.root().device_info(run.device(), CL_DEVICE_EXTENSIONS_WITH_VERSION, sizeof listed, listed, nullptr),
             CL_INVALID_DEVICE); }
  bool starved = false;
  for (std::uint32_t nth = 1u; nth <= 6u; ++nth) {
      ClIcdRig     rig;
      ClRunningRig run(rig);
      CHECK(run.made());
      if (!run.made()) return;
      rig.host().cl_info(CL_SUCCESS, "cl_khr_icd cl_khr_fp64", 23u);
      rig.platform().fail_allocation_at(nth);
      char text[128] = {};
      const cl_int result = rig.loader().device_info(run.device(), CL_DEVICE_EXTENSIONS, sizeof text, text, nullptr);
      CHECK(result == CL_SUCCESS || result == CL_OUT_OF_HOST_MEMORY);
      starved = starved || result == CL_OUT_OF_HOST_MEMORY;
      CHECK(rig.root().invariants_hold());
  }
  CHECK(starved); }

TEST(cl_icd_root_memory, a_buffer_made_over_blocks_the_arena_no_longer_holds_cannot_be_mapped)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  ClObject *const buffer = ClIcdRootProbe::with(rig.root(), ClObject::Kind::mem, 0x6610u, ClIcdRootProbe::context(rig.root(), run.context()));
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;
  buffer->back_with(1u << 30u, 64u);
  cl_int error = CL_SUCCESS;
  const std::uint32_t sent = rig.kmt().commands();
  CHECK(rig.root().map_buffer(run.queue(), reinterpret_cast<cl_mem>(buffer), CL_TRUE, CL_MAP_READ, 0u, 16u, nullptr,
                              &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_memory, an_unmap_naming_events_but_counting_none_is_a_bad_wait_list)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_event gate = run.gate();
  rig.host().cl_handle(host_mapped, CL_SUCCESS);
  cl_int error = CL_SUCCESS;
  void *const mapped = rig.root().map_buffer(run.queue(), run.buffer(), CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0u,
                                             16u, nullptr, &error);
  CHECK(mapped != nullptr);
  if (mapped == nullptr) return;
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().unmap(run.queue(), run.buffer(), mapped, nullptr, ClWaitList(0u, &gate)),
           CL_INVALID_EVENT_WAIT_LIST);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_memory, an_image_copy_names_its_wait_list_and_hands_back_the_hosts_event)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_mem image = run.image();
  const std::size_t origin[3] = {0u, 0u, 0u}, region[3] = {4u, 2u, 1u};
  int            not_an_event = 0;
  const cl_event stray = reinterpret_cast<cl_event>(&not_an_event);
  CHECK_EQ(rig.root().enqueue_copy_image(run.queue(), image, image, origin, origin, region, nullptr,
                                         ClWaitList(1u, &stray)),
           CL_INVALID_EVENT_WAIT_LIST);
  cl_event event = nullptr;
  rig.host().cl_enqueued(CL_SUCCESS, ClRunningRig::host_event + 5u);
  CHECK_EQ(rig.root().enqueue_copy_image(run.queue(), image, image, origin, origin, region, &event), CL_SUCCESS);
  CHECK(event != nullptr);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_memory, a_sampler_the_host_does_not_make_is_an_error_and_an_unknown_gl_property_is_refused)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  cl_int error = CL_SUCCESS;
  rig.host().cl_handle(0u, CL_SUCCESS);
  CHECK(rig.root().create_sampler(run.context(), nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  const cl_context_properties unknown[] = {0x4242, 1, 0};
  cl_device_id device = nullptr;
  CHECK_EQ(rig.root().gl_context_info(unknown, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof device, &device, nullptr),
           CL_INVALID_PROPERTY);
  CHECK(rig.root().create_context_from_type(unknown, CL_DEVICE_TYPE_CPU, nullptr, av::clicd::ClContextNotify()) ==
        nullptr);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_memory, a_flush_the_transport_cannot_publish_is_out_of_resources)
{ bool refused = false;
  for (std::uint32_t nth = 1u; nth <= 4u; ++nth) {
      ClIcdRig     rig;
      ClRunningRig run(rig);
      CHECK(run.made());
      if (!run.made()) return;
      rig.host().cl_result(CL_SUCCESS);
      rig.kmt().fail_at(av::MockKmt::Call::submit, nth, 0xC0000001u);
      const cl_int flushed = rig.root().flush(run.queue());
      refused = refused || flushed == CL_OUT_OF_RESOURCES;
      CHECK(flushed == CL_SUCCESS || flushed == CL_OUT_OF_RESOURCES);
      CHECK(rig.root().invariants_hold());
  }
  CHECK(refused); }

namespace {

std::uint32_t destructions = 0u;

void CL_CALLBACK buffer_went(cl_mem, void *) { ++destructions; }

}

TEST(cl_icd_root_memory, a_destructor_callback_that_cannot_be_queued_for_want_of_memory_is_lost_and_logged)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  rig.host().cl_handle(ClRunningRig::host_buffer + 3u, CL_SUCCESS);
  const cl_mem buffer = rig.loader().create_buffer(run.context(), CL_MEM_READ_WRITE, 64u, nullptr);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;
  destructions = 0u;
  CHECK_EQ(rig.root().watch_destruction(buffer, &buffer_went, nullptr), CL_SUCCESS);
  const std::uint32_t lines = rig.log().lines();
  rig.host().cl_result(CL_SUCCESS);
  rig.platform().fail_allocation_at(1u);
  CHECK_EQ(rig.root().release(buffer, ClObject::Kind::mem), CL_SUCCESS);
  CHECK_EQ(destructions, 0u);
  CHECK(rig.log().lines() > lines);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_memory, a_buffer_over_application_memory_the_host_cannot_borrow_is_copied_instead)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  rig.host().blob_import(1u, 0u);
  rig.host().cl_handle(host_lent, CL_SUCCESS);
  cl_int error = CL_INVALID_VALUE;
  const cl_mem copied = rig.root().create_buffer(run.context(), CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
                                                 sizeof lent_block, lent_block, nullptr, &error);
  CHECK(copied != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_memory, a_borrowed_buffer_the_host_refuses_is_given_back_and_the_memory_copied_instead)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  rig.host().blob_import(0u, 0x0000700000000005ull);
  rig.host().cl_handle(0u, CL_INVALID_BUFFER_SIZE);
  rig.host().nop();
  rig.host().cl_handle(host_lent, CL_SUCCESS);
  cl_int error = CL_INVALID_VALUE;
  const cl_mem copied = rig.root().create_buffer(run.context(), CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
                                                 sizeof lent_block, lent_block, nullptr, &error);
  CHECK(copied != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_memory, a_borrowed_buffer_the_guest_cannot_keep_is_given_back_whenever_memory_runs_out)
{ bool refused = false;
  for (std::uint32_t nth = 1u; nth <= 6u; ++nth) {
      ClIcdRig     rig;
      ClRunningRig run(rig);
      CHECK(run.made());
      if (!run.made()) return;
      rig.host().blob_import(0u, 0x0000700000000006ull);
      rig.host().cl_handle(host_lent, CL_SUCCESS);
      rig.host().cl_result(CL_SUCCESS);
      rig.host().nop();
      rig.host().cl_handle(host_lent + 1u, CL_SUCCESS);
      rig.host().cl_result(CL_SUCCESS);
      rig.host().nop();
      av::clicd::ClIcdRootProbe::fill_the_object_pool(rig);
      rig.platform().fail_allocation_at(nth);
      cl_int error = CL_SUCCESS;
      const cl_mem buffer = rig.root().create_buffer(run.context(), CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
                                                     sizeof lent_block, lent_block, nullptr, &error);
      if (buffer == nullptr) {
          refused = true;
          CHECK(error != CL_SUCCESS);
      }
      CHECK(rig.root().invariants_hold());
  }
  CHECK(refused); }
