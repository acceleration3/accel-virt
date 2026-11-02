#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "harness/icd/fake_cl_loader.hpp"
#include "icd/opencl/cl_icd_root.hpp"
#include "icd/opencl/cl_wait_list.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

using av::ClIcdRig;
using av::FakeClLoader;
using av::clicd::ClIcdRoot;
using av::clicd::ClWaitList;

namespace {

constexpr std::uint64_t host_platform = 0x6200u;
constexpr std::uint64_t host_device = 0x6300u;
constexpr std::uint64_t host_context = 0x6400u;
constexpr std::uint64_t host_queue = 0x6500u;
constexpr std::uint64_t host_buffer = 0x6600u;
constexpr std::uint64_t host_program = 0x6700u;
constexpr std::uint64_t host_kernel = 0x6800u;
constexpr std::uint64_t host_event = 0x6900u;
constexpr std::uint64_t host_gate = 0x6950u;
constexpr std::uint64_t host_image = 0x6A00u;
constexpr std::uint64_t host_sub = 0x6B00u;
constexpr std::uint64_t host_pipe = 0x6C00u;
constexpr std::uint64_t host_shared = 0x0000'7F55'0000'1000ull;
constexpr std::size_t   shared_bytes = 256u;

const cl_icd_dispatch *table_of(const void *object)
{
    return object != nullptr ? *reinterpret_cast<const cl_icd_dispatch *const *>(object) : nullptr;
}

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

class Running {
public:
    explicit Running(ClIcdRig &rig) : rig_(rig)
    {
        if (rig_.loader().load() != FakeClLoader::Load::kept) return;
        rig_.host().blob_import(0u, 0x77u);
        rig_.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
        rig_.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
        rig_.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
        rig_.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
        rig_.host().cl_info(CL_SUCCESS, "", 1u);
        cl_uint devices = 0u;
        if (rig_.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device_, &devices) != CL_SUCCESS) return;
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
        rig_.host().cl_handle(host_gate, CL_SUCCESS);
        gate_ = rig_.loader().create_user_event(context_, nullptr);
    }

    bool             made() const { return kernel_ != nullptr && buffer_ != nullptr && gate_ != nullptr; }
    cl_device_id     device() const { return device_; }
    cl_context       context() const { return context_; }
    cl_command_queue queue() const { return queue_; }
    cl_mem           buffer() const { return buffer_; }
    cl_kernel        kernel() const { return kernel_; }
    const cl_event  *gate() const { return &gate_; }

    cl_mem image()
    {
        const cl_image_format format = {CL_RGBA, CL_UNORM_INT8};
        cl_image_desc         desc = {};
        desc.image_type = CL_MEM_OBJECT_IMAGE2D;
        desc.image_width = 16u;
        desc.image_height = 8u;
        rig_.host().cl_handle(host_image, CL_SUCCESS);
        return rig_.root().create_image(context_, CL_MEM_READ_WRITE, &format, &desc, nullptr, nullptr);
    }

    void leaves_work()
    {
        rig_.host().cl_enqueued(CL_SUCCESS, host_event);
        cl_event marker = nullptr;
        static_cast<void>(table_of(queue_)->clEnqueueMarkerWithWaitList(queue_, 0u, nullptr, &marker));
    }

    void drains()
    {
        rig_.host().cl_enqueued(CL_SUCCESS, host_event);
        rig_.host().cl_result(CL_SUCCESS);
        const cl_int complete = CL_COMPLETE;
        rig_.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
        rig_.host().cl_result(CL_SUCCESS);
    }

    template <typename Word>
    void answers(Word value)
    {
        rig_.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&value), sizeof value);
    }

    cl_event foreign_event()
    {
        rig_.host().blob_import(0u, 0x79u);
        rig_.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
        rig_.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
        rig_.host().cl_handle(host_context + 1u, CL_SUCCESS);
        const cl_context other = rig_.loader().create_context(device_, nullptr);
        if (other == nullptr) return nullptr;
        rig_.host().cl_handle(host_event + 9u, CL_SUCCESS);
        return rig_.root().create_user_event(other, nullptr);
    }

private:
    ClIcdRig        &rig_;
    cl_device_id     device_ = nullptr;
    cl_context       context_ = nullptr;
    cl_command_queue queue_ = nullptr;
    cl_mem           buffer_ = nullptr;
    cl_program       program_ = nullptr;
    cl_kernel        kernel_ = nullptr;
    cl_event         gate_ = nullptr;
};

struct Freed {
    cl_command_queue queue;
    cl_uint          count;
    void            *first;
    void            *user_data;
};

void CL_CALLBACK frees(cl_command_queue queue, cl_uint count, void *pointers[], void *user_data)
{
    Freed *const freed = static_cast<Freed *>(user_data);
    freed->queue = queue;
    freed->count = count;
    freed->first = pointers[0];
    freed->user_data = user_data;
}

}

TEST(cl_wait_list, a_wait_list_is_the_count_and_the_events_it_was_given)
{ const ClWaitList none;
  CHECK_EQ(none.count(), 0u);
  CHECK(none.events() == nullptr);
  cl_event events[2] = {reinterpret_cast<cl_event>(std::uintptr_t{0x10}), reinterpret_cast<cl_event>(std::uintptr_t{0x20})};
  const ClWaitList two(2u, events);
  CHECK_EQ(two.count(), 2u);
  CHECK(two.events() == events);
  CHECK(!none.blocks());
  CHECK(!two.blocks());
  CHECK(ClWaitList(2u, events, CL_TRUE).blocks());
  CHECK(!ClWaitList(2u, events, CL_FALSE).blocks()); }

TEST(cl_enqueue_api, a_kernel_and_a_device_copy_carry_their_wait_list_to_the_host)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const std::size_t global = 64u;

  std::uint32_t sent = rig.kmt().commands();
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 1u);
  cl_event ran = nullptr;
  CHECK_EQ(table_of(run.queue())->clEnqueueNDRangeKernel(run.queue(), run.kernel(), 1u, nullptr, &global, nullptr,
                                                         1u, run.gate(), &ran),
           CL_SUCCESS);
  CHECK(ran != nullptr);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueNDRangeKernel);
  CHECK(carries(rig.kmt(), sent, host_gate));

  sent = rig.kmt().commands();
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 2u);
  cl_event copied = nullptr;
  CHECK_EQ(table_of(run.queue())->clEnqueueCopyBuffer(run.queue(), run.buffer(), run.buffer(), 0u, 128u, 64u, 1u,
                                                      &ran, &copied),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueCopyBuffer);
  CHECK(carries(rig.kmt(), sent, host_event + 1u));

  const cl_event both[2] = {ran, copied};
  sent = rig.kmt().commands();
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 3u);
  const std::uint32_t pattern = 0xA5A5A5A5u;
  cl_event            filled = nullptr;
  CHECK_EQ(table_of(run.queue())->clEnqueueFillBuffer(run.queue(), run.buffer(), &pattern, sizeof pattern, 0u, 64u,
                                                      2u, both, &filled),
           CL_SUCCESS);
  CHECK(carries(rig.kmt(), sent, host_event + 1u));
  CHECK(carries(rig.kmt(), sent, host_event + 2u));
  const size_t origin[3] = {0u, 0u, 0u}, region[3] = {16u, 2u, 1u};
  sent = rig.kmt().commands();
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 4u);
  cl_event rect = nullptr;
  CHECK_EQ(table_of(run.queue())->clEnqueueCopyBufferRect(run.queue(), run.buffer(), run.buffer(), origin, origin,
                                                          region, 0u, 0u, 0u, 0u, 1u, &filled, &rect),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueCopyBufferRect);
  CHECK(carries(rig.kmt(), sent, host_event + 3u));
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_api, a_wait_list_the_api_refuses_is_refused_by_every_enqueue_and_sends_nothing)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_event foreign = run.foreign_event();
  CHECK(foreign != nullptr);
  const std::size_t global = 64u;
  unsigned char     bytes[16] = {};
  const size_t      origin[3] = {0u, 0u, 0u}, region[3] = {4u, 1u, 1u};
  const ClWaitList  counted_but_none(1u, nullptr);
  const ClWaitList  listed_but_not_counted(0u, run.gate());
  const ClWaitList  of_another_context(1u, &foreign);
  const std::uint32_t sent = rig.kmt().commands();
  for (const ClWaitList *list : {&counted_but_none, &listed_but_not_counted, &of_another_context}) {
      CHECK_EQ(rig.root().enqueue_kernel(run.queue(), run.kernel(), 1u, nullptr, &global, nullptr, nullptr, *list),
               CL_INVALID_EVENT_WAIT_LIST);
      CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_FALSE, 0u, sizeof bytes, bytes, nullptr, *list),
               CL_INVALID_EVENT_WAIT_LIST);
      CHECK_EQ(rig.root().enqueue_read(run.queue(), run.buffer(), CL_TRUE, 0u, sizeof bytes, bytes, nullptr, *list),
               CL_INVALID_EVENT_WAIT_LIST);
      CHECK_EQ(rig.root().enqueue_write(run.queue(), run.buffer(), CL_TRUE, 0u, sizeof bytes, bytes, nullptr, *list),
               CL_INVALID_EVENT_WAIT_LIST);
      CHECK_EQ(rig.root().enqueue_copy_buffer(run.queue(), run.buffer(), run.buffer(), 0u, 64u, 16u, nullptr, *list),
               CL_INVALID_EVENT_WAIT_LIST);
      CHECK_EQ(rig.root().enqueue_fill_buffer(run.queue(), run.buffer(), bytes, 4u, 0u, 16u, nullptr, *list),
               CL_INVALID_EVENT_WAIT_LIST);
      CHECK_EQ(rig.root().enqueue_copy_buffer_rect(run.queue(), run.buffer(), run.buffer(), origin, origin, region,
                                                   0u, 0u, 0u, 0u, nullptr, *list),
               CL_INVALID_EVENT_WAIT_LIST);
      cl_int error = CL_SUCCESS;
      CHECK(rig.root().map_buffer(run.queue(), run.buffer(), CL_TRUE, CL_MAP_READ, 0u, 16u, nullptr, &error,
                                  *list) == nullptr);
      CHECK_EQ(error, CL_INVALID_EVENT_WAIT_LIST);
      CHECK_EQ(rig.root().enqueue_task(run.queue(), run.kernel(), nullptr, *list), CL_INVALID_EVENT_WAIT_LIST);
      const cl_mem moved = run.buffer();
      CHECK_EQ(rig.root().enqueue_migrate(run.queue(), 1u, &moved, 0u, nullptr, *list), CL_INVALID_EVENT_WAIT_LIST);
  }
  cl_event many[ClIcdRoot::max_waited + 1u] = {};
  for (cl_event &one : many) one = *run.gate();
  CHECK_EQ(rig.root().enqueue_kernel(run.queue(), run.kernel(), 1u, nullptr, &global, nullptr, nullptr,
                                     ClWaitList(ClIcdRoot::max_waited + 1u, many)),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_api, a_non_blocking_read_is_reached_behind_its_wait_list_and_returns_at_once)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  unsigned char into[16] = {};
  const std::uint32_t sent = rig.kmt().commands();
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 1u);
  cl_event read = nullptr;
  CHECK_EQ(table_of(run.queue())->clEnqueueReadBuffer(run.queue(), run.buffer(), CL_FALSE, 0u, sizeof into, into, 1u,
                                                      run.gate(), &read),
           CL_SUCCESS);
  CHECK(read != nullptr);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueMarkerWithWaitList);
  CHECK(carries(rig.kmt(), sent, host_gate));
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_api, a_transfer_the_host_blocks_on_waits_for_its_list_here_before_it_is_sent)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  unsigned char bytes[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};

  run.answers(static_cast<cl_int>(CL_COMPLETE));
  run.drains();
  std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(table_of(run.queue())->clEnqueueWriteBuffer(run.queue(), run.buffer(), CL_FALSE, 0u, sizeof bytes, bytes,
                                                       1u, run.gate(), nullptr),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 5u);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clGetEventInfo);
  CHECK_EQ(rig.kmt().command_id(sent + 1u), av::clproto::kCmd_clEnqueueMarkerWithWaitList);
  CHECK(carries(rig.kmt(), sent + 1u, host_gate));

  rig.host().cl_enqueued(CL_OUT_OF_RESOURCES, 0u);
  sent = rig.kmt().commands();
  unsigned char into[8] = {};
  CHECK_EQ(table_of(run.queue())->clEnqueueReadBuffer(run.queue(), run.buffer(), CL_TRUE, 0u, sizeof into, into, 1u,
                                                      run.gate(), nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.kmt().commands(), sent + 2u);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueWriteBuffer);
  CHECK(carries(rig.kmt(), sent + 1u, host_gate));

  run.drains();
  rig.host().cl_mapped(0x1000u, 0u, CL_SUCCESS);
  rig.host().cl_bytes(CL_SUCCESS, bytes, sizeof bytes);
  sent = rig.kmt().commands();
  cl_int      error = CL_INVALID_VALUE;
  void *const mapped = table_of(run.queue())->clEnqueueMapBuffer(run.queue(), run.buffer(), CL_TRUE, CL_MAP_READ, 0u,
                                                                 sizeof bytes, 1u, run.gate(), nullptr, &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(mapped != nullptr);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueMarkerWithWaitList);
  CHECK(carries(rig.kmt(), sent, host_gate));
  CHECK_EQ(rig.kmt().command_id(sent + 4u), av::clproto::kCmd_clEnqueueMapBuffer);
  run.answers(static_cast<cl_int>(CL_COMPLETE));
  run.drains();
  rig.host().cl_result(CL_SUCCESS);
  sent = rig.kmt().commands();
  CHECK_EQ(table_of(run.queue())->clEnqueueUnmapMemObject(run.queue(), run.buffer(), mapped, 1u, run.gate(), nullptr),
           CL_SUCCESS);
  CHECK(carries(rig.kmt(), sent + 1u, host_gate));
  CHECK_EQ(rig.kmt().command_id(sent + 5u), av::clproto::kCmd_clEnqueueUnmapMemObject);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_api, a_non_blocking_transfer_does_not_wait_for_a_user_event_not_yet_set)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  unsigned char bytes[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
  run.answers(static_cast<cl_int>(CL_SUBMITTED));
  std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(table_of(run.queue())->clEnqueueWriteBuffer(run.queue(), run.buffer(), CL_FALSE, 0u, sizeof bytes, bytes,
                                                       1u, run.gate(), nullptr),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clGetEventInfo);
  for (std::uint32_t index = sent + 1u; index < rig.kmt().commands(); ++index) {
      CHECK(rig.kmt().command_id(index) != av::clproto::kCmd_clEnqueueMarkerWithWaitList ||
            !carries(rig.kmt(), index, host_gate));
  }
  bool told = false;
  for (std::uint32_t line = 0u; line < rig.log().lines(); ++line)
      told = told || std::strstr(rig.log().line(line), "a user event not yet set") != nullptr;
  CHECK(told);
  run.drains();
  sent = rig.kmt().commands();
  CHECK_EQ(table_of(run.queue())->clEnqueueWriteBuffer(run.queue(), run.buffer(), CL_TRUE, 0u, sizeof bytes, bytes,
                                                       1u, run.gate(), nullptr),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueWriteBuffer);
  CHECK_EQ(rig.kmt().command_id(sent + 1u), av::clproto::kCmd_clEnqueueMarkerWithWaitList);
  CHECK(carries(rig.kmt(), sent + 1u, host_gate));
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_api, a_task_is_an_ndrange_of_one_and_the_1_1_points_are_markers_and_barriers)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  std::uint32_t sent = rig.kmt().commands();
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 1u);
  cl_event task = nullptr;
  CHECK_EQ(table_of(run.queue())->clEnqueueTask(run.queue(), run.kernel(), 1u, run.gate(), &task), CL_SUCCESS);
  CHECK(task != nullptr);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueNDRangeKernel);
  CHECK(carries(rig.kmt(), sent, host_gate));
  CHECK_EQ(rig.root().enqueue_task(run.queue(), reinterpret_cast<cl_kernel>(run.queue()), nullptr),
           CL_INVALID_KERNEL);

  sent = rig.kmt().commands();
  CHECK_EQ(table_of(run.queue())->clEnqueueMarker(run.queue(), nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_event_marker(reinterpret_cast<cl_command_queue>(run.kernel()), &task),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.kmt().commands(), sent);
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 2u);
  cl_event marker = nullptr;
  CHECK_EQ(rig.root().enqueue_event_marker(run.queue(), &marker), CL_SUCCESS);
  CHECK(marker != nullptr);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueMarkerWithWaitList);

  sent = rig.kmt().commands();
  CHECK_EQ(table_of(run.queue())->clEnqueueWaitForEvents(run.queue(), 0u, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_wait_for_events(reinterpret_cast<cl_command_queue>(run.kernel()), 1u, run.gate()),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.kmt().commands(), sent);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().enqueue_wait_for_events(run.queue(), 1u, run.gate()), CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueBarrierWithWaitList);
  CHECK(carries(rig.kmt(), sent, host_gate));

  sent = rig.kmt().commands();
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(table_of(run.queue())->clEnqueueBarrier(run.queue()), CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueBarrierWithWaitList);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_api, image_buffer_copies_an_image_fill_and_a_migration_cross_as_the_hosts_numbers)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_mem image = run.image();
  CHECK(image != nullptr);
  const size_t origin[3] = {0u, 0u, 0u}, region[3] = {4u, 2u, 1u};

  std::uint32_t sent = rig.kmt().commands();
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 1u);
  cl_event out = nullptr;
  CHECK_EQ(table_of(run.queue())->clEnqueueCopyImageToBuffer(run.queue(), image, run.buffer(), origin, region, 32u,
                                                             1u, run.gate(), &out),
           CL_SUCCESS);
  CHECK(out != nullptr);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueCopyImageToBuffer);
  CHECK(carries(rig.kmt(), sent, host_image));
  CHECK(carries(rig.kmt(), sent, host_buffer));
  CHECK(carries(rig.kmt(), sent, host_gate));

  sent = rig.kmt().commands();
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 2u);
  cl_event in = nullptr;
  CHECK_EQ(table_of(run.queue())->clEnqueueCopyBufferToImage(run.queue(), run.buffer(), image, 32u, origin, region,
                                                             1u, &out, &in),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueCopyBufferToImage);
  CHECK(carries(rig.kmt(), sent, host_event + 1u));

  const std::uint32_t colour[4] = {0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u};
  sent = rig.kmt().commands();
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 3u);
  cl_event filled = nullptr;
  CHECK_EQ(table_of(run.queue())->clEnqueueFillImage(run.queue(), image, colour, origin, region, 0u, nullptr, &filled),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueFillImage);
  const std::uint64_t last_half = (std::uint64_t{0x44444444u} << 32u) | 0x33333333u;
  CHECK(carries(rig.kmt(), sent, last_half));

  sent = rig.kmt().commands();
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 4u);
  const cl_mem moved[2] = {run.buffer(), image};
  cl_event     migrated = nullptr;
  CHECK_EQ(table_of(run.queue())->clEnqueueMigrateMemObjects(run.queue(), 2u, moved, CL_MIGRATE_MEM_OBJECT_HOST, 1u,
                                                             &filled, &migrated),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueMigrateMemObjects);
  CHECK(carries(rig.kmt(), sent, host_buffer));
  CHECK(carries(rig.kmt(), sent, host_image));
  CHECK(carries(rig.kmt(), sent, host_event + 3u));

  sent = rig.kmt().commands();
  const cl_mem not_memory = reinterpret_cast<cl_mem>(run.kernel());
  const cl_command_queue not_a_queue = reinterpret_cast<cl_command_queue>(run.kernel());
  CHECK_EQ(rig.root().enqueue_copy_image_to_buffer(not_a_queue, image, run.buffer(), origin, region, 0u, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().enqueue_copy_image_to_buffer(run.queue(), not_memory, run.buffer(), origin, region, 0u, nullptr),
           CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().enqueue_copy_image_to_buffer(run.queue(), image, run.buffer(), nullptr, region, 0u, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_copy_buffer_to_image(not_a_queue, run.buffer(), image, 0u, origin, region, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().enqueue_copy_buffer_to_image(run.queue(), run.buffer(), not_memory, 0u, origin, region, nullptr),
           CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().enqueue_copy_buffer_to_image(run.queue(), run.buffer(), image, 0u, origin, nullptr, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_fill_image(not_a_queue, image, colour, origin, region, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().enqueue_fill_image(run.queue(), not_memory, colour, origin, region, nullptr),
           CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().enqueue_fill_image(run.queue(), image, nullptr, origin, region, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_migrate(not_a_queue, 2u, moved, 0u, nullptr), CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().enqueue_migrate(run.queue(), 0u, moved, 0u, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_migrate(run.queue(), 1u, &not_memory, 0u, nullptr), CL_INVALID_MEM_OBJECT);
  cl_mem too_many[ClIcdRoot::max_waited + 1u] = {};
  for (cl_mem &one : too_many) one = run.buffer();
  CHECK_EQ(rig.root().enqueue_migrate(run.queue(), ClIcdRoot::max_waited + 1u, too_many, 0u, nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_api, a_sub_buffer_holds_its_buffer_and_names_it_as_its_associated_object)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_buffer_region region = {64u, 64u};
  cl_int                 error = CL_INVALID_VALUE;
  rig.host().cl_handle(host_sub, CL_SUCCESS);
  const cl_mem sub = table_of(run.buffer())->clCreateSubBuffer(run.buffer(), CL_MEM_READ_WRITE,
                                                                 CL_BUFFER_CREATE_TYPE_REGION, &region, &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(sub != nullptr);
  if (sub == nullptr) return;
  cl_mem associated = nullptr;
  CHECK_EQ(rig.root().mem_object_info(sub, CL_MEM_ASSOCIATED_MEMOBJECT, sizeof associated, &associated, nullptr),
           CL_SUCCESS);
  CHECK(associated == run.buffer());
  CHECK_EQ(rig.root().mem_object_info(run.buffer(), CL_MEM_ASSOCIATED_MEMOBJECT, sizeof associated, &associated,
                                      nullptr),
           CL_SUCCESS);
  CHECK(associated == nullptr);
  cl_uint count = 0u;
  CHECK_EQ(rig.root().mem_object_info(run.buffer(), CL_MEM_REFERENCE_COUNT, sizeof count, &count, nullptr),
           CL_SUCCESS);
  CHECK_EQ(count, 2u);

  const std::uint32_t sent = rig.kmt().commands();
  CHECK(rig.root().create_sub_buffer(sub, CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION, &region, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_MEM_OBJECT);
  CHECK(rig.root().create_sub_buffer(reinterpret_cast<cl_mem>(run.kernel()), CL_MEM_READ_WRITE,
                                     CL_BUFFER_CREATE_TYPE_REGION, &region, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_MEM_OBJECT);
  CHECK(rig.root().create_sub_buffer(run.buffer(), CL_MEM_READ_WRITE, 0x1235u, &region, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK(rig.root().create_sub_buffer(run.buffer(), CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION, nullptr,
                                     &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent);
  rig.host().cl_handle(0u, CL_MISALIGNED_SUB_BUFFER_OFFSET);
  CHECK(rig.root().create_sub_buffer(run.buffer(), CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION, &region,
                                     &error) == nullptr);
  CHECK_EQ(error, CL_MISALIGNED_SUB_BUFFER_OFFSET);

  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(table_of(sub)->clReleaseMemObject(sub), CL_SUCCESS);
  CHECK_EQ(rig.root().mem_object_info(run.buffer(), CL_MEM_REFERENCE_COUNT, sizeof count, &count, nullptr),
           CL_SUCCESS);
  CHECK_EQ(count, 1u);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_api, an_image_map_is_staged_packed_and_written_back_at_its_unmap)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_mem image = run.image();
  CHECK(image != nullptr);
  const size_t        origin[3] = {2u, 1u, 0u}, region[3] = {4u, 2u, 1u};
  const unsigned char answered[32] = {1u,  2u,  3u,  4u,  5u,  6u,  7u,  8u,  9u,  10u, 11u,
                                      12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u, 20u, 21u, 22u,
                                      23u, 24u, 25u, 26u, 27u, 28u, 29u, 30u, 31u, 32u};
  run.answers(static_cast<cl_mem_object_type>(CL_MEM_OBJECT_IMAGE2D));
  run.answers(std::size_t{4u});
  rig.host().cl_bytes(CL_SUCCESS, answered, sizeof answered);
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 1u);
  std::size_t row_pitch = 0u, slice_pitch = 99u;
  cl_int      error = CL_INVALID_VALUE;
  cl_event    mapped_event = nullptr;
  std::uint32_t sent = rig.kmt().commands();
  unsigned char *const mapped = static_cast<unsigned char *>(table_of(run.queue())->clEnqueueMapImage(
      run.queue(), image, CL_FALSE, CL_MAP_READ | CL_MAP_WRITE, origin, region, &row_pitch, &slice_pitch, 0u, nullptr,
      &mapped_event, &error));
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(mapped != nullptr);
  if (mapped == nullptr) return;
  CHECK(mapped_event != nullptr);
  CHECK_EQ(row_pitch, 16u);
  CHECK_EQ(slice_pitch, 0u);
  for (std::size_t index = 0u; index < sizeof answered; ++index) CHECK_EQ(mapped[index], answered[index]);
  CHECK_EQ(rig.kmt().command_id(sent + 2u), av::clproto::kCmd_clEnqueueReadImage);

  mapped[0] = 0xEEu;
  sent = rig.kmt().commands();
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 2u);
  cl_event unmapped = nullptr;
  CHECK_EQ(table_of(run.queue())->clEnqueueUnmapMemObject(run.queue(), image, mapped, 0u, nullptr, &unmapped),
           CL_SUCCESS);
  CHECK(unmapped != nullptr);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueWriteImage);
  CHECK_EQ(rig.kmt().command_id(sent + 1u), av::clproto::kCmd_clEnqueueMarkerWithWaitList);
  CHECK_EQ(rig.root().unmap(run.queue(), image, mapped, nullptr), CL_INVALID_VALUE);

  run.answers(std::size_t{4u});
  sent = rig.kmt().commands();
  void *const fresh = rig.root().map_image(run.queue(), image, CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, origin,
                                           region, &row_pitch, nullptr, nullptr, &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(fresh != nullptr);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK_EQ(static_cast<unsigned char *>(fresh)[0], 0u);

  run.answers(static_cast<cl_mem_object_type>(CL_MEM_OBJECT_BUFFER));
  CHECK(rig.root().map_image(run.queue(), run.buffer(), CL_TRUE, CL_MAP_READ, origin, region, &row_pitch, nullptr,
                             nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_MEM_OBJECT);
  CHECK(rig.root().map_image(run.queue(), image, CL_TRUE, CL_MAP_READ, origin, region, nullptr, nullptr, nullptr,
                             &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK(rig.root().map_image(reinterpret_cast<cl_command_queue>(run.kernel()), image, CL_TRUE, CL_MAP_READ, origin,
                             region, &row_pitch, nullptr, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_COMMAND_QUEUE);
  CHECK(rig.root().map_image(run.queue(), reinterpret_cast<cl_mem>(run.kernel()), CL_TRUE, CL_MAP_READ, origin,
                             region, &row_pitch, nullptr, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_api, an_image_with_properties_takes_none_and_a_pipe_is_the_hosts)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  const cl_image_format format = {CL_RGBA, CL_UNORM_INT8};
  cl_image_desc         desc = {};
  desc.image_type = CL_MEM_OBJECT_IMAGE2D;
  desc.image_width = 4u;
  desc.image_height = 4u;
  const cl_mem_properties some[] = {0x4242u, 1u, 0u};
  const cl_mem_properties none[] = {0u};
  cl_int                  error = CL_SUCCESS;
  std::uint32_t           sent = rig.kmt().commands();
  CHECK(table_of(run.context())->clCreateImageWithProperties(run.context(), some, CL_MEM_READ_WRITE, &format, &desc,
                                                             nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PROPERTY);
  CHECK_EQ(rig.kmt().commands(), sent);
  rig.host().cl_handle(host_image, CL_SUCCESS);
  const cl_mem image = rig.root().create_image_with_properties(run.context(), none, CL_MEM_READ_WRITE, &format, &desc,
                                                               nullptr, &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(image != nullptr);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clCreateImage);

  const cl_pipe_properties pipe_some[] = {1, 0};
  sent = rig.kmt().commands();
  CHECK(table_of(run.context())->clCreatePipe(run.context(), CL_MEM_READ_WRITE, 16u, 8u, pipe_some, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK(rig.root().create_pipe(reinterpret_cast<cl_context>(run.queue()), CL_MEM_READ_WRITE, 16u, 8u, nullptr,
                               &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  CHECK_EQ(rig.kmt().commands(), sent);
  rig.host().cl_handle(0u, CL_INVALID_OPERATION);
  CHECK(rig.root().create_pipe(run.context(), CL_MEM_READ_WRITE, 16u, 8u, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_OPERATION);
  rig.host().cl_handle(host_pipe, CL_SUCCESS);
  const cl_mem pipe = rig.root().create_pipe(run.context(), CL_MEM_READ_WRITE, 16u, 8u, nullptr, &error);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(pipe != nullptr);
  run.answers(cl_uint{16u});
  cl_uint packet = 0u;
  CHECK_EQ(table_of(pipe)->clGetPipeInfo(pipe, CL_PIPE_PACKET_SIZE, sizeof packet, &packet, nullptr), CL_SUCCESS);
  CHECK_EQ(packet, 16u);
  CHECK_EQ(rig.root().pipe_info(reinterpret_cast<cl_mem>(run.queue()), CL_PIPE_PACKET_SIZE, sizeof packet, &packet,
                                nullptr),
           CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_enqueue_api, shared_memory_is_copied_and_filled_at_the_hosts_addresses_and_freed_behind_its_queue)
{ ClIcdRig rig;
  Running  run(rig);
  CHECK(run.made());
  if (!run.made()) return;
  rig.host().cl_handle(host_shared, CL_SUCCESS);
  void *const first = rig.root().svm_alloc(run.context(), CL_MEM_READ_WRITE, shared_bytes, 0u);
  rig.host().cl_handle(host_shared + 0x1000u, CL_SUCCESS);
  void *const second = rig.root().svm_alloc(run.context(), CL_MEM_READ_WRITE, shared_bytes, 0u);
  CHECK(first != nullptr && second != nullptr);
  if (first == nullptr || second == nullptr) return;

  std::uint32_t sent = rig.kmt().commands();
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 1u);
  cl_event copied = nullptr;
  CHECK_EQ(table_of(run.queue())->clEnqueueSVMMemcpy(run.queue(), CL_FALSE, second, static_cast<char *>(first) + 16,
                                                     64u, 1u, run.gate(), &copied),
           CL_SUCCESS);
  CHECK(copied != nullptr);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueSVMMemcpy);
  CHECK(carries(rig.kmt(), sent, host_shared + 0x1000u));
  CHECK(carries(rig.kmt(), sent, host_shared + 16u));
  CHECK(carries(rig.kmt(), sent, host_gate));
  sent = rig.kmt().commands();
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 2u);
  rig.host().cl_result(CL_SUCCESS);
  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().svm_memcpy(run.queue(), CL_TRUE, first, second, 64u, nullptr), CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(sent + 3u), av::clproto::kCmd_clReleaseEvent);

  unsigned char from[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u}, into[8] = {};
  run.drains();
  sent = rig.kmt().commands();
  CHECK_EQ(rig.root().svm_memcpy(run.queue(), CL_TRUE, into, from, sizeof from, nullptr), CL_SUCCESS);
  CHECK_EQ(std::memcmp(into, from, sizeof from), 0);
  CHECK_EQ(rig.kmt().commands(), sent + 4u);
  sent = rig.kmt().commands();
  CHECK_EQ(rig.root().svm_memcpy(run.queue(), CL_TRUE, from + 2, from, 4u, nullptr), CL_MEM_COPY_OVERLAP);
  CHECK_EQ(rig.root().svm_memcpy(run.queue(), CL_TRUE, from, from + 2, 4u, nullptr), CL_MEM_COPY_OVERLAP);
  CHECK_EQ(rig.root().svm_memcpy(run.queue(), CL_TRUE, nullptr, from, 4u, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().svm_memcpy(reinterpret_cast<cl_command_queue>(run.kernel()), CL_TRUE, into, from, 4u, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().svm_memcpy(run.queue(), CL_TRUE, static_cast<char *>(first) + 200, second, 64u, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent);

  const std::uint32_t pattern = 0x5A5A5A5Au;
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 3u);
  cl_event filled = nullptr;
  CHECK_EQ(table_of(run.queue())->clEnqueueSVMMemFill(run.queue(), first, &pattern, sizeof pattern, 64u, 1u,
                                                      run.gate(), &filled),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueSVMMemFill);
  CHECK(carries(rig.kmt(), sent, host_shared));
  sent = rig.kmt().commands();
  CHECK_EQ(rig.root().svm_mem_fill(run.queue(), first, nullptr, 4u, 64u, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().svm_mem_fill(run.queue(), first, &pattern, sizeof pattern, 6u, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().svm_mem_fill(run.queue(), from, &pattern, sizeof pattern, 8u, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().svm_mem_fill(run.queue(), static_cast<char *>(first) + 240, &pattern, sizeof pattern, 64u,
                                   nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent);

  const void *migrated[1] = {first};
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 4u);
  cl_event moved = nullptr;
  CHECK_EQ(table_of(run.queue())->clEnqueueSVMMigrateMem(run.queue(), 1u, migrated, nullptr, 0u, 1u, run.gate(),
                                                         &moved),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clEnqueueMarkerWithWaitList);
  CHECK(carries(rig.kmt(), sent, host_gate));
  const void *plain[1] = {from};
  CHECK_EQ(rig.root().enqueue_svm_migrate(run.queue(), 1u, plain, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_svm_migrate(run.queue(), 0u, migrated, nullptr), CL_INVALID_VALUE);

  run.drains();
  Freed freed = {};
  void *pointers[1] = {first};
  CHECK_EQ(table_of(run.queue())->clEnqueueSVMFree(run.queue(), 1u, pointers, &frees, &freed, 0u, nullptr, nullptr),
           CL_SUCCESS);
  CHECK(freed.queue == run.queue());
  CHECK_EQ(freed.count, 1u);
  CHECK(freed.first == first);
  sent = rig.kmt().commands();
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  void *both[2] = {first, second};
  CHECK_EQ(rig.root().enqueue_svm_free(run.queue(), 2u, both, nullptr, nullptr, nullptr), CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 2u);
  CHECK_EQ(rig.kmt().command_id(sent), av::clproto::kCmd_clSVMFree);
  void *nothing[1] = {nullptr};
  CHECK_EQ(rig.root().enqueue_svm_free(run.queue(), 1u, nothing, nullptr, nullptr, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().enqueue_svm_free(run.queue(), 0u, both, nullptr, nullptr, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().queued_answers(), 0u);
  CHECK(rig.root().invariants_hold()); }
