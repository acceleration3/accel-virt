/* The kernels and the transfers, and the event each of them may be asked for.
 *
 * Every enqueue has two shapes: one the application wants an event back from
 * and one it does not. They are different paths - an event asked for is one
 * more guest object to make and hand over, and an event not asked for is one
 * the ICD may still have made for itself and has to give back - so both are
 * driven here for every enqueue that has them.
 *
 * A kernel argument is the other thing worth its own case: an argument the
 * size of a cl_mem may be one, in which case what crosses is the number the
 * host knows that object by rather than the guest's pointer, and anything
 * else is bytes. Getting that backwards would send a guest address to a host
 * that would use it. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "harness/icd/fake_cl_loader.hpp"
#include "icd/opencl/cl_icd_root.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"

#include <CL/cl_ext.h>
#include <cstdint>

using av::ClIcdRig;
using av::FakeClLoader;

namespace {

constexpr std::uint64_t host_platform = 0x6200u;
constexpr std::uint64_t host_device = 0x6300u;
constexpr std::uint64_t host_context = 0x6400u;
constexpr std::uint64_t host_queue = 0x6500u;
constexpr std::uint64_t host_buffer = 0x6600u;
constexpr std::uint64_t host_program = 0x6700u;
constexpr std::uint64_t host_kernel = 0x6800u;
constexpr std::uint64_t host_event = 0x6900u;

/* A context, a queue, a buffer, a program and a kernel: everything an enqueue
 * needs, so each test below is about the enqueue and not the setup. */
class Running {
public:
    explicit Running(ClIcdRig &rig)
        : rig_(rig), device_(nullptr), context_(nullptr), queue_(nullptr), buffer_(nullptr),
          program_(nullptr), kernel_(nullptr)
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
        rig_.host().cl_handle(host_context, CL_SUCCESS);
        context_ = rig_.loader().create_context(device_, nullptr);
        if (context_ == nullptr) return;
        rig_.host().cl_handle(host_queue, CL_SUCCESS);
        queue_ = rig_.loader().create_queue(context_, device_, nullptr);
        rig_.host().cl_handle(host_buffer, CL_SUCCESS);
        buffer_ = rig_.loader().create_buffer(context_, CL_MEM_READ_WRITE, 256u, nullptr);
        rig_.host().cl_handle(host_program, CL_SUCCESS);
        program_ = rig_.loader().create_program(context_, "__kernel void k(void){}", nullptr);
        if (program_ == nullptr) return;
        rig_.host().cl_handle(host_kernel, CL_SUCCESS);
        kernel_ = rig_.loader().create_kernel(program_, "k", nullptr);
    }

    bool             made() const { return kernel_ != nullptr && buffer_ != nullptr; }
    cl_context       context() const { return context_; }
    cl_command_queue queue() const { return queue_; }
    cl_mem           buffer() const { return buffer_; }
    cl_program       program() const { return program_; }
    cl_kernel        kernel() const { return kernel_; }

private:
    ClIcdRig        &rig_;
    cl_device_id     device_;
    cl_context       context_;
    cl_command_queue queue_;
    cl_mem           buffer_;
    cl_program       program_;
    cl_kernel        kernel_;
};

/* A callback that is never run: what is being tested is the registration, not
 * what it does. */
void nothing_happened(cl_event, cl_int, void *) {}

}  // namespace

TEST(cl_enqueue_paths, a_kernel_of_no_name_or_of_something_that_is_not_a_program_is_refused)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const std::uint32_t sent = rig.kmt().commands();
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().create_kernel(reinterpret_cast<cl_program>(run.queue()), "k", &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_PROGRAM);
  error = CL_SUCCESS;
  CHECK(rig.root().create_kernel(run.program(), nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent);

  /* A host that has no such kernel says so; one that answers success with no
   * kernel has contradicted itself, and the API's word for a kernel that is
   * not there is what the application gets. */
  rig.host().cl_handle(0u, CL_INVALID_KERNEL_NAME);
  error = CL_SUCCESS;
  CHECK(rig.root().create_kernel(run.program(), "absent", &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_KERNEL_NAME);
  rig.host().cl_handle(0u, CL_SUCCESS);
  error = CL_SUCCESS;
  CHECK(rig.root().create_kernel(run.program(), "k", &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_KERNEL_NAME);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_paths, an_argument_the_size_of_a_buffer_crosses_as_the_host_s_number_for_it)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;

  /* The application passes the address of its own cl_mem, and what crosses
   * is the number the host knows that buffer by - never the guest pointer,
   * which the host would happily use as an address. */
  const cl_mem        named = run.buffer();
  const std::uint32_t sent = rig.kmt().commands();
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().set_kernel_argument(run.kernel(), 0u, sizeof named, &named), CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK_EQ(rig.kmt().command_id(sent),
           static_cast<std::uint32_t>(av::clproto::kCmd_clSetKernelArg));

  /* The same size, but not a buffer this ICD made: it is bytes, and they
   * cross as bytes. A pointer-shaped value the application invented must not
   * be mistaken for an object. */
  void *const invented = reinterpret_cast<void *>(std::uintptr_t{0x1234});
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().set_kernel_argument(run.kernel(), 1u, sizeof invented, &invented),
           CL_SUCCESS);

  /* A local-memory argument: a size and no value at all. */
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().set_kernel_argument(run.kernel(), 2u, 64u, nullptr), CL_SUCCESS);

  /* And a scalar, which is neither. */
  const std::uint32_t scalar = 7u;
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().set_kernel_argument(run.kernel(), 3u, sizeof scalar, &scalar), CL_SUCCESS);

  /* Something that is not a kernel is refused before any of that. */
  CHECK_EQ(rig.root().set_kernel_argument(reinterpret_cast<cl_kernel>(run.queue()), 0u,
                                          sizeof named, &named),
           CL_INVALID_KERNEL);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_paths, a_kernel_enqueued_with_an_event_hands_one_back_and_without_hands_none)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const std::size_t global = 64u;

  /* Not a queue, and not a kernel. */
  CHECK_EQ(rig.root().enqueue_kernel(reinterpret_cast<cl_command_queue>(run.context()),
                                     run.kernel(), 1u, nullptr, &global, nullptr, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().enqueue_kernel(run.queue(), reinterpret_cast<cl_kernel>(run.context()), 1u,
                                     nullptr, &global, nullptr, nullptr),
           CL_INVALID_KERNEL);

  /* Asked for: the application is handed an object of its own. */
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  cl_event event = nullptr;
  CHECK_EQ(rig.root().enqueue_kernel(run.queue(), run.kernel(), 1u, nullptr, &global, nullptr,
                                     &event),
           CL_SUCCESS);
  CHECK(event != nullptr);

  /* Not asked for: nothing is handed over, and nothing is left behind. */
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 1u);
  CHECK_EQ(rig.root().enqueue_kernel(run.queue(), run.kernel(), 1u, nullptr, &global, nullptr,
                                     nullptr),
           CL_SUCCESS);

  /* A host that refuses the enqueue makes no event whether one was asked for
   * or not, and the application's own is left as it was. */
  rig.host().cl_enqueued(CL_OUT_OF_RESOURCES, 0u);
  cl_event refused = nullptr;
  CHECK_EQ(rig.root().enqueue_kernel(run.queue(), run.kernel(), 1u, nullptr, &global, nullptr,
                                     &refused),
           CL_OUT_OF_RESOURCES);
  CHECK(refused == nullptr);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_paths, a_transfer_of_nothing_or_to_nowhere_is_refused_before_it_is_sent)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  unsigned char bytes[8] = {};
  const std::uint32_t sent = rig.kmt().commands();

  /* Not a queue, not a buffer, and nowhere to read into or write from. */
  CHECK_EQ(rig.root().enqueue_read(reinterpret_cast<cl_command_queue>(run.context()), run.buffer(),
                                   CL_TRUE, 0u, sizeof bytes, bytes, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().enqueue_read(run.queue(), reinterpret_cast<cl_mem>(run.queue()), CL_TRUE, 0u,
                                   sizeof bytes, bytes, nullptr),
           CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_TRUE, 0u, sizeof bytes, nullptr,
                                   nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_write(reinterpret_cast<cl_command_queue>(run.context()), run.buffer(),
                                    CL_TRUE, 0u, sizeof bytes, bytes, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().enqueue_write(run.queue(), reinterpret_cast<cl_mem>(run.queue()), CL_TRUE, 0u,
                                    sizeof bytes, bytes, nullptr),
           CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().enqueue_write(run.queue(), run.buffer(), CL_TRUE, 0u, sizeof bytes, nullptr,
                                    nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_paths, a_write_the_host_refuses_makes_no_event_and_is_not_waited_for)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  unsigned char bytes[8] = {};

  /* The enqueue failed, so there is nothing to wait for and nothing to hand
   * back - a wait here would be a wait on an event that was never made. */
  rig.host().cl_enqueued(CL_MEM_OBJECT_ALLOCATION_FAILURE, 0u);
  cl_event event = nullptr;
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().enqueue_write(run.queue(), run.buffer(), CL_TRUE, 0u, sizeof bytes, bytes,
                                    &event),
           CL_MEM_OBJECT_ALLOCATION_FAILURE);
  CHECK(event == nullptr);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_paths, a_non_blocking_read_the_marker_fails_for_never_becomes_a_row)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  unsigned char into[8] = {};

  /* The marker is what a deferred read is named by. A host that will not
   * make one leaves nothing to record against, so the call fails rather than
   * recording a read nobody could ever deliver. */
  rig.host().cl_enqueued(CL_OUT_OF_RESOURCES, 0u);
  CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_FALSE, 0u, sizeof into, into,
                                   nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_paths, a_blocking_read_the_host_refuses_answers_its_error)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  unsigned char into[8] = {};

  /* A blocking read is sent as a read the ICD waits for itself, so a host
   * that refuses the enqueue ends it there - before the flush and before
   * anything is waited on. */
  rig.host().cl_bytes_for(CL_OUT_OF_RESOURCES, nullptr, 0u, 0u);
  CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_TRUE, 0u, sizeof into, into,
                                   nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK(rig.root().invariants_hold()); }

/* ---- events, and the delivery a status query triggers -------------------- */

/* A read the application polls for rather than waits on: asking the event's
 * status is where it learns the read is done, so it is where the bytes have
 * to be there. Every other query, and every other answer, leaves the read
 * where it was - delivering on a status of 'running' would write into the
 * application's memory while it is still entitled to use it. */
TEST(cl_enqueue_paths, only_a_completed_execution_status_delivers_a_deferred_read)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  unsigned char into[8] = {};

  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  cl_event event = nullptr;
  CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_FALSE, 0u, sizeof into, into,
                                   &event),
           CL_SUCCESS);
  CHECK(event != nullptr);
  if (event == nullptr) return;

  /* Another query entirely: the read is not delivered. */
  cl_command_queue named = nullptr;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&named), sizeof named);
  CHECK_EQ(rig.root().event_info(event, CL_EVENT_COMMAND_QUEUE, sizeof named, &named, nullptr),
           CL_SUCCESS);

  /* The right query, asked only for its size: there is nothing to read the
   * status out of, so nothing is delivered. */
  std::size_t bytes = 0u;
  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  CHECK_EQ(rig.root().event_info(event, CL_EVENT_COMMAND_EXECUTION_STATUS, 0u, nullptr, &bytes),
           CL_SUCCESS);

  /* The right query with too little room: refused by the host rather than
   * read past. */
  cl_int status = 0;
  rig.host().cl_info(CL_INVALID_VALUE, nullptr, 0u);
  CHECK_EQ(rig.root().event_info(event, CL_EVENT_COMMAND_EXECUTION_STATUS, 1u, &status, nullptr),
           CL_INVALID_VALUE);

  /* The right query, answered 'still running': the application may still be
   * using that memory, so nothing is written into it. */
  const cl_int running = CL_RUNNING;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&running), sizeof running);
  CHECK_EQ(rig.root().event_info(event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status, &status,
                                 nullptr),
           CL_SUCCESS);
  for (const unsigned char byte : into) CHECK_EQ(byte, 0u);

  /* And completed, which is the one that delivers. */
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  const unsigned char answered[8] = {5u, 5u, 5u, 5u, 5u, 5u, 5u, 5u};
  rig.host().cl_bytes(CL_SUCCESS, answered, sizeof answered);
  CHECK_EQ(rig.root().event_info(event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status, &status,
                                 nullptr),
           CL_SUCCESS);
  for (const unsigned char byte : into) CHECK_EQ(byte, 5u);
  CHECK(rig.root().invariants_hold()); }

/* A delivery the host refuses is the answer the caller gets, and the row goes
 * back either way: a read that can never be delivered must not be kept
 * forever, and the application is told rather than left waiting. */
TEST(cl_enqueue_paths, a_delivery_the_host_refuses_is_answered_and_the_row_is_still_given_back)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  unsigned char into[8] = {};

  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  cl_event event = nullptr;
  CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_FALSE, 0u, sizeof into, into,
                                   &event),
           CL_SUCCESS);
  CHECK(event != nullptr);
  if (event == nullptr) return;

  /* The wait says it is done and the fetch behind it fails. */
  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_bytes(CL_OUT_OF_RESOURCES, nullptr, 0u);
  CHECK_EQ(rig.root().wait_for_events(1u, &event), CL_OUT_OF_RESOURCES);
  /* Asked again, there is nothing left to deliver: the row went back. */
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  CHECK_EQ(rig.root().wait_for_events(1u, &event), CL_SUCCESS);
  CHECK(rig.root().invariants_hold()); }

/* Waits name events, and events belong to contexts. A wait over two contexts
 * is a wait this ICD cannot make, because it has one transport to ask over
 * per context and no way to wait on both at once. */
TEST(cl_enqueue_paths, a_wait_over_two_contexts_or_too_many_events_is_refused)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;

  rig.host().cl_handle(0x6902u, CL_SUCCESS);
  cl_event mine = rig.loader().create_user_event(run.context(), nullptr);
  CHECK(mine != nullptr);
  if (mine == nullptr) return;

  /* A second context, and an event of its own. */
  rig.host().blob_import(0u, 0x79u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(0x6405u, CL_SUCCESS);
  cl_context second = rig.loader().create_context(nullptr, nullptr);
  if (second == nullptr) {
      /* The rig's loader makes a context on the device it found; if it
       * cannot, there is nothing to cross contexts with. */
      CHECK(rig.root().invariants_hold());
      return;
  }
  rig.host().cl_handle(0x6903u, CL_SUCCESS);
  cl_event theirs = rig.loader().create_user_event(second, nullptr);
  CHECK(theirs != nullptr);
  if (theirs == nullptr) return;

  const cl_event both[] = {mine, theirs};
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().wait_for_events(2u, both), CL_INVALID_CONTEXT);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

/* More non-blocking reads outstanding than the ICD can remember.
 *
 * A read it cannot record is one it could never deliver, so that read is made
 * blocking instead - slower, and never wrong. Dropping it would lose bytes an
 * application was told it would get. */
TEST(cl_enqueue_paths, a_read_the_icd_cannot_remember_is_made_blocking_instead)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  unsigned char into[8] = {};

  /* Fill the table. Each read is a marker the ICD keeps until it delivers. */
  for (std::uint32_t index = 0u; index < 64u; ++index) {
      rig.host().cl_enqueued(CL_SUCCESS, host_event + index);
      CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_FALSE, 0u, sizeof into, into,
                                       nullptr),
               CL_SUCCESS);
  }
  /* The next one has nowhere to be recorded. The marker it made is given
   * back and the read is sent blocking: enqueued, flushed, waited for. */
  const std::uint32_t sent = rig.kmt().commands();
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 64u);   /* the marker */
  rig.host().cl_result(CL_SUCCESS);                       /* it is released */
  const unsigned char answered[8] = {7u, 7u, 7u, 7u, 7u, 7u, 7u, 7u};
  rig.host().cl_bytes_for(CL_SUCCESS, answered, sizeof answered, host_event + 65u);
  rig.host().cl_result(CL_SUCCESS);                       /* the queue flushed */
  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_result(CL_SUCCESS);                       /* the read's event released */
  CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_FALSE, 0u, sizeof into, into,
                                   nullptr),
           CL_SUCCESS);
  /* The bytes are here, which is what makes a blocking read right. */
  for (const unsigned char byte : into) CHECK_EQ(byte, 7u);
  CHECK(rig.kmt().commands() > sent);
  CHECK_EQ(rig.log().holding("too many reads outstanding"), 1u);
  CHECK(rig.root().invariants_hold()); }

/* clFinish says everything queued has run, so it is where every read still
 * outstanding on that queue is delivered - and a delivery that fails is the
 * finish's answer rather than something to swallow. */
TEST(cl_enqueue_paths, a_finish_delivers_every_read_outstanding_on_its_queue)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  unsigned char first[8] = {};
  unsigned char second[8] = {};

  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_FALSE, 0u, sizeof first, first,
                                   nullptr),
           CL_SUCCESS);
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 1u);
  CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_FALSE, 0u, sizeof second, second,
                                   nullptr),
           CL_SUCCESS);

  /* The finish: its own marker, the flush, the wait, the marker released,
   * then the two deliveries with their markers released behind them. */
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 2u);
  rig.host().cl_result(CL_SUCCESS);
  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_result(CL_SUCCESS);
  const unsigned char ones[8] = {1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u};
  rig.host().cl_bytes(CL_SUCCESS, ones, sizeof ones);
  rig.host().cl_result(CL_SUCCESS);
  /* The second delivery is refused, which is what the finish answers. */
  rig.host().cl_bytes(CL_OUT_OF_RESOURCES, nullptr, 0u);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.loader().finish(run.queue()), CL_OUT_OF_RESOURCES);
  /* The one that worked still delivered, and both rows went back. */
  for (const unsigned char byte : first) CHECK_EQ(byte, 1u);
  CHECK(rig.root().invariants_hold()); }

/* A user event the host will not make, and a callback it will not register. */
TEST(cl_enqueue_paths, a_user_event_and_a_callback_the_host_refuses_are_answered)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;

  rig.host().cl_handle(0u, CL_OUT_OF_HOST_MEMORY);
  cl_int error = CL_SUCCESS;
  CHECK(rig.loader().create_user_event(run.context(), &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);
  /* And the contradiction: success with no event. */
  rig.host().cl_handle(0u, CL_SUCCESS);
  error = CL_SUCCESS;
  CHECK(rig.loader().create_user_event(run.context(), &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);

  /* A real one, and a callback the host refuses to register on it. */
  rig.host().cl_handle(0x6902u, CL_SUCCESS);
  cl_event user = rig.loader().create_user_event(run.context(), nullptr);
  CHECK(user != nullptr);
  if (user == nullptr) return;
  rig.host().cl_result(CL_INVALID_EVENT);
  CHECK_EQ(rig.root().watch_event(user, CL_COMPLETE, &nothing_happened, nullptr),
           CL_INVALID_EVENT);
  CHECK(rig.root().invariants_hold()); }

/* A finish whose wait fails answers that rather than going on to deliver:
 * the queue has not reached the marker, so nothing on it is finished and
 * delivering would hand the application bytes that are not there yet. */
TEST(cl_enqueue_paths, a_finish_whose_wait_fails_delivers_nothing)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  unsigned char into[8] = {};

  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_FALSE, 0u, sizeof into, into,
                                   nullptr),
           CL_SUCCESS);

  /* The marker is made, the flush works, and the host will not say how the
   * marker is doing. */
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 1u);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_info(CL_INVALID_EVENT, nullptr, 0u);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.loader().finish(run.queue()), CL_INVALID_EVENT);
  /* Nothing was delivered: the buffer is as the application left it. */
  for (const unsigned char byte : into) CHECK_EQ(byte, 0u);
  CHECK(rig.root().invariants_hold());

  /* And a finish whose marker the host will not even make. */
  rig.host().cl_enqueued(CL_OUT_OF_RESOURCES, 0u);
  CHECK_EQ(rig.loader().finish(run.queue()), CL_OUT_OF_RESOURCES);
  CHECK(rig.root().invariants_hold()); }

/* A local-memory argument of exactly the size of a cl_mem: the size is what
 * makes it look like one, and the missing value is what says it is not. */
TEST(cl_enqueue_paths, a_local_argument_the_size_of_a_buffer_is_still_local)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().set_kernel_argument(run.kernel(), 0u, sizeof(cl_mem), nullptr), CL_SUCCESS);
  CHECK(rig.root().invariants_hold()); }

/* A blocking read that was asked for an event: the application gets both the
 * bytes and something to release. */
TEST(cl_enqueue_paths, a_blocking_read_asked_for_an_event_hands_one_back_with_the_bytes)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  unsigned char into[8] = {};
  const unsigned char answered[8] = {3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u};

  rig.host().cl_bytes_for(CL_SUCCESS, answered, sizeof answered, host_event);
  rig.host().cl_result(CL_SUCCESS);                       /* the queue flushed */
  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  cl_event event = nullptr;
  CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_TRUE, 0u, sizeof into, into,
                                   &event),
           CL_SUCCESS);
  CHECK(event != nullptr);
  for (const unsigned char byte : into) CHECK_EQ(byte, 3u);
  CHECK(rig.root().invariants_hold()); }
