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
#include <cstring>

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

namespace {

using Declared = RecordingClDispatch::Declared;

/* clCreateProgramWithSource(context, one source), then clBuildProgram(program,
 * no devices, `options`): the program's number. */
std::uint64_t make_program(ClSession &session, std::uint64_t context, const char *options)
{
    constexpr char source[] = "kernel void add(global float *a) {}";
    std::uint8_t bytes[frame_bytes];
    std::uint64_t program = 0u;
    { WireWriter frame(bytes, sizeof bytes);
      frame.write_pod<std::uint64_t>(context);
      frame.write_varint(1u);
      frame.write_pod<std::uint8_t>(1u);
      frame.write_varint(1u);
      frame.write_varint(sizeof source - 1u);
      frame.write(source, sizeof source - 1u);
      frame.write_pod<std::uint8_t>(0u);
      CHECK(session.run(av::clproto::kCmd_clCreateProgramWithSource, frame, bytes));
      WireReader reply = session.answer();
      CHECK(reply.read_pod(program)); }
    WireWriter frame(bytes, sizeof bytes);
    frame.write_pod<std::uint64_t>(program);
    frame.write_varint(0u);
    frame.write_pod<std::uint8_t>(0u);
    frame.write_pod<std::uint8_t>(options != nullptr ? 1u : 0u);
    if (options != nullptr) {
        const std::size_t length = std::strlen(options);
        frame.write_varint(length);
        frame.write(options, length);
    }
    CHECK(session.run(av::clproto::kCmd_clBuildProgram, frame, bytes));
    return program;
}

std::uint64_t make_kernel(ClSession &session, std::uint64_t context)
{
    const std::uint64_t program = make_program(session, context, nullptr);
    constexpr char name[] = "add";
    std::uint8_t bytes[frame_bytes];
    WireWriter frame(bytes, sizeof bytes);
    frame.write_pod<std::uint64_t>(program);
    frame.write_pod<std::uint8_t>(1u);
    frame.write_varint(sizeof name - 1u);
    frame.write(name, sizeof name - 1u);
    frame.write_pod<std::uint8_t>(0u);
    CHECK(session.run(av::clproto::kCmd_clCreateKernel, frame, bytes));
    WireReader reply = session.answer();
    std::uint64_t kernel = 0u;
    CHECK(reply.read_pod(kernel));
    return kernel;
}

std::uint64_t make_sampler(ClSession &session, std::uint64_t context)
{
    std::uint8_t bytes[frame_bytes];
    WireWriter frame(bytes, sizeof bytes);
    frame.write_pod<std::uint64_t>(context);
    frame.write_varint(CL_FALSE);
    frame.write_varint(CL_ADDRESS_CLAMP);
    frame.write_varint(CL_FILTER_NEAREST);
    frame.write_pod<std::uint8_t>(0u);
    CHECK(session.run(av::clproto::kCmd_clCreateSampler, frame, bytes));
    WireReader reply = session.answer();
    std::uint64_t sampler = 0u;
    CHECK(reply.read_pod(sampler));
    return sampler;
}

/* The first index whose argument the fake's kernels declare as `declared`. */
std::uint32_t index_declared(const RecordingClDispatch &dispatch, Declared declared)
{
    std::uint32_t index = 0u;
    while (dispatch.argument_declared(index) != declared) index++;
    return index;
}

/* clSetKernelArg(kernel, index, size, `size` bytes of `value` or none). */
bool set_argument(ClSession &session, std::uint64_t kernel, std::uint32_t index, std::uint64_t size,
                  bool present, std::uint64_t value)
{
    std::uint8_t bytes[frame_bytes];
    WireWriter frame(bytes, sizeof bytes);
    frame.write_pod<std::uint64_t>(kernel);
    frame.write_varint(index);
    frame.write_varint(size);
    frame.write_pod<std::uint8_t>(present ? 1u : 0u);
    if (present) {
        frame.write_varint(size);
        frame.write(&value, static_cast<std::size_t>(size));
    }
    return session.run(av::clproto::kCmd_clSetKernelArg, frame, bytes);
}

}  // namespace

/* Without -cl-kernel-arg-info the ICD cannot later say which arguments are
 * objects, and every eight-byte argument of the kernel would be refused. */
TEST(opencl_dispatch, every_build_and_compile_asks_the_icd_to_keep_argument_information)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  const std::uint64_t bare = make_program(session, context, nullptr);
  CHECK_EQ(session.dispatch().last_called(), av::clproto::kCmd_clBuildProgram);
  (void)make_program(session, context, "-O2 -DWIDTH=4");
  CHECK_EQ(session.dispatch().last_called(), av::clproto::kCmd_clBuildProgram);
  constexpr char options[] = "-DX";
  std::uint8_t bytes[frame_bytes];
  WireWriter frame(bytes, sizeof bytes);
  frame.write_pod<std::uint64_t>(bare);
  frame.write_varint(0u);
  frame.write_pod<std::uint8_t>(0u);
  frame.write_pod<std::uint8_t>(1u);
  frame.write_varint(sizeof options - 1u);
  frame.write(options, sizeof options - 1u);
  frame.write_varint(0u);
  frame.write_pod<std::uint8_t>(0u);
  frame.write_pod<std::uint8_t>(0u);
  CHECK(session.run(av::clproto::kCmd_clCompileProgram, frame, bytes));
  CHECK_EQ(session.dispatch().last_called(), av::clproto::kCmd_clCompileProgram);
  CHECK_EQ(session.dispatch().builds_without_argument_info(), 0u); }

/* The hole: eight guest bytes where the kernel reads a cl_mem, an image or a
 * cl_sampler, which the ICD dereferences. Each argument is held to what the
 * kernel declared - an object this host made, of that kind - and one the ICD
 * cannot describe may not be an object's width at all. */
TEST(opencl_dispatch, a_kernel_argument_reaches_the_icd_only_as_what_the_kernel_declared)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  const std::uint64_t buffer = session.make_buffer(context, 64u);
  const std::uint64_t sampler = make_sampler(session, context);
  const std::uint64_t kernel = make_kernel(session, context);
  constexpr std::uint64_t invented = 0x7F0000001000u;
  struct Case { Declared declared; std::uint64_t size; bool present; std::uint64_t value; bool runs; };
  const Case cases[] = {
      {Declared::buffer, 8u, true, buffer, true},
      {Declared::buffer, 8u, true, invented, false},
      {Declared::buffer, 8u, true, sampler, false},
      {Declared::buffer, 8u, true, 0u, true},
      {Declared::buffer, 8u, false, 0u, true},
      {Declared::buffer, 4u, true, buffer, false},
      {Declared::image, 8u, true, buffer, true},
      {Declared::image, 8u, true, sampler, false},
      {Declared::image, 8u, true, 0u, false},
      {Declared::image, 8u, false, 0u, false},
      {Declared::sampler, 8u, true, sampler, true},
      {Declared::sampler, 8u, true, buffer, false},
      {Declared::sampler, 8u, true, 0u, false},
      {Declared::scalar, 8u, true, invented, true},
      {Declared::local, 64u, false, 0u, true},
      {Declared::local, 8u, true, buffer, false},
      {Declared::undeclared, 8u, true, buffer, false},
      {Declared::undeclared, 8u, false, 0u, true},
      {Declared::undeclared, 4u, true, invented, true},
      {Declared::pipe, 8u, true, buffer, true},
      {Declared::pipe, 8u, true, invented, false},
      {Declared::constant, 8u, true, invented, false},
      {Declared::constant, 8u, true, buffer, true},
  };
  for (const Case &item : cases) {
      const std::uint32_t index = index_declared(session.dispatch(), item.declared);
      CHECK_EQ(set_argument(session, kernel, index, item.size, item.present, item.value), item.runs);
      CHECK_EQ(session.dispatch().last_called() == av::clproto::kCmd_clSetKernelArg, item.runs);
      CHECK_EQ(session.dispatch().strangers(), 0u);
      CHECK_EQ(session.dispatch().strays(), 0u);
  } }

/* The index the guest names is the one asked about: an object argument past
 * the first few is refused an invented value as the first would be. */
TEST(opencl_dispatch, a_kernel_argument_is_described_by_its_own_index)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  const std::uint64_t kernel = make_kernel(session, context);
  constexpr std::uint64_t invented = 0x7F0000001000u;
  const std::uint32_t scalar = index_declared(session.dispatch(), Declared::scalar);
  const std::uint32_t indices[] = {scalar, scalar + RecordingClDispatch::declared_arguments,
                                   RecordingClDispatch::declared_arguments};
  for (const std::uint32_t index : indices) {
      CHECK_EQ(set_argument(session, kernel, index, 8u, true, invented),
               session.dispatch().argument_declared(index) == Declared::scalar);
  }
  CHECK_EQ(session.dispatch().strangers(), 0u); }

namespace {

constexpr std::size_t image_frame_bytes = 1024u;

/* clCreateImage(context, COPY_HOST_PTR, format, description, `crossed` bytes of
 * host pointer): whether it ran. */
bool create_image(ClSession &session, std::uint64_t context, const cl_image_format &format,
                  const cl_image_desc *description, std::uint64_t crossed)
{
    std::uint8_t bytes[image_frame_bytes];
    const std::uint8_t filler[512] = {};
    WireWriter frame(bytes, sizeof bytes);
    frame.write_pod<std::uint64_t>(context);
    frame.write_varint(CL_MEM_COPY_HOST_PTR);
    frame.write_pod<std::uint8_t>(1u);
    frame.write(&format, sizeof format);
    frame.write_pod<std::uint8_t>(description != nullptr ? 1u : 0u);
    if (description != nullptr) frame.write(description, sizeof *description);
    frame.write_pod<std::uint8_t>(1u);
    frame.write_varint(crossed);
    frame.write(filler, static_cast<std::size_t>(crossed));
    frame.write_pod<std::uint8_t>(0u);
    return session.run(av::clproto::kCmd_clCreateImage, frame, bytes);
}

cl_image_desc described(cl_mem_object_type type, std::size_t width, std::size_t height,
                        std::size_t depth, std::size_t array_size)
{
    cl_image_desc description = {};
    description.image_type = type;
    description.image_width = width;
    description.image_height = height;
    description.image_depth = depth;
    description.image_array_size = array_size;
    return description;
}

}  // namespace

/* The ICD copies the format's elements over the extent out of the host
 * pointer, at the pitches it was given, and the bytes that crossed are all
 * the pointer has. */
TEST(opencl_dispatch, an_image_copied_from_fewer_bytes_than_its_format_and_extent_is_refused)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  const cl_image_format rgba8 = {CL_RGBA, CL_UNORM_INT8};
  const cl_image_format float_r = {CL_R, CL_FLOAT};
  const cl_image_format rgb565 = {CL_RGB, CL_UNORM_SHORT_565};
  cl_image_desc padded = described(CL_MEM_OBJECT_IMAGE2D, 4u, 4u, 0u, 0u);
  padded.image_row_pitch = 20u;
  cl_image_desc sliced = described(CL_MEM_OBJECT_IMAGE3D, 2u, 2u, 3u, 0u);
  sliced.image_slice_pitch = 40u;
  struct Case { cl_image_format format; cl_image_desc description; std::uint64_t crossed; bool runs; };
  const Case cases[] = {
      {rgba8, described(CL_MEM_OBJECT_IMAGE2D, 4u, 4u, 0u, 0u), 63u, false},
      {rgba8, described(CL_MEM_OBJECT_IMAGE2D, 4u, 4u, 0u, 0u), 64u, true},
      {rgba8, padded, 79u, false},
      {rgba8, padded, 80u, true},
      {float_r, described(CL_MEM_OBJECT_IMAGE2D_ARRAY, 4u, 4u, 0u, 3u), 191u, false},
      {float_r, described(CL_MEM_OBJECT_IMAGE2D_ARRAY, 4u, 4u, 0u, 3u), 192u, true},
      {rgba8, described(CL_MEM_OBJECT_IMAGE1D_ARRAY, 8u, 0u, 0u, 4u), 127u, false},
      {rgba8, described(CL_MEM_OBJECT_IMAGE1D_ARRAY, 8u, 0u, 0u, 4u), 128u, true},
      {rgba8, described(CL_MEM_OBJECT_IMAGE1D, 8u, 5u, 0u, 0u), 31u, false},
      {rgba8, described(CL_MEM_OBJECT_IMAGE1D, 8u, 5u, 0u, 0u), 32u, true},
      {rgba8, sliced, 119u, false},
      {rgba8, sliced, 120u, true},
      {rgb565, described(CL_MEM_OBJECT_IMAGE2D, 4u, 4u, 0u, 0u), 31u, false},
      {rgb565, described(CL_MEM_OBJECT_IMAGE2D, 4u, 4u, 0u, 0u), 32u, true},
      /* A packed type in an order it does not pack, an order and an image
       * type this host does not know, and a width whose bytes pass 64 bits. */
      {{CL_RGBA, CL_UNORM_SHORT_565}, described(CL_MEM_OBJECT_IMAGE2D, 1u, 1u, 0u, 0u), 64u, false},
      {{0x1234u, CL_UNORM_INT8}, described(CL_MEM_OBJECT_IMAGE2D, 1u, 1u, 0u, 0u), 64u, false},
      {rgba8, described(0x1234u, 1u, 1u, 0u, 0u), 64u, false},
      {rgba8, described(CL_MEM_OBJECT_IMAGE2D, ~std::size_t{0} / 2u, 1u, 0u, 0u), 64u, false},
  };
  const std::uint32_t before = session.dispatch().calls();
  std::uint32_t ran = 0u;
  for (const Case &item : cases) {
      CHECK_EQ(create_image(session, context, item.format, &item.description, item.crossed), item.runs);
      if (item.runs) ran++;
  }
  CHECK(!create_image(session, context, rgba8, nullptr, 64u));
  CHECK_EQ(session.dispatch().calls(), before + ran);
  CHECK_EQ(session.dispatch().strays(), 0u); }

/* The 1.1 spellings carry the extent as arguments, and the properties
 * spelling in the description, and each is held to it the same way. */
TEST(opencl_dispatch, the_other_image_calls_hold_their_host_bytes_to_their_extent)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  const cl_image_format rgba8 = {CL_RGBA, CL_UNORM_INT8};
  const std::uint8_t filler[64] = {};
  const std::uint64_t crosseds[] = {47u, 48u};
  const std::uint32_t before = session.dispatch().calls();
  for (const std::uint64_t crossed : crosseds) {
      std::uint8_t bytes[frame_bytes];
      { WireWriter frame(bytes, sizeof bytes);
        frame.write_pod<std::uint64_t>(context);
        frame.write_varint(CL_MEM_COPY_HOST_PTR);
        frame.write_pod<std::uint8_t>(1u);
        frame.write(&rgba8, sizeof rgba8);
        frame.write_varint(4u);
        frame.write_varint(3u);
        frame.write_varint(0u);
        frame.write_pod<std::uint8_t>(1u);
        frame.write_varint(crossed);
        frame.write(filler, static_cast<std::size_t>(crossed));
        frame.write_pod<std::uint8_t>(0u);
        CHECK_EQ(session.run(av::clproto::kCmd_clCreateImage2D, frame, bytes), crossed == 48u); }
      { WireWriter frame(bytes, sizeof bytes);
        frame.write_pod<std::uint64_t>(context);
        frame.write_varint(CL_MEM_COPY_HOST_PTR);
        frame.write_pod<std::uint8_t>(1u);
        frame.write(&rgba8, sizeof rgba8);
        frame.write_varint(2u);
        frame.write_varint(2u);
        frame.write_varint(3u);
        frame.write_varint(0u);
        frame.write_varint(0u);
        frame.write_pod<std::uint8_t>(1u);
        frame.write_varint(crossed);
        frame.write(filler, static_cast<std::size_t>(crossed));
        frame.write_pod<std::uint8_t>(0u);
        CHECK_EQ(session.run(av::clproto::kCmd_clCreateImage3D, frame, bytes), crossed == 48u); }
      { const cl_image_desc description = described(CL_MEM_OBJECT_IMAGE2D, 4u, 3u, 0u, 0u);
        WireWriter frame(bytes, sizeof bytes);
        frame.write_pod<std::uint64_t>(context);
        frame.write_pod<std::uint8_t>(0u);
        frame.write_varint(CL_MEM_COPY_HOST_PTR);
        frame.write_pod<std::uint8_t>(1u);
        frame.write(&rgba8, sizeof rgba8);
        frame.write_pod<std::uint8_t>(1u);
        frame.write(&description, sizeof description);
        frame.write_pod<std::uint8_t>(1u);
        frame.write_varint(crossed);
        frame.write(filler, static_cast<std::size_t>(crossed));
        frame.write_pod<std::uint8_t>(0u);
        CHECK_EQ(session.run(av::clproto::kCmd_clCreateImageWithProperties, frame, bytes),
                 crossed == 48u); }
  }
  CHECK_EQ(session.dispatch().calls(), before + 3u);
  CHECK_EQ(session.dispatch().strays(), 0u); }

namespace {

/* An answer's result, as the decoder writes it. */
cl_int answered_result(WireReader &reply)
{
    std::uint64_t result = 0u;
    CHECK(reply.read_varint(result));
    return static_cast<cl_int>(static_cast<std::int64_t>(result));
}

/* clGetDeviceIDs(platform, all, num_entries, the ids wanted, the count wanted
 * or not). */
bool query_devices(ClSession &session, std::uint64_t platform, std::uint32_t entries, bool count_wanted)
{
    std::uint8_t bytes[frame_bytes];
    WireWriter frame(bytes, sizeof bytes);
    frame.write_pod<std::uint64_t>(platform);
    frame.write_varint(CL_DEVICE_TYPE_ALL);
    frame.write_varint(entries);
    frame.write_pod<std::uint8_t>(1u);
    frame.write_pod<std::uint8_t>(count_wanted ? 1u : 0u);
    return session.run(av::clproto::kCmd_clGetDeviceIDs, frame, bytes);
}

/* clGetDeviceInfo(device, CL_DEVICE_VENDOR, `room` bytes wanted, the size
 * wanted). */
bool query_vendor(ClSession &session, std::uint64_t device, std::uint32_t room)
{
    std::uint8_t bytes[frame_bytes];
    WireWriter frame(bytes, sizeof bytes);
    frame.write_pod<std::uint64_t>(device);
    frame.write_varint(CL_DEVICE_VENDOR);
    frame.write_varint(room);
    frame.write_pod<std::uint8_t>(1u);
    frame.write_pod<std::uint8_t>(1u);
    return session.run(av::clproto::kCmd_clGetDeviceInfo, frame, bytes);
}

}  // namespace

/* T033: an ICD that fails an ID query may leave anything in the array - a host
 * pointer included - and a count to match. The guest that wanted the ids and
 * not the count used to be sent num_entries of them regardless, and the host
 * to remember every one as an object it made. */
TEST(opencl_dispatch, a_failed_id_query_answers_no_ids_whatever_the_icd_left)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  RecordingClDispatch &dispatch = session.dispatch();
  const std::uint32_t held = dispatch.handles().count();
  for (std::uint32_t pass = 0u; pass < 2u; pass++) {
      const bool count_wanted = pass == 1u;
      dispatch.answer_next_with(CL_INVALID_VALUE, 4u);
      CHECK(query_devices(session, platform, 4u, count_wanted));
      WireReader reply = session.answer();
      CHECK_EQ(answered_result(reply), CL_INVALID_VALUE);
      std::uint64_t sent = ~std::uint64_t{0};
      CHECK(reply.read_varint(sent));
      CHECK_EQ(sent, 0u);
      if (count_wanted) {
          std::uint64_t total = ~std::uint64_t{0};
          CHECK(reply.read_varint(total));
          CHECK_EQ(total, 0u);
      }
      CHECK_EQ(reply.remaining(), 0u);
  }
  { std::uint8_t bytes[frame_bytes];
    WireWriter frame(bytes, sizeof bytes);
    frame.write_varint(4u);
    frame.write_pod<std::uint8_t>(1u);
    frame.write_pod<std::uint8_t>(0u);
    dispatch.answer_next_with(CL_OUT_OF_HOST_MEMORY, 4u);
    CHECK(session.run(av::clproto::kCmd_clGetPlatformIDs, frame, bytes));
    WireReader reply = session.answer();
    CHECK_EQ(answered_result(reply), CL_OUT_OF_HOST_MEMORY);
    std::uint64_t sent = ~std::uint64_t{0};
    CHECK(reply.read_varint(sent));
    CHECK_EQ(sent, 0u);
    CHECK_EQ(reply.remaining(), 0u); }
  CHECK_EQ(dispatch.handles().count(), held);
  CHECK_EQ(dispatch.refused(), 0u);
  CHECK(dispatch.handles().invariants_hold()); }

/* A query that worked answers as many ids as the ICD said it wrote, not as
 * many as there was room for: the slots past them are whatever the ICD left. */
TEST(opencl_dispatch, an_id_query_answers_only_the_ids_the_icd_reported)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  RecordingClDispatch &dispatch = session.dispatch();
  const std::uint32_t held = dispatch.handles().count();
  dispatch.answer_next_with(CL_SUCCESS, 1u);
  CHECK(query_devices(session, platform, 4u, false));
  WireReader reply = session.answer();
  CHECK_EQ(answered_result(reply), CL_SUCCESS);
  std::uint64_t sent = 0u;
  CHECK(reply.read_varint(sent));
  CHECK_EQ(sent, 1u);
  std::uint64_t listed = 0u;
  CHECK(reply.read_pod(listed));
  CHECK(dispatch.handle_for(HandleKind::device, listed) != nullptr);
  CHECK_EQ(reply.remaining(), 0u);
  CHECK_EQ(dispatch.handles().count(), held + 1u);
  /* An ICD reporting more than the room it was given is held to the room. */
  dispatch.answer_next_with(CL_SUCCESS, 9u);
  CHECK(query_devices(session, platform, 2u, true));
  WireReader bounded = session.answer();
  CHECK_EQ(answered_result(bounded), CL_SUCCESS);
  CHECK(bounded.read_varint(sent));
  CHECK_EQ(sent, 2u);
  CHECK(dispatch.handles().invariants_hold()); }

/* An info query answers the bytes the ICD reported writing, and a failed one
 * none, and no size. */
TEST(opencl_dispatch, an_info_query_answers_only_the_bytes_the_icd_reported)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  RecordingClDispatch &dispatch = session.dispatch();
  dispatch.answer_next_with(CL_SUCCESS, 5u);
  CHECK(query_vendor(session, device, 16u));
  { WireReader reply = session.answer();
    CHECK_EQ(answered_result(reply), CL_SUCCESS);
    std::uint64_t sent = 0u;
    CHECK(reply.read_varint(sent));
    CHECK_EQ(sent, 5u);
    std::uint8_t value[5] = {};
    CHECK(reply.read(value, sizeof value));
    for (const std::uint8_t byte : value) CHECK_EQ(byte, RecordingClDispatch::answered);
    std::uint64_t size = 0u;
    CHECK(reply.read_varint(size));
    CHECK_EQ(size, 5u);
    CHECK_EQ(reply.remaining(), 0u); }
  dispatch.answer_next_with(CL_INVALID_VALUE, 16u);
  CHECK(query_vendor(session, device, 16u));
  { WireReader reply = session.answer();
    CHECK_EQ(answered_result(reply), CL_INVALID_VALUE);
    std::uint64_t sent = ~std::uint64_t{0};
    std::uint64_t size = ~std::uint64_t{0};
    CHECK(reply.read_varint(sent));
    CHECK(reply.read_varint(size));
    CHECK_EQ(sent, 0u);
    CHECK_EQ(size, 0u);
    CHECK_EQ(reply.remaining(), 0u); } }

/* A failed read sends back no bytes, and no event: what the ICD left there was
 * never an event, and naming it would make it one the guest may wait on. */
TEST(opencl_dispatch, a_failed_read_answers_no_bytes_and_no_event)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  const std::uint64_t buffer = session.make_buffer(context, 64u);
  RecordingClDispatch &dispatch = session.dispatch();
  const std::uint32_t held = dispatch.handles().count();
  dispatch.answer_next_with(CL_MEM_OBJECT_ALLOCATION_FAILURE, 0u);
  std::uint8_t bytes[frame_bytes];
  WireWriter frame(bytes, sizeof bytes);
  frame.write_pod<std::uint64_t>(queue);
  frame.write_pod<std::uint64_t>(buffer);
  frame.write_varint(CL_TRUE);
  frame.write_varint(0u);
  frame.write_varint(16u);
  frame.write_pod<std::uint8_t>(1u);
  frame.write_varint(16u);
  frame.write_varint(0u);
  frame.write_pod<std::uint8_t>(0u);
  frame.write_pod<std::uint8_t>(1u);
  CHECK(session.run(av::clproto::kCmd_clEnqueueReadBuffer, frame, bytes));
  WireReader reply = session.answer();
  CHECK_EQ(answered_result(reply), CL_MEM_OBJECT_ALLOCATION_FAILURE);
  std::uint64_t sent = ~std::uint64_t{0};
  CHECK(reply.read_varint(sent));
  CHECK_EQ(sent, 0u);
  std::uint64_t event = ~std::uint64_t{0};
  CHECK(reply.read_pod(event));
  CHECK_EQ(event, 0u);
  CHECK_EQ(reply.remaining(), 0u);
  CHECK_EQ(dispatch.handles().count(), held); }

/* ---- addresses (feature 052) ---------------------------------------------
 *
 * The ten commands that name shared virtual memory or a mapped range. What is
 * asserted is the rule the handles already follow, applied to an address: only
 * one the host allocated resolves, and the extent the command will touch is
 * part of the question - so an address one byte inside a live allocation with
 * a size that runs past its end never reaches the ICD.
 */

TEST(opencl_dispatch, an_svm_address_the_host_never_allocated_is_refused)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  const std::uint32_t before = session.dispatch().calls();

  std::uint8_t bytes[frame_bytes];
  WireWriter frame(bytes, sizeof bytes);
  frame.write_pod<std::uint64_t>(context);
  /* clSVMFree(context, svm_pointer): an address the guest made up. */
  frame.write_pod<std::uint64_t>(0x7F0000001000u);
  CHECK(!session.run(av::clproto::kCmd_clSVMFree, frame, bytes));
  CHECK_EQ(session.dispatch().calls(), before);
  /* Refused at the decoder, so no call was handed the address at all: a
   * stranger is one that got through. */
  CHECK_EQ(session.dispatch().strangers(), 0u); }

TEST(opencl_dispatch, an_svm_address_the_host_allocated_resolves)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  const std::uint64_t allocated = session.dispatch().make_svm(256u);
  CHECK(allocated != 0u);
  const std::uint32_t before = session.dispatch().calls();
  const std::uint32_t strangers = session.dispatch().strangers();

  std::uint8_t bytes[frame_bytes];
  WireWriter frame(bytes, sizeof bytes);
  frame.write_pod<std::uint64_t>(context);
  frame.write_pod<std::uint64_t>(allocated);
  CHECK(session.run(av::clproto::kCmd_clSVMFree, frame, bytes));
  CHECK_EQ(session.dispatch().calls(), before + 1u);
  CHECK_EQ(session.dispatch().strangers(), strangers); }

TEST(opencl_dispatch, an_svm_extent_that_leaves_its_allocation_is_refused)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  const std::uint64_t allocated = session.dispatch().make_svm(256u);
  CHECK(allocated != 0u);

  /* clEnqueueSVMMap(queue, blocking, flags, svm_ptr, size, events..., event).
   * The address is inside the allocation; the size is not. */
  struct Case { std::uint64_t offset; std::uint64_t size; bool runs; };
  constexpr Case cases[] = {{0u, 256u, true}, {128u, 128u, true}, {255u, 1u, true},
                            {0u, 257u, false}, {128u, 129u, false}, {255u, 2u, false},
                            {256u, 0u, false}};
  for (const Case &item : cases) {
      const std::uint32_t before = session.dispatch().calls();
      std::uint8_t bytes[frame_bytes];
      WireWriter frame(bytes, sizeof bytes);
      frame.write_pod<std::uint64_t>(queue);
      frame.write_varint(1u);                       /* blocking_map */
      frame.write_varint(1u);                       /* map_flags */
      frame.write_pod<std::uint64_t>(allocated + item.offset);
      frame.write_varint(item.size);
      frame.write_varint(0u);                       /* num_events_in_wait_list */
      frame.write_pod<std::uint8_t>(0u);            /* event_wait_list */
      frame.write_pod<std::uint8_t>(0u);            /* event */
      CHECK_EQ(session.run(av::clproto::kCmd_clEnqueueSVMMap, frame, bytes), item.runs);
      CHECK_EQ(session.dispatch().calls(), before + (item.runs ? 1u : 0u));
  } }

TEST(opencl_dispatch, a_copy_judges_both_of_its_addresses)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  const std::uint64_t source = session.dispatch().make_svm(128u);
  const std::uint64_t destination = session.dispatch().make_svm(128u);
  CHECK(source != 0u);
  CHECK(destination != 0u);

  /* clEnqueueSVMMemcpy(queue, blocking, dst, src, size, events..., event). */
  struct Case { std::uint64_t destination_offset; std::uint64_t source_offset; std::uint64_t size; bool runs; };
  const Case cases[] = {{0u, 0u, 128u, true}, {0u, 0u, 129u, false}, {1u, 0u, 128u, false},
                        {0u, 1u, 128u, false}, {64u, 64u, 64u, true}};
  for (const Case &item : cases) {
      const std::uint32_t before = session.dispatch().calls();
      std::uint8_t bytes[frame_bytes];
      WireWriter frame(bytes, sizeof bytes);
      frame.write_pod<std::uint64_t>(queue);
      frame.write_varint(1u);
      frame.write_pod<std::uint64_t>(destination + item.destination_offset);
      frame.write_pod<std::uint64_t>(source + item.source_offset);
      frame.write_varint(item.size);
      frame.write_varint(0u);
      frame.write_pod<std::uint8_t>(0u);
      frame.write_pod<std::uint8_t>(0u);
      CHECK_EQ(session.run(av::clproto::kCmd_clEnqueueSVMMemcpy, frame, bytes), item.runs);
      CHECK_EQ(session.dispatch().calls(), before + (item.runs ? 1u : 0u));
  } }

TEST(opencl_dispatch, a_kernel_argument_may_point_into_the_middle_of_an_allocation)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  const std::uint64_t allocated = session.dispatch().make_svm(64u);
  CHECK(allocated != 0u);

  /* clSetKernelArgSVMPointer(kernel, arg_index, arg_value): no extent at all,
   * and an address that need not be the allocation's base. */
  struct Case { std::uint64_t address; bool runs; };
  const Case cases[] = {{allocated, true}, {allocated + 32u, true}, {allocated + 63u, true},
                        {allocated + 64u, false}, {allocated - 1u, false}};
  for (const Case &item : cases) {
      const std::uint32_t before = session.dispatch().calls();
      std::uint8_t bytes[frame_bytes];
      WireWriter frame(bytes, sizeof bytes);
      frame.write_pod<std::uint64_t>(0u);   /* kernel: null is the API's own error */
      frame.write_varint(0u);
      frame.write_pod<std::uint64_t>(item.address);
      CHECK_EQ(session.run(av::clproto::kCmd_clSetKernelArgSVMPointer, frame, bytes), item.runs);
      CHECK_EQ(session.dispatch().calls(), before + (item.runs ? 1u : 0u));
  } }

TEST(opencl_dispatch, a_null_svm_address_is_the_apis_own_error_and_not_a_refusal)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);
  const std::uint32_t before = session.dispatch().calls();
  const std::uint32_t strangers = session.dispatch().strangers();

  /* As for a null handle: the command decodes and the host answers
   * CL_INVALID_VALUE. Refusing the frame would fault the context over an
   * application's mistake. */
  std::uint8_t bytes[frame_bytes];
  WireWriter frame(bytes, sizeof bytes);
  frame.write_pod<std::uint64_t>(0u);
  frame.write_pod<std::uint64_t>(0u);
  CHECK(session.run(av::clproto::kCmd_clSVMFree, frame, bytes));
  CHECK_EQ(session.dispatch().calls(), before + 1u);
  CHECK_EQ(session.dispatch().strangers(), strangers); }

TEST(opencl_dispatch, an_address_returning_command_answers_a_wire_word)
{ ClSession session;
  std::uint64_t platform = 0u, device = 0u, context = 0u, queue = 0u;
  session.make_chain(platform, device, context, queue);

  /* clSVMAlloc(context, flags, size, alignment). The fake allocates nothing,
   * so the answer is zero - and it is eight bytes of answer, not a varint. */
  std::uint8_t bytes[frame_bytes];
  WireWriter frame(bytes, sizeof bytes);
  frame.write_pod<std::uint64_t>(context);
  frame.write_varint(1u);
  frame.write_varint(64u);
  frame.write_varint(0u);
  CHECK(session.run(av::clproto::kCmd_clSVMAlloc, frame, bytes));
  WireReader answer = session.answer();
  std::uint64_t address = 1u;
  CHECK(answer.read_pod(address));
  CHECK_EQ(address, 0u); }
