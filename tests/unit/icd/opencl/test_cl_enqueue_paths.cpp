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
