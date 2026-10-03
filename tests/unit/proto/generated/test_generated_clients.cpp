/* The generated Vulkan and OpenCL clients drive a real command through the
 * ClientTransport and read the answer back: proof the emitter's output compiles
 * and links against the interface (feature 043). The transport is the mock both
 * ends of the contract share; the device's answer is scripted. */
#include "cl_dispatch_client.h"
#include "cl_protocol_ids.h"
#include "harness/framework/test_registry.hpp"
#include "harness/transport/mock_transport.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"
#include "vk_dispatch_client.h"
#include "vk_protocol_ids.h"

#include <cstdint>

using av::MockTransport;
using av::WireWriter;

TEST(generated_clients, the_vulkan_client_creates_an_instance_and_reads_it_back)
{ MockTransport transport;
  /* The device answers VK_SUCCESS and the instance handle it minted. */
  std::uint8_t reply[64] = {};
  WireWriter answer(reply, sizeof reply);
  answer.write_pod<std::int32_t>(0);              /* VK_SUCCESS */
  answer.write_pod<std::uint64_t>(0xABCDEFull);   /* the instance */
  transport.set_reply(reply, answer.offset());

  VkInstanceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  std::uint64_t instance = 0u;
  const VkResult result = av::vkproto::av_vkCreateInstance(transport, &info, nullptr, &instance);
  CHECK(result == VK_SUCCESS);
  CHECK_EQ(instance, 0xABCDEFull);
  CHECK_EQ(transport.sends(), 1u);
  CHECK_EQ(transport.last_command(), av::vkproto::kCmd_vkCreateInstance);
  CHECK_EQ(transport.end_calls(), 1u); }

TEST(generated_clients, the_opencl_client_enumerates_platforms)
{ MockTransport transport;
  std::uint8_t reply[64] = {};
  WireWriter answer(reply, sizeof reply);
  answer.write_varint(0);                          /* CL_SUCCESS */
  answer.write_varint(1);                          /* one platform follows */
  answer.write_pod<std::uint64_t>(0x1111ull);      /* the platform id */
  answer.write_varint(2);                          /* num_platforms */
  transport.set_reply(reply, answer.offset());

  std::uint64_t platforms[4] = {};
  cl_uint       count = 0u;
  const cl_int  result = av::clproto::av_clGetPlatformIDs(transport, 4u, platforms, &count);
  CHECK(result == CL_SUCCESS);
  CHECK_EQ(platforms[0], 0x1111ull);
  CHECK_EQ(count, 2u);
  CHECK_EQ(transport.last_command(), av::clproto::kCmd_clGetPlatformIDs);
  CHECK_EQ(transport.end_calls(), 1u); }

/* A transport that fails the send gives the client the API's own error. */
TEST(generated_clients, a_failed_send_returns_the_apis_error)
{ MockTransport transport;
  transport.fail_next();
  VkInstanceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  std::uint64_t instance = 0u;
  CHECK(av::vkproto::av_vkCreateInstance(transport, &info, nullptr, &instance) == VK_ERROR_DEVICE_LOST);
  CHECK_EQ(transport.end_calls(), 1u); }

/* A reply cut short is a failed call, and the call still ends once: the
 * transport learns the decode is over on every return path. */
TEST(generated_clients, a_short_reply_ends_the_call_once)
{ MockTransport transport;
  const std::uint8_t reply[2] = {};
  transport.set_reply(reply, sizeof reply);
  VkInstanceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  std::uint64_t instance = 0u;
  CHECK(av::vkproto::av_vkCreateInstance(transport, &info, nullptr, &instance) == VK_ERROR_DEVICE_LOST);
  CHECK_EQ(transport.end_calls(), 1u);
  transport.set_reply(reply, 0u);
  cl_uint count = 0u;
  CHECK(av::clproto::av_clGetPlatformIDs(transport, 0u, nullptr, &count) != CL_SUCCESS);
  CHECK_EQ(transport.end_calls(), 2u); }

/* A recorded command answers nothing, so it opens no call to end. */
TEST(generated_clients, an_async_command_ends_no_call)
{ MockTransport transport;
  av::vkproto::av_vkCmdDraw(transport, nullptr, 3u, 1u, 0u, 0u);
  CHECK_EQ(transport.sends(), 1u);
  CHECK_EQ(transport.end_calls(), 0u); }

namespace {

/* What a transfer's frame says about its memory: the presence byte, the size,
 * and the offset when lent - read past the queue, the buffer, the blocking
 * flag and the buffer offset. */
struct TransferFrame {
    std::uint8_t  mode = 0xFFu;
    std::uint64_t size = 0u;
    std::uint64_t at = 0u;
};

TransferFrame transfer_frame(const MockTransport &transport)
{
    av::WireReader frame(transport.last_payload(), transport.last_length());
    std::uint64_t  skipped = 0u;
    TransferFrame  seen;
    CHECK(frame.read_pod(skipped) && frame.read_pod(skipped));
    CHECK(frame.read_varint(skipped) && frame.read_varint(skipped) && frame.read_varint(skipped));
    CHECK(frame.read_pod(seen.mode));
    CHECK(frame.read_varint(seen.size));
    if (seen.mode == 2u) CHECK(frame.read_varint(seen.at));
    return seen;
}

}  // namespace

/* A read into memory inside the lent arena names it by offset, and the answer
 * is a count with no bytes behind it: the host wrote them there. Memory that
 * is the caller's own, or runs past the arena's end, still crosses inline. */
TEST(generated_clients, an_opencl_read_into_the_arena_names_it_and_takes_no_bytes_back)
{ MockTransport transport;
  std::uint8_t  arena[512] = {};
  transport.blobs().lend(arena, 256u);
  std::uint8_t reply[64] = {};
  WireWriter   answer(reply, sizeof reply);
  answer.write_varint(0);
  answer.write_varint(64u);
  transport.set_reply(reply, answer.offset());
  CHECK_EQ(av::clproto::av_clEnqueueReadBuffer(transport, 1u, 2u, CL_TRUE, 0u, 64u, arena + 32, 0u, nullptr,
                                               nullptr),
           CL_SUCCESS);
  const TransferFrame lent = transfer_frame(transport);
  CHECK_EQ(lent.mode, 2u);
  CHECK_EQ(lent.size, 64u);
  CHECK_EQ(lent.at, 32u);
  CHECK_EQ(arena[32], 0u);

  /* Past the arena's end: inline, so the same answer is short of its bytes. */
  CHECK(av::clproto::av_clEnqueueReadBuffer(transport, 1u, 2u, CL_TRUE, 0u, 64u, arena + 224, 0u, nullptr,
                                            nullptr) != CL_SUCCESS);
  CHECK_EQ(transfer_frame(transport).mode, 1u);
  std::uint8_t own[64] = {};
  answer = WireWriter(reply, sizeof reply);
  answer.write_varint(0);
  answer.write_varint(4u);
  answer.write_pod<std::uint32_t>(0x01020304u);
  transport.set_reply(reply, answer.offset());
  CHECK_EQ(av::clproto::av_clEnqueueReadBuffer(transport, 1u, 2u, CL_TRUE, 0u, 4u, own, 0u, nullptr, nullptr),
           CL_SUCCESS);
  CHECK_EQ(transfer_frame(transport).mode, 1u);
  CHECK_EQ(own[0], 0x04u); }

/* A write from inside the arena carries its offset and none of its bytes; one
 * from the caller's own memory carries them all. */
TEST(generated_clients, an_opencl_write_from_the_arena_carries_no_bytes)
{ MockTransport transport;
  std::uint8_t  arena[256] = {};
  transport.blobs().lend(arena, sizeof arena);
  std::uint8_t reply[16] = {};
  WireWriter   answer(reply, sizeof reply);
  answer.write_varint(0);
  transport.set_reply(reply, answer.offset());
  CHECK_EQ(av::clproto::av_clEnqueueWriteBuffer(transport, 1u, 2u, CL_TRUE, 0u, 128u, arena + 128, 0u, nullptr,
                                                nullptr),
           CL_SUCCESS);
  const TransferFrame lent = transfer_frame(transport);
  CHECK_EQ(lent.mode, 2u);
  CHECK_EQ(lent.size, 128u);
  CHECK_EQ(lent.at, 128u);
  const std::size_t by_offset = transport.last_length();
  CHECK(by_offset < 128u);
  std::uint8_t own[128] = {};
  CHECK_EQ(av::clproto::av_clEnqueueWriteBuffer(transport, 1u, 2u, CL_TRUE, 0u, 128u, own, 0u, nullptr, nullptr),
           CL_SUCCESS);
  CHECK_EQ(transfer_frame(transport).mode, 1u);
  /* The bytes themselves in place of the offset, a two-byte varint. */
  CHECK_EQ(transport.last_length(), by_offset - 2u + 128u); }
