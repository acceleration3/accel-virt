/* The OpenCL server dispatch between a guest's bytes and the host ICD.
 *
 * Driven over RecordingClDispatch, which holds handles the way OpenClBackend
 * does and has no OpenCL behind it. What is asserted is the contract with the
 * ICD (feature 044, research section 2): a command that names an object this
 * host did not make, or asks the ICD to read or write past what crossed, is
 * refused before any call - and a well-formed one still runs. */
#include "harness/executor/recording_cl_dispatch.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/opencl/cl_dispatch_server.h"
#include "proto/generated/opencl/cl_protocol_ids.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

using av::RecordingClDispatch;
using av::WireArena;
using av::WireReader;
using av::WireWriter;
using av::clproto::HandleKind;

namespace {

constexpr std::size_t block_bytes = 16384u;
constexpr std::size_t frame_bytes = 1024u;
constexpr std::size_t reply_bytes = 4064u;

/* One guest context's worth of decoding: the block, the dispatch that holds
 * its handles, and the answer to the last command. */
class ClSession {
public:
    ClSession() : block_{}, arena_(block_, sizeof block_), dispatch_(block_, arena_), reply_{},
                  answered_(0u) {}

    bool run(std::uint32_t command, const WireWriter &frame, const std::uint8_t *bytes)
    {
        arena_.reset();
        WireReader payload(bytes, frame.offset());
        WireWriter answer(reply_, sizeof reply_);
        const bool ran = av::clproto::dispatch_command(dispatch_, command, payload, answer, arena_);
        answered_ = answer.offset();
        return ran;
    }

    WireReader answer() const { return WireReader(reply_, answered_); }
    RecordingClDispatch &dispatch() { return dispatch_; }

    /* clGetPlatformIDs, clGetDeviceIDs, clCreateContext and
     * clCreateCommandQueue, as a guest starts every program: the numbers each
     * answered, which the host now holds. */
    void make_chain(std::uint64_t &platform, std::uint64_t &device, std::uint64_t &context,
                    std::uint64_t &queue)
    {
        std::uint8_t bytes[frame_bytes];
        { WireWriter frame(bytes, sizeof bytes);
          frame.write_varint(1u);
          frame.write_pod<std::uint8_t>(1u);
          frame.write_pod<std::uint8_t>(0u);
          CHECK(run(av::clproto::kCmd_clGetPlatformIDs, frame, bytes));
          platform = first_listed(); }
        { WireWriter frame(bytes, sizeof bytes);
          frame.write_pod<std::uint64_t>(platform);
          frame.write_varint(CL_DEVICE_TYPE_ALL);
          frame.write_varint(1u);
          frame.write_pod<std::uint8_t>(1u);
          frame.write_pod<std::uint8_t>(0u);
          CHECK(run(av::clproto::kCmd_clGetDeviceIDs, frame, bytes));
          device = first_listed(); }
        { WireWriter frame(bytes, sizeof bytes);
          frame.write_pod<std::uint8_t>(1u);
          frame.write_varint(2u);
          frame.write_pod<std::uint64_t>(CL_CONTEXT_PLATFORM);
          frame.write_pod<std::uint64_t>(platform);
          frame.write_varint(1u);
          frame.write_pod<std::uint8_t>(1u);
          frame.write_varint(1u);
          frame.write_pod<std::uint64_t>(device);
          frame.write_pod<std::uint8_t>(0u);
          CHECK(run(av::clproto::kCmd_clCreateContext, frame, bytes));
          context = returned(); }
        { WireWriter frame(bytes, sizeof bytes);
          frame.write_pod<std::uint64_t>(context);
          frame.write_pod<std::uint64_t>(device);
          frame.write_varint(0u);
          frame.write_pod<std::uint8_t>(0u);
          CHECK(run(av::clproto::kCmd_clCreateCommandQueue, frame, bytes));
          queue = returned(); }
    }

    /* A buffer of `size` bytes in `context`, copied from nothing. */
    std::uint64_t make_buffer(std::uint64_t context, std::uint64_t size)
    {
        std::uint8_t bytes[frame_bytes];
        WireWriter frame(bytes, sizeof bytes);
        frame.write_pod<std::uint64_t>(context);
        frame.write_varint(CL_MEM_READ_WRITE);
        frame.write_varint(size);
        frame.write_pod<std::uint8_t>(0u);
        frame.write_pod<std::uint8_t>(0u);
        CHECK(run(av::clproto::kCmd_clCreateBuffer, frame, bytes));
        return returned();
    }

private:
    /* The first handle of an enumeration's answer. */
    std::uint64_t first_listed() const
    {
        WireReader reply = answer();
        std::uint64_t result = 0u;
        std::uint64_t count = 0u;
        std::uint64_t first = 0u;
        CHECK(reply.read_varint(result));
        CHECK(reply.read_varint(count));
        CHECK(count > 0u);
        CHECK(reply.read_pod(first));
        return first;
    }

    /* A creating call's answer. */
    std::uint64_t returned() const
    {
        WireReader reply = answer();
        std::uint64_t handle = 0u;
        CHECK(reply.read_pod(handle));
        return handle;
    }

    alignas(16) std::uint8_t block_[block_bytes];
    WireArena           arena_;
    RecordingClDispatch dispatch_;
    std::uint8_t        reply_[reply_bytes];
    std::size_t         answered_;
};

/* clReleaseX(handle), the one-handle frame every lifetime test sends. */
bool send_handle(ClSession &session, std::uint32_t command, std::uint64_t handle)
{
    std::uint8_t bytes[16];
    WireWriter frame(bytes, sizeof bytes);
    frame.write_pod<std::uint64_t>(handle);
    return session.run(command, frame, bytes);
}

/* clEnqueueReadBuffer(queue, buffer, blocking, 0, size, ptr of `capacity`,
 * no events, no event out). */
bool read_buffer(ClSession &session, std::uint64_t queue, std::uint64_t buffer, std::uint64_t size,
                 std::uint64_t capacity)
{
    std::uint8_t bytes[frame_bytes];
    WireWriter frame(bytes, sizeof bytes);
    frame.write_pod<std::uint64_t>(queue);
    frame.write_pod<std::uint64_t>(buffer);
    frame.write_varint(CL_TRUE);
    frame.write_varint(0u);
    frame.write_varint(size);
    frame.write_pod<std::uint8_t>(1u);
    frame.write_varint(capacity);
    frame.write_varint(0u);
    frame.write_pod<std::uint8_t>(0u);
    frame.write_pod<std::uint8_t>(0u);
    return session.run(av::clproto::kCmd_clEnqueueReadBuffer, frame, bytes);
}

}  // namespace

TEST(opencl_dispatch, a_program_s_first_objects_resolve_back_as_what_they_are)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  CHECK(platform != 0u && device != 0u && context != 0u && queue != 0u);
  RecordingClDispatch &dispatch = session.dispatch();
  CHECK_EQ(dispatch.calls(), 4u);
  CHECK_EQ(dispatch.strangers(), 0u);
  /* The ICD had two platforms and two devices, and the guest had room for
   * one of each: what the guest was never told of is not its to name. */
  CHECK_EQ(dispatch.handles().count(), 4u);
  CHECK(dispatch.handle_for(HandleKind::context, context) != nullptr);
  CHECK(dispatch.handle_for(HandleKind::command_queue, queue) != nullptr);
  CHECK(dispatch.handles().invariants_hold()); }

/* The hole this feature closes: a number the guest made up, handed to the ICD
 * as a pointer. */
TEST(opencl_dispatch, a_handle_the_host_never_made_is_refused_before_any_call)
{ ClSession session;
  CHECK(!send_handle(session, av::clproto::kCmd_clReleaseContext, 0x4141414141414141u));
  CHECK(!send_handle(session, av::clproto::kCmd_clFinish, 0x1000u));
  CHECK_EQ(session.dispatch().calls(), 0u); }

TEST(opencl_dispatch, an_object_named_as_another_kind_is_refused)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  const std::uint32_t before = session.dispatch().calls();
  CHECK(!send_handle(session, av::clproto::kCmd_clReleaseContext, queue));
  CHECK(!send_handle(session, av::clproto::kCmd_clFinish, context));
  CHECK(!send_handle(session, av::clproto::kCmd_clReleaseMemObject, device));
  CHECK_EQ(session.dispatch().calls(), before);
  CHECK(send_handle(session, av::clproto::kCmd_clFinish, queue));
  CHECK_EQ(session.dispatch().last_called(), av::clproto::kCmd_clFinish); }

/* Null is the API's own, and the ICD answers it with an error code. */
TEST(opencl_dispatch, a_null_handle_reaches_the_call_as_null)
{ ClSession session;
  CHECK(send_handle(session, av::clproto::kCmd_clFinish, 0u));
  CHECK_EQ(session.dispatch().calls(), 1u);
  CHECK_EQ(session.dispatch().strangers(), 0u); }

/* A retain keeps an object through one release; the last release makes its
 * number name nothing, because the ICD may have freed it. */
TEST(opencl_dispatch, an_object_released_for_the_last_time_is_refused_afterwards)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  const std::uint64_t buffer = session.make_buffer(context, 64u);
  CHECK(send_handle(session, av::clproto::kCmd_clRetainMemObject, buffer));
  CHECK(send_handle(session, av::clproto::kCmd_clReleaseMemObject, buffer));
  CHECK(session.dispatch().handle_for(HandleKind::mem, buffer) != nullptr);
  CHECK(send_handle(session, av::clproto::kCmd_clReleaseMemObject, buffer));
  CHECK(session.dispatch().handle_for(HandleKind::mem, buffer) == nullptr);
  const std::uint32_t before = session.dispatch().calls();
  CHECK(!send_handle(session, av::clproto::kCmd_clReleaseMemObject, buffer));
  CHECK_EQ(session.dispatch().calls(), before);
  CHECK(session.dispatch().handles().invariants_hold()); }

/* T006: the ICD writes `size` bytes at the pointer, and the pointer is room
 * for `capacity`. */
TEST(opencl_dispatch, a_read_larger_than_its_room_is_refused_before_the_call)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  const std::uint64_t buffer = session.make_buffer(context, 4096u);
  const std::uint32_t before = session.dispatch().calls();
  CHECK(!read_buffer(session, queue, buffer, 65u, 64u));
  CHECK(!read_buffer(session, queue, buffer, ~std::uint64_t{0}, 64u));
  CHECK_EQ(session.dispatch().calls(), before);
  /* At exactly its room it runs, and writes nowhere but the room. */
  CHECK(read_buffer(session, queue, buffer, 64u, 64u));
  CHECK_EQ(session.dispatch().calls(), before + 1u);
  CHECK_EQ(session.dispatch().strays(), 0u);
  CHECK(read_buffer(session, queue, buffer, 0u, 64u));
  CHECK_EQ(session.dispatch().strays(), 0u); }

TEST(opencl_dispatch, a_count_larger_than_the_list_that_crossed_is_refused)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  const std::uint32_t before = session.dispatch().calls();
  std::uint8_t bytes[frame_bytes];
  /* clCreateContext(null, num_devices 2, one device). */
  WireWriter frame(bytes, sizeof bytes);
  frame.write_pod<std::uint8_t>(0u);
  frame.write_varint(2u);
  frame.write_pod<std::uint8_t>(1u);
  frame.write_varint(1u);
  frame.write_pod<std::uint64_t>(device);
  frame.write_pod<std::uint8_t>(0u);
  CHECK(!session.run(av::clproto::kCmd_clCreateContext, frame, bytes));
  CHECK_EQ(session.dispatch().calls(), before); }

/* A property whose value is a host pointer by definition - here a GL context
 * - is one no guest can know, and a platform the host never enumerated is a
 * pointer like any other. */
TEST(opencl_dispatch, context_properties_holding_a_host_pointer_or_an_unknown_platform_are_refused)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  const std::uint32_t before = session.dispatch().calls();
  constexpr std::uint64_t gl_context_khr = 0x2008u;
  const std::uint64_t keys[] = {gl_context_khr, std::uint64_t{CL_CONTEXT_PLATFORM}};
  for (const std::uint64_t key : keys) {
      std::uint8_t bytes[frame_bytes];
      WireWriter frame(bytes, sizeof bytes);
      frame.write_pod<std::uint8_t>(1u);
      frame.write_varint(2u);
      frame.write_pod<std::uint64_t>(key);
      frame.write_pod<std::uint64_t>(0x7F0000001000u);
      frame.write_varint(0u);
      frame.write_pod<std::uint8_t>(0u);
      frame.write_pod<std::uint8_t>(0u);
      CHECK(!session.run(av::clproto::kCmd_clCreateContext, frame, bytes));
  }
  CHECK_EQ(session.dispatch().calls(), before); }

/* The copied bytes live in a block the next command reuses; an ICD told to
 * use them in place would keep reading and writing it. And bytes shorter than
 * the size the ICD copies are a read past the block. */
TEST(opencl_dispatch, a_buffer_over_the_guest_s_bytes_is_refused_unless_copied_whole)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  const std::uint32_t before = session.dispatch().calls();
  const std::uint8_t pattern[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  struct Case { std::uint64_t flags; std::uint64_t size; bool runs; };
  constexpr Case cases[] = {{CL_MEM_USE_HOST_PTR, 8u, false},
                            {CL_MEM_COPY_HOST_PTR, 9u, false},
                            {CL_MEM_COPY_HOST_PTR, 8u, true}};
  std::uint32_t ran = 0u;
  for (const Case &item : cases) {
      std::uint8_t bytes[frame_bytes];
      WireWriter frame(bytes, sizeof bytes);
      frame.write_pod<std::uint64_t>(context);
      frame.write_varint(item.flags);
      frame.write_varint(item.size);
      frame.write_pod<std::uint8_t>(1u);
      frame.write_varint(sizeof pattern);
      frame.write(pattern, sizeof pattern);
      frame.write_pod<std::uint8_t>(0u);
      CHECK_EQ(session.run(av::clproto::kCmd_clCreateBuffer, frame, bytes), item.runs);
      if (item.runs) ran++;
  }
  CHECK_EQ(session.dispatch().calls(), before + ran);
  CHECK_EQ(session.dispatch().strays(), 0u); }

/* A rectangle whose origin and pitches multiply past 64 bits is a size the
 * host cannot check, so it is not one it runs. */
TEST(opencl_dispatch, a_rectangle_read_that_overflows_or_outgrows_its_room_is_refused)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  const std::uint64_t buffer = session.make_buffer(context, 4096u);
  const std::uint32_t before = session.dispatch().calls();
  struct Case { std::uint64_t origin_z; std::uint64_t region_x; std::uint64_t slice; std::uint64_t capacity; bool runs; };
  /* Region {x, 2, 1} at pitches 0 reaches 2x bytes past an origin of
   * z slices of 2x bytes each. */
  const Case cases[] = {{0u, 16u, 0u, 32u, true},
                        {0u, 17u, 0u, 32u, false},
                        {1u, 4u, 0u, 15u, false},
                        {1u, 4u, 0u, 16u, true},
                        {~std::uint64_t{0}, 4u, 1u << 20, 64u, false},
                        {0u, 0u, 0u, 0u, true}};
  std::uint32_t ran = 0u;
  for (const Case &item : cases) {
      std::uint8_t bytes[frame_bytes];
      WireWriter frame(bytes, sizeof bytes);
      frame.write_pod<std::uint64_t>(queue);
      frame.write_pod<std::uint64_t>(buffer);
      frame.write_varint(CL_TRUE);
      /* buffer_origin, host_origin, region: three each. */
      const std::uint64_t triples[3][3] = {{0u, 0u, 0u}, {0u, 0u, item.origin_z},
                                           {item.region_x, 2u, 1u}};
      for (const auto &triple : triples) {
          frame.write_pod<std::uint8_t>(1u);
          frame.write_varint(3u);
          for (const std::uint64_t value : triple) frame.write_varint(value);
      }
      frame.write_varint(0u);
      frame.write_varint(0u);
      frame.write_varint(0u);
      frame.write_varint(item.slice);
      frame.write_pod<std::uint8_t>(1u);
      frame.write_varint(item.capacity);
      frame.write_varint(0u);
      frame.write_pod<std::uint8_t>(0u);
      frame.write_pod<std::uint8_t>(0u);
      CHECK_EQ(session.run(av::clproto::kCmd_clEnqueueReadBufferRect, frame, bytes), item.runs);
      if (item.runs) ran++;
  }
  CHECK_EQ(session.dispatch().calls(), before + ran); }

/* SVM and USM pointers: this wire makes neither, so every one is invented. */
TEST(opencl_dispatch, kernel_exec_info_naming_host_pointers_is_refused)
{ ClSession session;
  constexpr std::uint64_t names[] = {CL_KERNEL_EXEC_INFO_SVM_PTRS, 0x40B8u, 0x4203u};
  for (const std::uint64_t name : names) {
      std::uint8_t bytes[frame_bytes];
      WireWriter frame(bytes, sizeof bytes);
      frame.write_pod<std::uint64_t>(0u);
      frame.write_varint(name);
      frame.write_varint(8u);
      frame.write_pod<std::uint8_t>(1u);
      frame.write_varint(8u);
      frame.write_pod<std::uint64_t>(0x7F0000001000u);
      CHECK(!session.run(av::clproto::kCmd_clSetKernelExecInfo, frame, bytes));
  }
  CHECK_EQ(session.dispatch().calls(), 0u); }

/* An event a call made is one a later wait may name; one it did not is not. */
TEST(opencl_dispatch, an_event_the_host_made_may_be_waited_on_and_no_other)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  std::uint8_t bytes[frame_bytes];
  { WireWriter frame(bytes, sizeof bytes);
    frame.write_pod<std::uint64_t>(queue);
    frame.write_varint(0u);
    frame.write_pod<std::uint8_t>(0u);
    frame.write_pod<std::uint8_t>(1u);
    CHECK(session.run(av::clproto::kCmd_clEnqueueMarkerWithWaitList, frame, bytes)); }
  WireReader reply = session.answer();
  std::uint64_t result = 0u;
  std::uint64_t event = 0u;
  CHECK(reply.read_varint(result));
  CHECK(reply.read_pod(event));
  CHECK(session.dispatch().handle_for(HandleKind::event, event) != nullptr);
  const std::uint64_t nameds[] = {event, event + 1u};
  for (const std::uint64_t named : nameds) {
      WireWriter frame(bytes, sizeof bytes);
      frame.write_varint(1u);
      frame.write_pod<std::uint8_t>(1u);
      frame.write_varint(1u);
      frame.write_pod<std::uint64_t>(named);
      CHECK_EQ(session.run(av::clproto::kCmd_clWaitForEvents, frame, bytes), named == event);
  }
  CHECK_EQ(session.dispatch().strangers(), 0u); }

/* An image description carries a buffer handle inside the struct's bytes. */
TEST(opencl_dispatch, an_image_description_naming_an_unknown_buffer_is_refused)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  const std::uint64_t buffer = session.make_buffer(context, 64u);
  const std::uint32_t before = session.dispatch().calls();
  const std::uint64_t nameds[] = {buffer + 8u, buffer};
  for (const std::uint64_t named : nameds) {
      cl_image_desc description = {};
      description.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;
      description.image_width = 16u;
      description.buffer = reinterpret_cast<cl_mem>(static_cast<std::uintptr_t>(named));
      const cl_image_format format = {CL_RGBA, CL_UNORM_INT8};
      std::uint8_t bytes[frame_bytes];
      WireWriter frame(bytes, sizeof bytes);
      frame.write_pod<std::uint64_t>(context);
      frame.write_varint(CL_MEM_READ_ONLY);
      frame.write_pod<std::uint8_t>(1u);
      frame.write(&format, sizeof format);
      frame.write_pod<std::uint8_t>(1u);
      frame.write(&description, sizeof description);
      frame.write_pod<std::uint8_t>(0u);
      frame.write_pod<std::uint8_t>(0u);
      CHECK_EQ(session.run(av::clproto::kCmd_clCreateImage, frame, bytes), named == buffer);
  }
  CHECK_EQ(session.dispatch().calls(), before + 1u);
  CHECK_EQ(session.dispatch().strangers(), 0u); }

/* An image transfer's size is the image's element size over the region, and
 * an image this ICD cannot say the element size of is not read into. */
TEST(opencl_dispatch, an_image_read_whose_element_size_is_unknown_is_refused)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  const std::uint64_t image = session.make_buffer(context, 64u);
  const std::uint32_t before = session.dispatch().calls();
  std::uint8_t bytes[frame_bytes];
  WireWriter frame(bytes, sizeof bytes);
  frame.write_pod<std::uint64_t>(queue);
  frame.write_pod<std::uint64_t>(image);
  frame.write_varint(CL_TRUE);
  const std::uint64_t firsts[] = {0u, 1u};
  for (const std::uint64_t first : firsts) {
      frame.write_pod<std::uint8_t>(1u);
      frame.write_varint(3u);
      frame.write_varint(first);
      frame.write_varint(first);
      frame.write_varint(first);
  }
  frame.write_varint(0u);
  frame.write_varint(0u);
  frame.write_pod<std::uint8_t>(1u);
  frame.write_varint(64u);
  frame.write_varint(0u);
  frame.write_pod<std::uint8_t>(0u);
  frame.write_pod<std::uint8_t>(0u);
  CHECK(!session.run(av::clproto::kCmd_clEnqueueReadImage, frame, bytes));
  /* The one call made was the element-size question itself. */
  CHECK_EQ(session.dispatch().calls(), before + 1u);
  CHECK_EQ(session.dispatch().last_called(), av::clproto::kCmd_clGetImageInfo); }
