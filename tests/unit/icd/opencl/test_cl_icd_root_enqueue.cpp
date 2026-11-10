#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "harness/icd/cl_running_rig.hpp"
#include "icd/opencl/cl_icd_root.hpp"
#include "icd/opencl/cl_wait_list.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"

#include <cstddef>
#include <cstdint>

using av::ClIcdRig;
using av::ClRunningRig;
using av::clicd::ClObject;
using av::clicd::ClWaitList;

namespace av::clicd {
struct ClIcdRootProbe {
    static ClObject *without_a_context(ClIcdRoot &root, ClObject::Kind kind, std::uint64_t wire)
    { ClObject *made = nullptr;
      return root.objects_.make(kind, wire, nullptr, nullptr, made) == av::Status::ok ? made : nullptr; }
    static GuestClContext *context(ClIcdRoot &root, cl_context handle)
    { ClObject *const held = root.judged(handle, ClObject::Kind::context);
      return held != nullptr ? root.context_of(*held) : nullptr; }
    static void fill_the_object_pool(ClIcdRig &rig)
    { ClObject *made = nullptr;
      rig.platform().fail_allocation_at(1u);
      for (std::uint64_t wire = 0x9000u; wire < 0x9400u; ++wire)
          if (rig.root().objects_.make(ClObject::Kind::event, wire, nullptr, nullptr, made) != av::Status::ok) break;
      rig.platform().fail_allocation_at(0u); }
    static std::uint32_t without_unset_user_events(ClIcdRoot &root, ClientTransport &wire, const ClWaitList &waits,
                                                   std::uint64_t *listed)
    { return root.without_unset_user_events(wire, waits, listed); }
};
}

using av::clicd::ClIcdRootProbe;

namespace {

constexpr std::size_t origin[3] = {0u, 0u, 0u};
constexpr std::size_t region[3] = {4u, 2u, 1u};
constexpr std::size_t apart[3] = {8u, 4u, 0u};

cl_mem foreign_memory(ClIcdRig &rig)
{
    return reinterpret_cast<cl_mem>(ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::mem, 0x7700u));
}

}

TEST(cl_icd_root_enqueue, a_wait_list_names_only_the_user_events_that_are_still_unset_as_ones_to_run_ahead_of)
{ ClIcdRig       rig;
  ClRunningRig   run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  av::clicd::GuestClContext *const guest = ClIcdRootProbe::context(rig.root(), run.context());
  CHECK(guest != nullptr);
  if (guest == nullptr) return;
  ClObject *const parentless = ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::event, 0x7701u);
  const cl_event  queued = run.queue_event();
  CHECK(parentless != nullptr && queued != nullptr);
  if (parentless == nullptr || queued == nullptr) return;
  int not_an_event = 0;
  const cl_event events[] = {reinterpret_cast<cl_event>(&not_an_event), reinterpret_cast<cl_event>(parentless),
                             queued, run.gate(), run.gate(), run.gate()};
  std::uint64_t listed[6] = {1u, 2u, 3u, 4u, 5u, 6u};
  const std::int32_t unset = CL_QUEUED;
  const std::int32_t done = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&unset), sizeof unset);
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&done), sizeof done);
  rig.host().cl_info(CL_INVALID_EVENT, nullptr, 0u);
  const std::uint32_t kept =
      ClIcdRootProbe::without_unset_user_events(rig.root(), *guest->wire(), ClWaitList(6u, events), listed);
  CHECK_EQ(kept, 5u);
  CHECK_EQ(listed[0], 1u);
  CHECK_EQ(listed[1], 2u);
  CHECK_EQ(listed[2], 3u);
  CHECK_EQ(listed[3], 5u);
  CHECK_EQ(listed[4], 6u);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_enqueue, image_and_buffer_copies_fills_and_migrations_without_an_event_send_one_command_and_hand_none_back)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_mem image = run.image();
  CHECK(image != nullptr);
  const cl_event gate = run.gate();
  const std::uint32_t colour[4] = {1u, 2u, 3u, 4u};
  const cl_mem        moved[1] = {run.buffer()};

  std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().enqueue_copy_image_to_buffer(run.queue(), image, run.buffer(), origin, region, 0u, nullptr),
           CL_SUCCESS);
  CHECK_EQ(rig.root().enqueue_copy_buffer_to_image(run.queue(), run.buffer(), image, 0u, origin, region, nullptr),
           CL_SUCCESS);
  CHECK_EQ(rig.root().enqueue_fill_image(run.queue(), image, colour, origin, region, nullptr), CL_SUCCESS);
  CHECK_EQ(rig.root().enqueue_migrate(run.queue(), 1u, moved, 0u, nullptr), CL_SUCCESS);
  CHECK_EQ(rig.root().enqueue_copy_image(run.queue(), image, image, origin, apart, region, nullptr), CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(ClIcdRootProbe::context(rig.root(), run.context())->flush() == av::TransportResult::ok);
  CHECK(rig.kmt().commands() > sent);

  sent = rig.kmt().commands();
  const ClWaitList waits(1u, &gate);
  CHECK_EQ(rig.root().enqueue_copy_image_to_buffer(run.queue(), image, run.buffer(), origin, region, 0u, nullptr, waits),
           CL_SUCCESS);
  CHECK_EQ(rig.root().enqueue_copy_buffer_to_image(run.queue(), run.buffer(), image, 0u, origin, region, nullptr, waits),
           CL_SUCCESS);
  CHECK_EQ(rig.root().enqueue_fill_image(run.queue(), image, colour, origin, region, nullptr, waits), CL_SUCCESS);
  CHECK_EQ(rig.root().enqueue_migrate(run.queue(), 1u, moved, 0u, nullptr, waits), CL_SUCCESS);
  CHECK_EQ(rig.root().enqueue_copy_image(run.queue(), image, image, origin, apart, region, nullptr, waits),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(ClIcdRootProbe::context(rig.root(), run.context())->flush() == av::TransportResult::ok);
  CHECK(rig.kmt().commands() > sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_enqueue, a_copy_fill_or_migration_over_memory_of_no_context_or_a_bad_wait_list_sends_nothing)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_mem image = run.image();
  const cl_mem foreign = foreign_memory(rig);
  CHECK(image != nullptr && foreign != nullptr);
  if (image == nullptr || foreign == nullptr) return;
  int          not_an_event = 0;
  const cl_event stray = reinterpret_cast<cl_event>(&not_an_event);
  const ClWaitList bad(1u, &stray);
  const std::uint32_t colour[4] = {1u, 2u, 3u, 4u};
  const cl_mem        mixed[2] = {run.buffer(), foreign};
  const std::uint32_t sent = rig.kmt().commands();

  CHECK_EQ(rig.root().enqueue_copy_image_to_buffer(run.queue(), foreign, run.buffer(), origin, region, 0u, nullptr),
           CL_INVALID_CONTEXT);
  CHECK_EQ(rig.root().enqueue_copy_image_to_buffer(run.queue(), image, foreign, origin, region, 0u, nullptr),
           CL_INVALID_CONTEXT);
  CHECK_EQ(rig.root().enqueue_copy_image_to_buffer(run.queue(), image, run.buffer(), origin, nullptr, 0u, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_copy_image_to_buffer(run.queue(), image, run.buffer(), origin, region, 0u, nullptr, bad),
           CL_INVALID_EVENT_WAIT_LIST);
  CHECK_EQ(rig.root().enqueue_copy_buffer_to_image(run.queue(), foreign, image, 0u, origin, region, nullptr),
           CL_INVALID_CONTEXT);
  CHECK_EQ(rig.root().enqueue_copy_buffer_to_image(run.queue(), run.buffer(), foreign, 0u, origin, region, nullptr),
           CL_INVALID_CONTEXT);
  CHECK_EQ(rig.root().enqueue_copy_buffer_to_image(run.queue(), run.buffer(), image, 0u, nullptr, region, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_copy_buffer_to_image(run.queue(), run.buffer(), image, 0u, origin, region, nullptr, bad),
           CL_INVALID_EVENT_WAIT_LIST);
  CHECK_EQ(rig.root().enqueue_fill_image(run.queue(), foreign, colour, origin, region, nullptr), CL_INVALID_CONTEXT);
  CHECK_EQ(rig.root().enqueue_fill_image(run.queue(), image, colour, nullptr, region, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_fill_image(run.queue(), image, colour, origin, nullptr, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_fill_image(run.queue(), image, colour, origin, region, nullptr, bad),
           CL_INVALID_EVENT_WAIT_LIST);
  CHECK_EQ(rig.root().enqueue_migrate(run.queue(), 2u, mixed, 0u, nullptr), CL_INVALID_CONTEXT);
  CHECK_EQ(rig.root().enqueue_migrate(run.queue(), 1u, mixed, 0u, nullptr, bad), CL_INVALID_EVENT_WAIT_LIST);
  CHECK_EQ(rig.root().enqueue_migrate(run.queue(), 1u, nullptr, 0u, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_wait_for_events(run.queue(), 0u, &stray), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_wait_for_events(run.queue(), 1u, nullptr), CL_INVALID_VALUE);
  CHECK(rig.root().create_sub_buffer(foreign, CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION, nullptr, nullptr) ==
        nullptr);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

namespace {

constexpr std::uint64_t host_sub = 0x6B00u;
constexpr std::uint64_t host_pipe = 0x6C00u;
constexpr std::uint64_t host_shared = 0x0000'7F55'0000'1000ull;
constexpr std::size_t   shared_bytes = 256u;

}

TEST(cl_icd_root_enqueue, a_lost_context_refuses_every_copy_map_pipe_and_shared_memory_entry_at_once)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_mem image = run.image();
  rig.host().cl_handle(host_sub, CL_SUCCESS);
  const cl_buffer_region sub_region = {0u, 64u};
  const cl_mem           sub = rig.root().create_sub_buffer(run.buffer(), CL_MEM_READ_WRITE,
                                                            CL_BUFFER_CREATE_TYPE_REGION, &sub_region, nullptr);
  rig.host().cl_handle(host_pipe, CL_SUCCESS);
  const cl_mem pipe = rig.root().create_pipe(run.context(), CL_MEM_READ_WRITE, 4u, 4u, nullptr, nullptr);
  rig.host().cl_handle(host_shared, CL_SUCCESS);
  void *const mirror = rig.root().svm_alloc(run.context(), CL_MEM_READ_WRITE, shared_bytes, 0u);
  CHECK(image != nullptr && sub != nullptr && pipe != nullptr && mirror != nullptr);
  if (image == nullptr || sub == nullptr || pipe == nullptr || mirror == nullptr) return;

  run.lose();
  const std::uint32_t sent = rig.kmt().commands();
  const std::uint32_t colour[4] = {1u, 2u, 3u, 4u};
  const cl_mem        moved[1] = {run.buffer()};
  CHECK_EQ(rig.root().enqueue_copy_image_to_buffer(run.queue(), image, run.buffer(), origin, region, 0u, nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().enqueue_copy_buffer_to_image(run.queue(), run.buffer(), image, 0u, origin, region, nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().enqueue_fill_image(run.queue(), image, colour, origin, region, nullptr), CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().enqueue_migrate(run.queue(), 1u, moved, 0u, nullptr), CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().enqueue_copy_image(run.queue(), image, image, origin, apart, region, nullptr),
           CL_OUT_OF_RESOURCES);

  cl_int error = CL_SUCCESS;
  CHECK(rig.root().create_sub_buffer(run.buffer(), CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION, &sub_region,
                                     &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  std::size_t row_pitch = 0u, slice_pitch = 0u;
  error = CL_SUCCESS;
  CHECK(rig.root().map_image(run.queue(), image, CL_TRUE, CL_MAP_READ, origin, region, &row_pitch, &slice_pitch,
                             nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  error = CL_SUCCESS;
  CHECK(rig.root().create_pipe(run.context(), CL_MEM_READ_WRITE, 4u, 4u, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  cl_uint packet = 0u;
  CHECK_EQ(rig.root().pipe_info(pipe, CL_PIPE_PACKET_SIZE, sizeof packet, &packet, nullptr), CL_OUT_OF_RESOURCES);

  unsigned char from[8] = {}, into[8] = {};
  const std::uint32_t pattern = 0x5A5A5A5Au;
  void *pointers[1] = {mirror};
  CHECK_EQ(rig.root().svm_memcpy(run.queue(), CL_TRUE, into, from, sizeof from, nullptr), CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().svm_mem_fill(run.queue(), mirror, &pattern, sizeof pattern, 64u, nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().enqueue_svm_free(run.queue(), 1u, pointers, nullptr, nullptr, nullptr), CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_enqueue, a_queue_that_belongs_to_no_context_runs_no_map_copy_fill_free_or_migration)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_mem image = run.image();
  ClObject *const bare = ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::command_queue, 0x6501u);
  CHECK(image != nullptr && bare != nullptr);
  if (image == nullptr || bare == nullptr) return;
  const cl_command_queue queue = reinterpret_cast<cl_command_queue>(bare);
  const std::uint32_t    sent = rig.kmt().commands();
  std::size_t            row_pitch = 0u, slice_pitch = 0u;
  cl_int                 error = CL_SUCCESS;
  CHECK(rig.root().map_image(queue, image, CL_TRUE, CL_MAP_READ, origin, region, &row_pitch, &slice_pitch, nullptr,
                             &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  unsigned char       from[8] = {}, into[8] = {};
  const std::uint32_t pattern = 0x5A5A5A5Au;
  void               *pointers[1] = {into};
  const void         *migrated[1] = {into};
  CHECK_EQ(rig.root().svm_memcpy(queue, CL_TRUE, into, from, sizeof from, nullptr), CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().svm_mem_fill(queue, into, &pattern, sizeof pattern, 4u, nullptr), CL_INVALID_CONTEXT);
  CHECK_EQ(rig.root().enqueue_svm_free(queue, 1u, pointers, nullptr, nullptr, nullptr), CL_INVALID_CONTEXT);
  CHECK_EQ(rig.root().enqueue_svm_migrate(queue, 1u, migrated, nullptr), CL_INVALID_CONTEXT);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_enqueue, shared_memory_calls_the_api_would_refuse_send_nothing)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  rig.host().cl_handle(host_shared, CL_SUCCESS);
  std::uint8_t *const mirror = static_cast<std::uint8_t *>(rig.root().svm_alloc(run.context(), CL_MEM_READ_WRITE, shared_bytes, 0u));
  CHECK(mirror != nullptr);
  if (mirror == nullptr) return;
  int                  not_an_event = 0;
  const cl_event       stray = reinterpret_cast<cl_event>(&not_an_event);
  const ClWaitList     bad(1u, &stray);
  unsigned char        guest[16] = {};
  const std::uint32_t  pattern = 0x5A5A5A5Au;
  void                *pointers[1] = {mirror};
  void                *with_null[2] = {mirror, nullptr};
  const void          *outside[1] = {guest};
  const std::uint32_t  sent = rig.kmt().commands();

  CHECK_EQ(rig.root().svm_memcpy(run.queue(), CL_TRUE, guest, nullptr, 4u, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().svm_memcpy(run.queue(), CL_TRUE, guest, mirror, 4u, nullptr, bad), CL_INVALID_EVENT_WAIT_LIST);
  CHECK_EQ(rig.root().svm_memcpy(run.queue(), CL_TRUE, guest, mirror + shared_bytes - 2u, 8u, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().svm_mem_fill(run.queue(), mirror, &pattern, 0u, 64u, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().svm_mem_fill(run.queue(), mirror, &pattern, sizeof pattern, 0u, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().svm_mem_fill(run.queue(), mirror, &pattern, sizeof pattern, 64u, nullptr, bad),
           CL_INVALID_EVENT_WAIT_LIST);
  CHECK_EQ(rig.root().enqueue_svm_free(run.queue(), 0u, pointers, nullptr, nullptr, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_svm_free(run.queue(), 1u, nullptr, nullptr, nullptr, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_svm_free(run.queue(), 2u, with_null, nullptr, nullptr, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_svm_free(run.queue(), 1u, pointers, nullptr, nullptr, nullptr, bad),
           CL_INVALID_EVENT_WAIT_LIST);
  CHECK_EQ(rig.root().enqueue_svm_free(reinterpret_cast<cl_command_queue>(run.kernel()), 1u, pointers, nullptr,
                                       nullptr, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().enqueue_svm_migrate(run.queue(), 0u, nullptr, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_svm_migrate(run.queue(), 1u, nullptr, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_svm_migrate(run.queue(), 1u, outside, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_svm_migrate(reinterpret_cast<cl_command_queue>(run.kernel()), 1u, outside, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().svm_mem_fill(reinterpret_cast<cl_command_queue>(run.kernel()), mirror, &pattern,
                                   sizeof pattern, 64u, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.kmt().commands(), sent);

  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().svm_memcpy(run.queue(), CL_FALSE, guest, guest + 8, 0u, nullptr), CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

namespace {

alignas(4096) std::uint8_t lent_block[av::clicd::ClIcdRoot::lent_minimum];
std::uint8_t               own_block[256];

constexpr std::uint64_t host_over_own = 0x6D00u;
constexpr std::uint64_t host_over_lent = 0x6E00u;

}

TEST(cl_icd_root_enqueue, a_sub_buffer_over_the_applications_memory_or_memory_lent_to_the_host_is_over_the_same_bytes)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  rig.host().cl_handle(host_over_own, CL_SUCCESS);
  cl_int       error = CL_INVALID_VALUE;
  const cl_mem own = rig.root().create_buffer(run.context(), CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, sizeof own_block,
                                              own_block, nullptr, &error);
  CHECK(own != nullptr);
  rig.host().blob_import(0u, 0x0000700000000001ull);
  rig.host().cl_handle(host_over_lent, CL_SUCCESS);
  const cl_mem lent = rig.root().create_buffer(run.context(), CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
                                               sizeof lent_block, lent_block, nullptr, &error);
  CHECK(lent != nullptr);
  if (own == nullptr || lent == nullptr) return;

  const cl_buffer_region inside_own = {128u, 32u};
  const cl_uint align_bits = 1024u;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&align_bits), sizeof align_bits);
  rig.host().cl_handle(host_sub, CL_SUCCESS);
  const cl_mem sub_of_own = rig.root().create_sub_buffer(own, CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION,
                                                         &inside_own, &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(sub_of_own != nullptr);
  void *host_pointer = nullptr;
  if (sub_of_own != nullptr) {
      CHECK_EQ(rig.root().mem_object_info(sub_of_own, CL_MEM_HOST_PTR, sizeof host_pointer, &host_pointer, nullptr),
               CL_SUCCESS);
      CHECK(host_pointer == own_block + 128);
  }

  const cl_buffer_region inside_lent = {4096u, 4096u};
  rig.host().cl_handle(host_sub + 1u, CL_SUCCESS);
  const cl_mem sub_of_lent = rig.root().create_sub_buffer(lent, CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION,
                                                          &inside_lent, &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(sub_of_lent != nullptr);
  if (sub_of_lent != nullptr) {
      host_pointer = nullptr;
      CHECK_EQ(rig.root().mem_object_info(sub_of_lent, CL_MEM_HOST_PTR, sizeof host_pointer, &host_pointer, nullptr),
               CL_SUCCESS);
      CHECK(host_pointer == lent_block + 4096);
  }
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_enqueue, a_sub_buffer_the_host_does_not_make_or_the_guest_cannot_name_is_an_error_and_holds_nothing)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_buffer_region sub_region = {0u, 64u};
  cl_int                 error = CL_SUCCESS;
  rig.host().cl_handle(0u, CL_SUCCESS);
  CHECK(rig.root().create_sub_buffer(run.buffer(), CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION, &sub_region,
                                     &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  cl_uint count = 0u;
  CHECK_EQ(rig.root().mem_object_info(run.buffer(), CL_MEM_REFERENCE_COUNT, sizeof count, &count, nullptr),
           CL_SUCCESS);
  CHECK_EQ(count, 1u);

  rig.host().cl_handle(host_sub, CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  av::clicd::ClIcdRootProbe::fill_the_object_pool(rig);
  rig.platform().fail_allocation_at(1u);
  CHECK(rig.root().create_sub_buffer(run.buffer(), CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION, &sub_region,
                                     &error) == nullptr);
  CHECK(error != CL_SUCCESS);
  CHECK_EQ(rig.root().mem_object_info(run.buffer(), CL_MEM_REFERENCE_COUNT, sizeof count, &count, nullptr),
           CL_SUCCESS);
  CHECK_EQ(count, 1u);
  CHECK(rig.root().invariants_hold()); }

namespace {

class SharedRig {
public:
    SharedRig() : rig_(), run_(rig_), mirror_(nullptr)
    {
        if (!run_.made()) return;
        rig_.host().cl_handle(host_shared, CL_SUCCESS);
        mirror_ = static_cast<std::uint8_t *>(rig_.root().svm_alloc(run_.context(), CL_MEM_READ_WRITE, shared_bytes, 0u));
    }

    bool          made() const { return run_.made() && mirror_ != nullptr; }
    ClIcdRig     &rig() { return rig_; }
    ClRunningRig &run() { return run_; }
    std::uint8_t *mirror() { return mirror_; }

    cl_int into_shared() { return rig_.root().svm_memcpy(run_.queue(), CL_TRUE, mirror_, guest_, sizeof guest_, nullptr); }
    cl_int out_of_shared() { return rig_.root().svm_memcpy(run_.queue(), CL_TRUE, guest_, mirror_, sizeof guest_, nullptr); }

private:
    ClIcdRig      rig_;
    ClRunningRig  run_;
    std::uint8_t *mirror_;
    std::uint8_t  guest_[16] = {1u, 2u, 3u};
};

constexpr std::uint64_t host_scratch = 0x7100u;
constexpr std::uint64_t host_mapped = 0x0000'7F00'0000'9000ull;

}

namespace {

enum class Direction { into_shared, out_of_shared };

cl_int copy_through(SharedRig &shared, Direction direction)
{
    return direction == Direction::into_shared ? shared.into_shared() : shared.out_of_shared();
}

void scratch_buffer_refused(Direction direction, std::int32_t error, cl_int expected)
{
    SharedRig shared;
    CHECK(shared.made());
    if (!shared.made()) return;
    shared.rig().host().cl_handle(0u, error);
    CHECK_EQ(copy_through(shared, direction), expected);
    CHECK_EQ(shared.rig().kmt().queued_answers(), 0u);
    CHECK(shared.rig().root().invariants_hold());
}

void scratch_map_refused(Direction direction, std::int32_t error, cl_int expected)
{
    SharedRig shared;
    CHECK(shared.made());
    if (!shared.made()) return;
    shared.rig().host().cl_handle(host_scratch, CL_SUCCESS);
    shared.rig().host().cl_handle(0u, error);
    if (direction == Direction::into_shared) shared.rig().host().cl_result(CL_SUCCESS);
    CHECK_EQ(copy_through(shared, direction), expected);
    CHECK_EQ(shared.rig().kmt().queued_answers(), 0u);
    CHECK(shared.rig().root().invariants_hold());
}

}

TEST(cl_icd_root_enqueue, a_copy_into_shared_memory_through_a_scratch_buffer_ends_with_the_first_thing_the_host_refuses)
{ scratch_buffer_refused(Direction::into_shared, CL_INVALID_BUFFER_SIZE, CL_INVALID_BUFFER_SIZE);
  scratch_buffer_refused(Direction::into_shared, CL_SUCCESS, CL_OUT_OF_RESOURCES);
  scratch_map_refused(Direction::into_shared, CL_INVALID_OPERATION, CL_INVALID_OPERATION);
  scratch_map_refused(Direction::into_shared, CL_SUCCESS, CL_MAP_FAILURE);
  SharedRig shared;
  CHECK(shared.made());
  if (!shared.made()) return;
  shared.rig().host().cl_handle(host_scratch, CL_SUCCESS);
  shared.rig().host().cl_handle(host_mapped, CL_SUCCESS);
  shared.rig().host().cl_result(CL_INVALID_OPERATION);
  shared.rig().host().cl_result(CL_SUCCESS);
  shared.rig().host().cl_result(CL_SUCCESS);
  CHECK_EQ(shared.into_shared(), CL_INVALID_OPERATION);
  CHECK_EQ(shared.rig().kmt().queued_answers(), 0u);
  CHECK(shared.rig().root().invariants_hold()); }

TEST(cl_icd_root_enqueue, a_copy_out_of_shared_memory_through_a_scratch_buffer_ends_with_the_first_thing_the_host_refuses)
{ scratch_buffer_refused(Direction::out_of_shared, CL_INVALID_BUFFER_SIZE, CL_INVALID_BUFFER_SIZE);
  scratch_buffer_refused(Direction::out_of_shared, CL_SUCCESS, CL_OUT_OF_RESOURCES);
  scratch_map_refused(Direction::out_of_shared, CL_INVALID_OPERATION, CL_INVALID_OPERATION);
  scratch_map_refused(Direction::out_of_shared, CL_SUCCESS, CL_MAP_FAILURE);
  SharedRig shared;
  CHECK(shared.made());
  if (!shared.made()) return;
  shared.rig().host().cl_handle(host_scratch, CL_SUCCESS);
  shared.rig().host().cl_handle(host_mapped, CL_SUCCESS);
  shared.rig().host().cl_result(CL_INVALID_OPERATION);
  shared.rig().host().cl_result(CL_SUCCESS);
  CHECK_EQ(shared.out_of_shared(), CL_INVALID_OPERATION);
  CHECK_EQ(shared.rig().kmt().queued_answers(), 0u);
  CHECK(shared.rig().root().invariants_hold()); }

TEST(cl_icd_root_enqueue, a_host_that_refuses_an_enqueue_that_asks_for_an_event_hands_back_no_event)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_mem        image = run.image();
  const std::uint32_t colour[4] = {1u, 2u, 3u, 4u};
  const cl_mem        moved[1] = {run.buffer()};
  cl_event            event = nullptr;
  rig.host().cl_enqueued(CL_INVALID_OPERATION, 0u);
  CHECK_EQ(rig.root().enqueue_copy_image_to_buffer(run.queue(), image, run.buffer(), origin, region, 0u, &event),
           CL_INVALID_OPERATION);
  rig.host().cl_enqueued(CL_INVALID_OPERATION, 0u);
  CHECK_EQ(rig.root().enqueue_copy_buffer_to_image(run.queue(), run.buffer(), image, 0u, origin, region, &event),
           CL_INVALID_OPERATION);
  rig.host().cl_enqueued(CL_INVALID_OPERATION, 0u);
  CHECK_EQ(rig.root().enqueue_fill_image(run.queue(), image, colour, origin, region, &event), CL_INVALID_OPERATION);
  rig.host().cl_enqueued(CL_INVALID_OPERATION, 0u);
  CHECK_EQ(rig.root().enqueue_migrate(run.queue(), 1u, moved, 0u, &event), CL_INVALID_OPERATION);
  rig.host().cl_enqueued(CL_INVALID_OPERATION, 0u);
  CHECK_EQ(rig.root().enqueue_copy_image(run.queue(), image, image, origin, apart, region, &event),
           CL_INVALID_OPERATION);
  CHECK(event == nullptr);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_enqueue, a_pipe_takes_no_properties_but_an_empty_list_and_the_host_may_refuse_to_make_it)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_pipe_properties none[] = {0};
  cl_int                   error = CL_INVALID_VALUE;
  rig.host().cl_handle(host_pipe, CL_SUCCESS);
  CHECK(rig.root().create_pipe(run.context(), CL_MEM_READ_WRITE, 16u, 8u, none, &error) != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  rig.host().cl_handle(0u, CL_SUCCESS);
  CHECK(rig.root().create_pipe(run.context(), CL_MEM_READ_WRITE, 16u, 8u, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

namespace {

template <typename Word>
void host_says(ClIcdRig &rig, Word value)
{
    rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&value), sizeof value);
}

const unsigned char texels[32] = {1u,  2u,  3u,  4u,  5u,  6u,  7u,  8u,  9u,  10u, 11u, 12u, 13u, 14u, 15u, 16u,
                                  17u, 18u, 19u, 20u, 21u, 22u, 23u, 24u, 25u, 26u, 27u, 28u, 29u, 30u, 31u, 32u};

void map_of_type(cl_mem_object_type type, std::size_t expect_row, std::size_t expect_slice)
{
    ClIcdRig     rig;
    ClRunningRig run(rig);
    CHECK(run.made());
    if (!run.made()) return;
    const cl_mem image = run.image();
    host_says(rig, type);
    host_says(rig, std::size_t{4u});
    rig.host().cl_bytes(CL_SUCCESS, texels, sizeof texels);
    std::size_t row_pitch = 0u, slice_pitch = 99u;
    cl_int      error = CL_INVALID_VALUE;
    unsigned char *const mapped = static_cast<unsigned char *>(rig.root().map_image(
        run.queue(), image, CL_TRUE, CL_MAP_READ, origin, region, &row_pitch, &slice_pitch, nullptr, &error));
    CHECK_EQ(error, CL_SUCCESS);
    CHECK(mapped != nullptr);
    CHECK_EQ(row_pitch, expect_row);
    CHECK_EQ(slice_pitch, expect_slice);
    CHECK_EQ(rig.kmt().queued_answers(), 0u);
    CHECK(rig.root().invariants_hold());
}

}

TEST(cl_icd_root_enqueue, a_map_of_an_image_with_slices_names_the_slice_pitch_by_the_images_kind)
{ map_of_type(CL_MEM_OBJECT_IMAGE3D, 16u, 32u);
  map_of_type(CL_MEM_OBJECT_IMAGE2D_ARRAY, 16u, 32u);
  map_of_type(CL_MEM_OBJECT_IMAGE1D_ARRAY, 16u, 16u); }

namespace {

struct Mapping {
    explicit Mapping(ClRunningRig &run, std::uint64_t handle) : run_(run), image_(run.image(handle)) {}

    void *map(const std::size_t *from, const std::size_t *extent, std::size_t *row, std::size_t *slice,
              const ClWaitList &waits = ClWaitList())
    {
        error_ = CL_SUCCESS;
        return run_.rig().root().map_image(run_.queue(), image_, CL_TRUE, CL_MAP_READ, from, extent, row, slice, nullptr,
                                           &error_, waits);
    }

    ClRunningRig &run_;
    cl_mem        image_;
    cl_int        error_ = CL_SUCCESS;
};

}

TEST(cl_icd_root_enqueue, a_map_the_host_the_layout_or_the_staging_room_refuses_is_an_error_and_holds_no_mapping)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  std::size_t    row_pitch = 0u, slice_pitch = 0u;
  int            not_an_event = 0;
  const cl_event stray = reinterpret_cast<cl_event>(&not_an_event);
  const std::size_t empty[3] = {0u, 2u, 1u};

  Mapping nameless(run, 0x7A00u);
  CHECK(nameless.map(nullptr, region, &row_pitch, &slice_pitch) == nullptr);
  CHECK_EQ(nameless.error_, CL_INVALID_VALUE);
  CHECK(nameless.map(origin, nullptr, &row_pitch, &slice_pitch) == nullptr);
  CHECK_EQ(nameless.error_, CL_INVALID_VALUE);

  Mapping unknown(run, 0x7A01u);
  rig.host().cl_info(CL_INVALID_MEM_OBJECT, nullptr, 0u);
  CHECK(unknown.map(origin, region, &row_pitch, &slice_pitch) == nullptr);
  CHECK_EQ(unknown.error_, CL_INVALID_MEM_OBJECT);

  Mapping pipe(run, 0x7A02u);
  host_says(rig, static_cast<cl_mem_object_type>(CL_MEM_OBJECT_PIPE));
  CHECK(pipe.map(origin, region, &row_pitch, &slice_pitch) == nullptr);
  CHECK_EQ(pipe.error_, CL_INVALID_MEM_OBJECT);

  Mapping volume(run, 0x7A03u);
  host_says(rig, static_cast<cl_mem_object_type>(CL_MEM_OBJECT_IMAGE3D));
  CHECK(volume.map(origin, region, &row_pitch, nullptr) == nullptr);
  CHECK_EQ(volume.error_, CL_INVALID_VALUE);

  Mapping flat(run, 0x7A04u);
  host_says(rig, static_cast<cl_mem_object_type>(CL_MEM_OBJECT_IMAGE2D));
  host_says(rig, std::size_t{4u});
  CHECK(flat.map(origin, empty, &row_pitch, &slice_pitch) == nullptr);
  CHECK_EQ(flat.error_, CL_INVALID_VALUE);
  host_says(rig, static_cast<cl_mem_object_type>(CL_MEM_OBJECT_IMAGE2D));
  CHECK(flat.map(origin, region, &row_pitch, &slice_pitch, ClWaitList(1u, &stray)) == nullptr);
  CHECK_EQ(flat.error_, CL_INVALID_EVENT_WAIT_LIST);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

namespace {

void map_refused_after_the_layout(std::uint32_t starved_at, bool read_refused, cl_int expected)
{
    ClIcdRig     rig;
    ClRunningRig run(rig);
    CHECK(run.made());
    if (!run.made()) return;
    std::size_t row_pitch = 0u, slice_pitch = 0u;
    Mapping     flat(run, 0x7A05u);
    host_says(rig, static_cast<cl_mem_object_type>(CL_MEM_OBJECT_IMAGE2D));
    host_says(rig, std::size_t{4u});
    if (read_refused) rig.host().cl_bytes(CL_INVALID_OPERATION, nullptr, 0u);
    if (starved_at != 0u) rig.platform().fail_allocation_at(starved_at);
    CHECK(flat.map(origin, region, &row_pitch, &slice_pitch) == nullptr);
    CHECK_EQ(flat.error_, expected);
    CHECK_EQ(rig.kmt().queued_answers(), 0u);
    CHECK(rig.root().invariants_hold());
}

}

TEST(cl_icd_root_enqueue, a_map_with_no_staging_room_or_whose_rows_the_host_refuses_to_read_holds_no_mapping)
{ map_refused_after_the_layout(0u, true, CL_INVALID_OPERATION);
  bool starved = false;
  for (std::uint32_t nth = 1u; nth <= 6u; ++nth) {
      ClIcdRig     rig;
      ClRunningRig run(rig);
      CHECK(run.made());
      if (!run.made()) return;
      Mapping flat(run, 0x7A06u);
      std::size_t row_pitch = 0u, slice_pitch = 0u;
      host_says(rig, static_cast<cl_mem_object_type>(CL_MEM_OBJECT_IMAGE2D));
      host_says(rig, std::size_t{4u});
      rig.platform().fail_allocation_at(nth);
      CHECK(flat.map(origin, region, &row_pitch, &slice_pitch) == nullptr);
      CHECK(flat.error_ == CL_OUT_OF_HOST_MEMORY || flat.error_ == CL_OUT_OF_RESOURCES);
      starved = starved || flat.error_ == CL_OUT_OF_HOST_MEMORY;
      CHECK(rig.root().invariants_hold());
  }
  CHECK(starved); }

TEST(cl_icd_root_enqueue, an_image_mapped_to_read_only_is_unmapped_without_a_write_and_a_failed_drain_is_the_unmaps_error)
{ { ClIcdRig     rig;
    ClRunningRig run(rig);
    CHECK(run.made());
    if (!run.made()) return;
    const cl_mem image = run.image();
    host_says(rig, static_cast<cl_mem_object_type>(CL_MEM_OBJECT_IMAGE2D));
    host_says(rig, std::size_t{4u});
    rig.host().cl_bytes(CL_SUCCESS, texels, sizeof texels);
    std::size_t row_pitch = 0u;
    cl_int      error = CL_INVALID_VALUE;
    void *const mapped = rig.root().map_image(run.queue(), image, CL_TRUE, CL_MAP_READ, origin, region, &row_pitch,
                                              nullptr, nullptr, &error);
    CHECK(mapped != nullptr);
    const std::uint32_t sent = rig.kmt().commands();
    CHECK_EQ(rig.root().unmap(run.queue(), image, mapped, nullptr), CL_SUCCESS);
    CHECK_EQ(rig.kmt().commands(), sent);
    CHECK(rig.root().invariants_hold()); }
  { ClIcdRig     rig;
    ClRunningRig run(rig);
    CHECK(run.made());
    if (!run.made()) return;
    const cl_mem image = run.image();
    host_says(rig, static_cast<cl_mem_object_type>(CL_MEM_OBJECT_IMAGE2D));
    host_says(rig, std::size_t{4u});
    rig.host().cl_bytes(CL_SUCCESS, texels, sizeof texels);
    std::size_t row_pitch = 0u;
    cl_int      error = CL_INVALID_VALUE;
    void *const mapped = rig.root().map_image(run.queue(), image, CL_TRUE, CL_MAP_WRITE, origin, region, &row_pitch,
                                              nullptr, nullptr, &error);
    CHECK(mapped != nullptr);
    CHECK(run.queue_event() != nullptr);
    rig.host().cl_enqueued(CL_INVALID_OPERATION, 0u);
    CHECK_EQ(rig.root().unmap(run.queue(), image, mapped, nullptr), CL_INVALID_OPERATION);
    CHECK_EQ(rig.kmt().queued_answers(), 0u);
    CHECK(rig.root().invariants_hold()); } }

namespace {

constexpr std::size_t big_shared_bytes = 64u * 1024u;
std::uint8_t          big_guest[big_shared_bytes];

}

TEST(cl_icd_root_enqueue, a_copy_into_shared_memory_whose_staged_bytes_the_host_refuses_gives_the_scratch_buffer_back)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  rig.host().cl_handle(host_shared, CL_SUCCESS);
  void *const mirror = rig.root().svm_alloc(run.context(), CL_MEM_READ_WRITE, big_shared_bytes, 0u);
  CHECK(mirror != nullptr);
  if (mirror == nullptr) return;
  rig.host().cl_handle(host_scratch, CL_SUCCESS);
  rig.host().cl_result(CL_INVALID_OPERATION);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().svm_memcpy(run.queue(), CL_TRUE, mirror, big_guest, big_shared_bytes, nullptr),
           CL_INVALID_OPERATION);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

namespace {

struct TwoMirrors {
    TwoMirrors() : rig(), run(rig), first(nullptr), second(nullptr)
    {
        if (!run.made()) return;
        rig.host().cl_handle(host_shared, CL_SUCCESS);
        first = static_cast<std::uint8_t *>(rig.root().svm_alloc(run.context(), CL_MEM_READ_WRITE, shared_bytes, 0u));
        rig.host().cl_handle(host_shared + 0x1000u, CL_SUCCESS);
        second = static_cast<std::uint8_t *>(rig.root().svm_alloc(run.context(), CL_MEM_READ_WRITE, shared_bytes, 0u));
    }

    bool made() const { return run.made() && first != nullptr && second != nullptr; }

    ClIcdRig      rig;
    ClRunningRig  run;
    std::uint8_t *first;
    std::uint8_t *second;
};

}

TEST(cl_icd_root_enqueue, shared_to_shared_copies_and_fills_run_without_an_event_or_a_wait)
{ TwoMirrors shared;
  CHECK(shared.made());
  if (!shared.made()) return;
  ClIcdRig &rig = shared.rig;
  rig.host().cl_result(CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().svm_memcpy(shared.run.queue(), CL_FALSE, shared.second, shared.first, 16u, nullptr), CL_SUCCESS);
  const std::uint32_t pattern = 0x5A5A5A5Au;
  CHECK_EQ(rig.root().svm_mem_fill(shared.run.queue(), shared.first, &pattern, sizeof pattern, 64u, nullptr), CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK(ClIcdRootProbe::context(rig.root(), shared.run.context())->flush() == av::TransportResult::ok);
  CHECK(rig.kmt().commands() > sent + 1u);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_enqueue, a_shared_to_shared_copy_the_host_refuses_hands_back_no_event)
{ TwoMirrors shared;
  CHECK(shared.made());
  if (!shared.made()) return;
  cl_event event = nullptr;
  shared.rig.host().cl_enqueued(CL_INVALID_OPERATION, 0u);
  CHECK_EQ(shared.rig.root().svm_memcpy(shared.run.queue(), CL_FALSE, shared.second, shared.first, 16u, &event),
           CL_INVALID_OPERATION);
  CHECK(event == nullptr);
  CHECK(shared.rig.root().invariants_hold()); }

TEST(cl_icd_root_enqueue, a_copy_between_shared_and_guest_memory_whose_drain_the_host_refuses_is_the_copys_error)
{ TwoMirrors shared;
  CHECK(shared.made());
  if (!shared.made()) return;
  unsigned char guest[16] = {};
  const cl_event gate = shared.run.gate();
  shared.rig.host().cl_enqueued(CL_INVALID_OPERATION, 0u);
  CHECK_EQ(shared.rig.root().svm_memcpy(shared.run.queue(), CL_TRUE, shared.second, guest, sizeof guest, nullptr,
                                        ClWaitList(1u, &gate, CL_TRUE)),
           CL_INVALID_OPERATION);
  CHECK(shared.rig.root().invariants_hold()); }

TEST(cl_icd_root_enqueue, a_shared_fill_the_host_refuses_hands_back_no_event)
{ TwoMirrors shared;
  CHECK(shared.made());
  if (!shared.made()) return;
  const std::uint32_t pattern = 1u;
  cl_event            event = nullptr;
  shared.rig.host().cl_enqueued(CL_INVALID_OPERATION, 0u);
  CHECK_EQ(shared.rig.root().svm_mem_fill(shared.run.queue(), shared.first, &pattern, sizeof pattern, 64u, &event),
           CL_INVALID_OPERATION);
  CHECK(event == nullptr);
  CHECK(shared.rig.root().invariants_hold()); }

TEST(cl_icd_root_enqueue, a_copy_between_shared_and_guest_memory_behind_a_bad_wait_list_sends_nothing)
{ TwoMirrors shared;
  CHECK(shared.made());
  if (!shared.made()) return;
  int            not_an_event = 0;
  const cl_event stray = reinterpret_cast<cl_event>(&not_an_event);
  unsigned char  guest[16] = {};
  const std::uint32_t sent = shared.rig.kmt().commands();
  CHECK_EQ(shared.rig.root().svm_memcpy(shared.run.queue(), CL_TRUE, shared.second, guest, sizeof guest, nullptr,
                                        ClWaitList(1u, &stray)),
           CL_INVALID_EVENT_WAIT_LIST);
  CHECK_EQ(shared.rig.root().svm_memcpy(shared.run.queue(), CL_TRUE, guest, shared.first, sizeof guest, nullptr,
                                        ClWaitList(1u, &stray)),
           CL_INVALID_EVENT_WAIT_LIST);
  CHECK_EQ(shared.rig.kmt().commands(), sent);
  CHECK(shared.rig.root().invariants_hold()); }

TEST(cl_icd_root_enqueue, a_copy_into_or_out_of_a_buffer_that_is_no_buffer_asks_the_host_nothing)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_mem image = run.image();
  const cl_mem not_memory = reinterpret_cast<cl_mem>(run.kernel());
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().enqueue_copy_image_to_buffer(run.queue(), image, not_memory, origin, region, 0u, nullptr),
           CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().enqueue_copy_buffer_to_image(run.queue(), not_memory, image, 0u, origin, region, nullptr),
           CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.kmt().commands(), sent); }

namespace {

std::uint32_t count_commands(const av::MockKmt &kmt, std::uint32_t from, std::uint32_t command)
{
    std::uint32_t counted = 0u;
    for (std::uint32_t index = from; index < kmt.commands(); ++index)
        if (kmt.command_id(index) == command) ++counted;
    return counted;
}

std::uint32_t position_of(const av::MockKmt &kmt, std::uint32_t from, std::uint32_t command)
{
    for (std::uint32_t index = from; index < kmt.commands(); ++index)
        if (kmt.command_id(index) == command) return index;
    return ~std::uint32_t{0};
}

}

TEST(cl_icd_root_enqueue, a_queue_whose_command_waited_on_a_user_event_that_failed_gets_a_new_host_queue_before_its_next_command)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_event gate = run.gate();
  rig.host().cl_enqueued(CL_SUCCESS, ClRunningRig::host_event);
  cl_event victim = nullptr;
  CHECK_EQ(rig.root().enqueue_marker(run.queue(), 1u, &gate, &victim), CL_SUCCESS);

  const std::uint32_t before_failing = rig.kmt().commands();
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().set_user_event_status(gate, -5), CL_SUCCESS);
  CHECK_EQ(count_commands(rig.kmt(), before_failing, av::clproto::kCmd_clCreateCommandQueueWithProperties), 0u);

  const std::uint32_t before_next = rig.kmt().commands();
  const cl_int        complete = CL_COMPLETE;
  rig.host().cl_enqueued(CL_SUCCESS, ClRunningRig::host_event + 1u);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_handle(ClRunningRig::host_queue + 1u, CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_enqueued(CL_SUCCESS, ClRunningRig::host_event + 2u);
  cl_event next = nullptr;
  CHECK_EQ(rig.root().enqueue_marker(run.queue(), 0u, nullptr, &next), CL_SUCCESS);
  CHECK(ClIcdRootProbe::context(rig.root(), run.context())->flush() == av::TransportResult::ok);
  CHECK_EQ(count_commands(rig.kmt(), before_next, av::clproto::kCmd_clCreateCommandQueueWithProperties), 1u);
  CHECK_EQ(count_commands(rig.kmt(), before_next, av::clproto::kCmd_clReleaseCommandQueue), 1u);
  CHECK(position_of(rig.kmt(), before_next, av::clproto::kCmd_clCreateCommandQueueWithProperties) <
        position_of(rig.kmt(), before_next, av::clproto::kCmd_clReleaseCommandQueue));
  CHECK_EQ(rig.kmt().queued_answers(), 0u);

  const std::uint32_t before_again = rig.kmt().commands();
  rig.host().cl_enqueued(CL_SUCCESS, ClRunningRig::host_event + 3u);
  CHECK_EQ(rig.root().enqueue_marker(run.queue(), 0u, nullptr, nullptr), CL_SUCCESS);
  CHECK_EQ(count_commands(rig.kmt(), before_again, av::clproto::kCmd_clCreateCommandQueueWithProperties), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root_enqueue, a_queue_that_never_waited_on_a_user_event_keeps_its_host_queue_when_one_fails)
{ ClIcdRig     rig;
  ClRunningRig run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const std::uint32_t before = rig.kmt().commands();
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().set_user_event_status(run.gate(), -5), CL_SUCCESS);
  rig.host().cl_enqueued(CL_SUCCESS, ClRunningRig::host_event);
  CHECK_EQ(rig.root().enqueue_marker(run.queue(), 0u, nullptr, nullptr), CL_SUCCESS);
  CHECK_EQ(count_commands(rig.kmt(), before, av::clproto::kCmd_clCreateCommandQueueWithProperties), 0u);
  CHECK_EQ(count_commands(rig.kmt(), before, av::clproto::kCmd_clReleaseCommandQueue), 0u);
  CHECK(rig.root().invariants_hold()); }
