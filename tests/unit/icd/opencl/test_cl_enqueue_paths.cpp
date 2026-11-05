#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "harness/icd/fake_cl_loader.hpp"
#include "icd/opencl/cl_icd_root.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"

#include <CL/cl_ext.h>
#include <cstdint>
#include <cstring>

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

bool read_asks_the_host_to_block(const av::MockKmt &kmt, std::uint32_t index)
{
    const std::uint8_t *const frame = kmt.command_bytes(index);
    if (frame == nullptr || kmt.command_id(index) != av::clproto::kCmd_clEnqueueReadBuffer)
        return false;
    av::WireReader payload(frame + av::CommandHeader::encoded_size,
                           av::MockKmt::journal_bytes - av::CommandHeader::encoded_size);
    std::uint64_t  queue = 0u;
    std::uint64_t  buffer = 0u;
    std::uint64_t  blocking = 0u;
    if (!payload.read_pod(queue) || !payload.read_pod(buffer) || !payload.read_varint(blocking))
        return false;
    return blocking != 0u;
}

const cl_icd_dispatch *table_of(const void *object)
{
    return object != nullptr ? *reinterpret_cast<const cl_icd_dispatch *const *>(object) : nullptr;
}

class Running {
public:
    explicit Running(ClIcdRig &rig)
        : rig_(rig), device_(nullptr), context_(nullptr), queue_(nullptr), buffer_(nullptr),
          program_(nullptr), kernel_(nullptr)
    {
        if (rig_.loader().load() != FakeClLoader::Load::kept) return;
        rig_.host().arena_import(0x77u, 3u);
        rig_.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
        rig_.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
        rig_.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
        rig_.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
        rig_.host().cl_info(CL_SUCCESS, "", 1u);
        cl_uint devices = 0u;
        if (rig_.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device_, &devices) != CL_SUCCESS)
            return;
        rig_.host().arena_import(0x78u, 3u);
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
    void leaves_work()
    {
        rig_.host().cl_enqueued(CL_SUCCESS, host_event);
        cl_event marker = nullptr;
        static_cast<void>(table_of(queue_)->clEnqueueMarkerWithWaitList(queue_, 0u, nullptr, &marker));
    }
    cl_device_id     device() const { return device_; }
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

void nothing_happened(cl_event, cl_int, void *) {}

}

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

  rig.host().cl_handle(0u, CL_INVALID_KERNEL_NAME);
  error = CL_SUCCESS;
  CHECK(rig.root().create_kernel(run.program(), "absent", &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_KERNEL_NAME);
  rig.host().cl_handle(0u, CL_SUCCESS);
  error = CL_SUCCESS;
  CHECK(rig.root().create_kernel(run.program(), "k", &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_KERNEL_NAME);
  CHECK(rig.root().invariants_hold()); }

bool carries(const av::MockKmt &kmt, std::uint32_t index, std::uint64_t value)
{
    const std::uint8_t *const frame = kmt.command_bytes(index);
    if (frame == nullptr) return false;
    const std::uint32_t size = kmt.command_size(index) < av::MockKmt::journal_bytes ? kmt.command_size(index)
                                                                                     : av::MockKmt::journal_bytes;
    for (std::uint32_t at = 0u; at + sizeof value <= size; at++)
        if (std::memcmp(frame + at, &value, sizeof value) == 0) return true;
    return false;
}

TEST(cl_enqueue_paths, an_argument_the_size_of_a_buffer_crosses_as_the_host_s_number_for_it)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;

  const cl_mem        named = run.buffer();
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().set_kernel_argument(run.kernel(), 0u, sizeof named, &named), CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent);

  void *const invented = reinterpret_cast<void *>(std::uintptr_t{0x1234});
  CHECK_EQ(rig.root().set_kernel_argument(run.kernel(), 1u, sizeof invented, &invented),
           CL_SUCCESS);

  CHECK_EQ(rig.root().set_kernel_argument(run.kernel(), 2u, 64u, nullptr), CL_SUCCESS);

  const std::uint32_t scalar = 7u;
  CHECK_EQ(rig.root().set_kernel_argument(run.kernel(), 3u, sizeof scalar, &scalar), CL_SUCCESS);

  const std::size_t global = 64u;
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  cl_event event = nullptr;
  CHECK_EQ(rig.root().enqueue_kernel(run.queue(), run.kernel(), 1u, nullptr, &global, nullptr, &event),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 5u);
  for (std::uint32_t index = 0u; index < 4u; index++)
      CHECK_EQ(rig.kmt().command_id(sent + index), static_cast<std::uint32_t>(av::clproto::kCmd_clSetKernelArg));
  CHECK_EQ(rig.kmt().command_id(sent + 4u), static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueNDRangeKernel));
  CHECK(carries(rig.kmt(), sent, host_buffer));
  CHECK(!carries(rig.kmt(), sent, reinterpret_cast<std::uint64_t>(named)));
  CHECK(carries(rig.kmt(), sent + 1u, 0x1234u));
  CHECK(!carries(rig.kmt(), sent + 1u, host_buffer));

  CHECK_EQ(rig.root().set_kernel_argument(reinterpret_cast<cl_kernel>(run.queue()), 0u,
                                          sizeof named, &named),
           CL_INVALID_KERNEL);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_paths, a_kernel_enqueued_with_an_event_hands_one_back_and_without_hands_none)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const std::size_t global = 64u;

  CHECK_EQ(rig.root().enqueue_kernel(reinterpret_cast<cl_command_queue>(run.context()),
                                     run.kernel(), 1u, nullptr, &global, nullptr, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().enqueue_kernel(run.queue(), reinterpret_cast<cl_kernel>(run.context()), 1u,
                                     nullptr, &global, nullptr, nullptr),
           CL_INVALID_KERNEL);

  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  cl_event event = nullptr;
  CHECK_EQ(rig.root().enqueue_kernel(run.queue(), run.kernel(), 1u, nullptr, &global, nullptr,
                                     &event),
           CL_SUCCESS);
  CHECK(event != nullptr);

  CHECK_EQ(rig.root().enqueue_kernel(run.queue(), run.kernel(), 1u, nullptr, &global, nullptr,
                                     nullptr),
           CL_SUCCESS);

  rig.host().cl_enqueued(CL_OUT_OF_RESOURCES, 0u);
  cl_event refused = nullptr;
  CHECK_EQ(rig.root().enqueue_kernel(run.queue(), run.kernel(), 1u, nullptr, &global, nullptr,
                                     &refused),
           CL_OUT_OF_RESOURCES);
  CHECK(refused == nullptr);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_paths, a_transfer_of_nothing_or_to_nowhere_is_refused_before_it_is_sent)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  unsigned char bytes[8] = {};
  const std::uint32_t sent = rig.kmt().commands();

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

void drains(ClIcdRig &rig)
{
    rig.host().cl_enqueued(CL_SUCCESS, host_event);
    rig.host().cl_result(CL_SUCCESS);
    const cl_int complete = CL_COMPLETE;
    rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
    rig.host().cl_result(CL_SUCCESS);
}

bool write_asks_the_host_to_block(const av::MockKmt &kmt, std::uint32_t index)
{
    const std::uint8_t *const frame = kmt.command_bytes(index);
    if (frame == nullptr || kmt.command_id(index) != av::clproto::kCmd_clEnqueueWriteBuffer) return false;
    av::WireReader payload(frame + av::CommandHeader::encoded_size,
                           av::MockKmt::journal_bytes - av::CommandHeader::encoded_size);
    std::uint64_t  queue = 0u, buffer = 0u, blocking = 0u;
    if (!payload.read_pod(queue) || !payload.read_pod(buffer) || !payload.read_varint(blocking)) return false;
    return blocking != 0u;
}

TEST(cl_enqueue_paths, a_write_waits_for_its_queue_then_asks_the_host_to_block_whatever_it_was_asked)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  run.leaves_work();
  unsigned char bytes[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};

  drains(rig);
  std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().enqueue_write(run.queue(), run.buffer(), CL_FALSE, 0u, sizeof bytes, bytes,
                                    nullptr),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 4u);
  CHECK_EQ(rig.kmt().command_id(sent),
           static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueMarkerWithWaitList));

  sent = rig.kmt().commands();
  run.leaves_work();
  CHECK(write_asks_the_host_to_block(rig.kmt(), sent));
  drains(rig);
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 1u);
  cl_event event = nullptr;
  sent = rig.kmt().commands();
  CHECK_EQ(rig.root().enqueue_write(run.queue(), run.buffer(), CL_TRUE, 0u, sizeof bytes, bytes,
                                    &event),
           CL_SUCCESS);
  CHECK(event != nullptr);
  CHECK_EQ(rig.kmt().commands(), sent + 6u);
  CHECK_EQ(rig.kmt().command_id(sent),
           static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueMarkerWithWaitList));
  CHECK(write_asks_the_host_to_block(rig.kmt(), sent + 4u));
  CHECK_EQ(rig.kmt().command_id(sent + 5u),
           static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueMarkerWithWaitList));

  run.leaves_work();
  drains(rig);
  rig.host().cl_enqueued(CL_OUT_OF_RESOURCES, 0u);
  event = nullptr;
  CHECK_EQ(rig.root().enqueue_write(run.queue(), run.buffer(), CL_TRUE, 0u, sizeof bytes, bytes,
                                    &event),
           CL_OUT_OF_RESOURCES);
  CHECK(event == nullptr);

  run.leaves_work();
  rig.host().cl_enqueued(CL_OUT_OF_RESOURCES, 0u);
  sent = rig.kmt().commands();
  CHECK_EQ(rig.root().enqueue_write(run.queue(), run.buffer(), CL_TRUE, 0u, sizeof bytes, bytes,
                                    nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK_EQ(rig.kmt().command_id(sent),
           static_cast<std::uint32_t>(av::clproto::kCmd_clEnqueueMarkerWithWaitList));
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_paths, a_non_blocking_read_the_marker_fails_for_never_becomes_a_row)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  unsigned char into[8] = {};

  rig.host().cl_enqueued(CL_OUT_OF_RESOURCES, 0u);
  CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_FALSE, 0u, sizeof into, into,
                                   nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_paths, a_blocking_read_whose_drain_marker_the_host_refuses_answers_its_error)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  run.leaves_work();
  unsigned char into[8] = {};

  const std::uint32_t sent = rig.kmt().commands();
  rig.host().cl_enqueued(CL_OUT_OF_RESOURCES, 0u);
  CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_TRUE, 0u, sizeof into, into,
                                   nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueMarkerWithWaitList);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_paths, a_blocking_read_whose_drain_the_host_will_not_flush_answers_its_error)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  run.leaves_work();
  unsigned char into[8] = {};

  const std::uint32_t sent = rig.kmt().commands();
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  rig.host().cl_result(CL_OUT_OF_RESOURCES);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_TRUE, 0u, sizeof into, into,
                                   nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.kmt().commands(), sent + 3u);
  CHECK_EQ(rig.kmt().command_id(sent + 2u), av::clproto::kCmd_clReleaseEvent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_paths, a_blocking_read_the_host_refuses_answers_its_error)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  run.leaves_work();
  unsigned char into[8] = {};

  const std::uint32_t sent = rig.kmt().commands();
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  rig.host().cl_result(CL_SUCCESS);
  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_bytes(CL_OUT_OF_RESOURCES, nullptr, 0u);
  CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_TRUE, 0u, sizeof into, into,
                                   nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.kmt().commands(), sent + 5u);
  CHECK_EQ(rig.kmt().command_id(sent + 4u), av::clproto::kCmd_clEnqueueReadBuffer);
  for (const unsigned char byte : into) CHECK_EQ(byte, 0u);
  CHECK(rig.root().invariants_hold()); }

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

  cl_command_queue named = nullptr;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&named), sizeof named);
  CHECK_EQ(rig.root().event_info(event, CL_EVENT_COMMAND_QUEUE, sizeof named, &named, nullptr),
           CL_SUCCESS);

  std::size_t bytes = 0u;
  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  CHECK_EQ(rig.root().event_info(event, CL_EVENT_COMMAND_EXECUTION_STATUS, 0u, nullptr, &bytes),
           CL_SUCCESS);

  cl_int status = 0;
  rig.host().cl_info(CL_INVALID_VALUE, nullptr, 0u);
  CHECK_EQ(rig.root().event_info(event, CL_EVENT_COMMAND_EXECUTION_STATUS, 1u, &status, nullptr),
           CL_INVALID_VALUE);

  const cl_int running = CL_RUNNING;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&running), sizeof running);
  CHECK_EQ(rig.root().event_info(event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status, &status,
                                 nullptr),
           CL_SUCCESS);
  for (const unsigned char byte : into) CHECK_EQ(byte, 0u);

  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  const unsigned char answered[8] = {5u, 5u, 5u, 5u, 5u, 5u, 5u, 5u};
  rig.host().cl_bytes(CL_SUCCESS, answered, sizeof answered);
  CHECK_EQ(rig.root().event_info(event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status, &status,
                                 nullptr),
           CL_SUCCESS);
  for (const unsigned char byte : into) CHECK_EQ(byte, 5u);
  CHECK(rig.root().invariants_hold()); }

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

  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_bytes(CL_OUT_OF_RESOURCES, nullptr, 0u);
  CHECK_EQ(rig.root().wait_for_events(1u, &event), CL_OUT_OF_RESOURCES);
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  CHECK_EQ(rig.root().wait_for_events(1u, &event), CL_SUCCESS);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_paths, a_wait_over_two_contexts_or_too_many_events_is_refused)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;

  rig.host().cl_handle(0x6902u, CL_SUCCESS);
  cl_event mine = rig.loader().create_user_event(run.context(), nullptr);
  CHECK(mine != nullptr);
  if (mine == nullptr) return;

  rig.host().arena_import(0x79u, 3u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(0x6405u, CL_SUCCESS);
  cl_context second = rig.loader().create_context(nullptr, nullptr);
  if (second == nullptr) {
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

TEST(cl_enqueue_paths, a_read_the_icd_cannot_remember_is_made_blocking_instead)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  unsigned char into[8] = {};

  for (std::uint32_t index = 0u; index < 64u; ++index) {
      rig.host().cl_enqueued(CL_SUCCESS, host_event + index);
      CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_FALSE, 0u, sizeof into, into,
                                       nullptr),
               CL_SUCCESS);
  }
  const std::uint32_t sent = rig.kmt().commands();
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 64u);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 65u);
  rig.host().cl_result(CL_SUCCESS);
  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_result(CL_SUCCESS);
  const unsigned char answered[8] = {7u, 7u, 7u, 7u, 7u, 7u, 7u, 7u};
  rig.host().cl_bytes(CL_SUCCESS, answered, sizeof answered);
  CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_FALSE, 0u, sizeof into, into,
                                   nullptr),
           CL_SUCCESS);
  for (const unsigned char byte : into) CHECK_EQ(byte, 7u);
  CHECK(rig.kmt().commands() > sent);
  CHECK_EQ(rig.log().holding("too many reads outstanding"), 1u);
  CHECK(rig.root().invariants_hold()); }

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

  rig.host().cl_enqueued(CL_SUCCESS, host_event + 2u);
  rig.host().cl_result(CL_SUCCESS);
  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_result(CL_SUCCESS);
  const unsigned char ones[8] = {1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u};
  rig.host().cl_bytes(CL_SUCCESS, ones, sizeof ones);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_bytes(CL_OUT_OF_RESOURCES, nullptr, 0u);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.loader().finish(run.queue()), CL_OUT_OF_RESOURCES);
  for (const unsigned char byte : first) CHECK_EQ(byte, 1u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_paths, a_user_event_and_a_callback_the_host_refuses_are_answered)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;

  rig.host().cl_handle(0u, CL_OUT_OF_HOST_MEMORY);
  cl_int error = CL_SUCCESS;
  CHECK(rig.loader().create_user_event(run.context(), &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);
  rig.host().cl_handle(0u, CL_SUCCESS);
  error = CL_SUCCESS;
  CHECK(rig.loader().create_user_event(run.context(), &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);

  rig.host().cl_handle(0x6902u, CL_SUCCESS);
  cl_event user = rig.loader().create_user_event(run.context(), nullptr);
  CHECK(user != nullptr);
  if (user == nullptr) return;
  rig.host().cl_result(CL_INVALID_EVENT);
  CHECK_EQ(rig.root().watch_event(user, CL_COMPLETE, &nothing_happened, nullptr),
           CL_INVALID_EVENT);
  CHECK(rig.root().invariants_hold()); }

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

  rig.host().cl_enqueued(CL_SUCCESS, host_event + 1u);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_info(CL_INVALID_EVENT, nullptr, 0u);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.loader().finish(run.queue()), CL_INVALID_EVENT);
  for (const unsigned char byte : into) CHECK_EQ(byte, 0u);
  CHECK(rig.root().invariants_hold());

  rig.host().cl_enqueued(CL_OUT_OF_RESOURCES, 0u);
  CHECK_EQ(rig.loader().finish(run.queue()), CL_OUT_OF_RESOURCES);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_paths, a_local_argument_the_size_of_a_buffer_is_still_local)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().set_kernel_argument(run.kernel(), 0u, sizeof(cl_mem), nullptr), CL_SUCCESS);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_paths, a_blocking_read_is_sent_last_and_asks_the_host_to_block)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  run.leaves_work();
  unsigned char into[8] = {};
  const unsigned char answered[8] = {3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u};

  const std::uint32_t sent = rig.kmt().commands();
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  rig.host().cl_result(CL_SUCCESS);
  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_bytes(CL_SUCCESS, answered, sizeof answered);
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 1u);
  cl_event event = nullptr;
  CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_TRUE, 0u, sizeof into, into,
                                   &event),
           CL_SUCCESS);
  CHECK(event != nullptr);
  for (const unsigned char byte : into) CHECK_EQ(byte, 3u);

  CHECK_EQ(rig.kmt().commands(), sent + 6u);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueMarkerWithWaitList);
  CHECK_EQ(rig.kmt().command_id(sent + 4u), av::clproto::kCmd_clEnqueueReadBuffer);
  CHECK(read_asks_the_host_to_block(rig.kmt(), sent + 4u));
  CHECK_EQ(rig.kmt().command_id(sent + 5u), av::clproto::kCmd_clEnqueueMarkerWithWaitList);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_paths, a_finish_leaves_the_reads_of_every_other_queue_alone)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  rig.host().cl_handle(host_queue + 1u, CL_SUCCESS);
  cl_command_queue second = rig.root().create_queue(run.context(), run.device(), nullptr, nullptr);
  CHECK(second != nullptr);
  if (second == nullptr) return;
  unsigned char mine[8] = {};
  unsigned char theirs[8] = {};
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_FALSE, 0u, sizeof mine, mine,
                                   nullptr),
           CL_SUCCESS);
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 1u);
  CHECK_EQ(rig.root().enqueue_read(second, run.buffer(), CL_FALSE, 0u, sizeof theirs, theirs,
                                   nullptr),
           CL_SUCCESS);

  rig.host().cl_enqueued(CL_SUCCESS, host_event + 2u);
  rig.host().cl_result(CL_SUCCESS);
  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_result(CL_SUCCESS);
  const unsigned char ones[8] = {1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u};
  rig.host().cl_bytes(CL_SUCCESS, ones, sizeof ones);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.loader().finish(run.queue()), CL_SUCCESS);
  for (const unsigned char byte : mine) CHECK_EQ(byte, 1u);
  for (const unsigned char byte : theirs) CHECK_EQ(byte, 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_paths, the_first_delivery_that_fails_is_the_error_a_wait_answers)
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

  rig.host().cl_enqueued(CL_SUCCESS, host_event + 2u);
  rig.host().cl_result(CL_SUCCESS);
  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_bytes(CL_MEM_OBJECT_ALLOCATION_FAILURE, nullptr, 0u);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_bytes(CL_OUT_OF_RESOURCES, nullptr, 0u);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.loader().finish(run.queue()), CL_MEM_OBJECT_ALLOCATION_FAILURE);
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 3u);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.loader().finish(run.queue()), CL_SUCCESS);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_paths, a_wait_over_two_failed_deliveries_answers_the_first)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  unsigned char first[8] = {};
  unsigned char second[8] = {};

  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  cl_event one = nullptr;
  CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_FALSE, 0u, sizeof first, first,
                                   &one),
           CL_SUCCESS);
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 1u);
  cl_event two = nullptr;
  CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_FALSE, 0u, sizeof second, second,
                                   &two),
           CL_SUCCESS);
  CHECK(one != nullptr && two != nullptr);
  if (one == nullptr || two == nullptr) return;

  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_bytes(CL_MEM_OBJECT_ALLOCATION_FAILURE, nullptr, 0u);
  rig.host().cl_bytes(CL_OUT_OF_RESOURCES, nullptr, 0u);
  const cl_event both[] = {one, two};
  CHECK_EQ(rig.root().wait_for_events(2u, both), CL_MEM_OBJECT_ALLOCATION_FAILURE);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_paths, a_finish_delivers_every_row_the_icd_can_hold)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  unsigned char into[64][8] = {};

  for (std::uint32_t index = 0u; index < 64u; ++index) {
      rig.host().cl_enqueued(CL_SUCCESS, host_event + index);
      CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_FALSE, 0u, sizeof into[index],
                                       into[index], nullptr),
               CL_SUCCESS);
  }

  rig.host().cl_enqueued(CL_SUCCESS, host_event + 100u);
  rig.host().cl_result(CL_SUCCESS);
  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_result(CL_SUCCESS);
  const unsigned char ones[8] = {1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u};
  for (std::uint32_t index = 0u; index < 64u; ++index) {
      rig.host().cl_bytes(CL_SUCCESS, ones, sizeof ones);
      rig.host().cl_result(CL_SUCCESS);
  }
  CHECK_EQ(rig.loader().finish(run.queue()), CL_SUCCESS);
  for (const auto &row : into)
      for (const unsigned char byte : row) CHECK_EQ(byte, 1u);
  CHECK(rig.root().invariants_hold()); }
