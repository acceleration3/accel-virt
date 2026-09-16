/* The generated Vulkan and OpenCL clients drive a real command through the
 * ClientTransport and read the answer back: proof the emitter's output compiles
 * and links against the interface (feature 043). The transport is the mock both
 * ends of the contract share; the device's answer is scripted. */
#include "cl_dispatch_client.h"
#include "cl_protocol_ids.h"
#include "harness/framework/test_registry.hpp"
#include "harness/transport/mock_transport.hpp"
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
  const VkResult result = av::vkproto::rvk_vkCreateInstance(transport, &info, nullptr, &instance);
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
  const cl_int  result = av::clproto::rvk_clGetPlatformIDs(transport, 4u, platforms, &count);
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
  CHECK(av::vkproto::rvk_vkCreateInstance(transport, &info, nullptr, &instance) == VK_ERROR_DEVICE_LOST);
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
  CHECK(av::vkproto::rvk_vkCreateInstance(transport, &info, nullptr, &instance) == VK_ERROR_DEVICE_LOST);
  CHECK_EQ(transport.end_calls(), 1u);
  transport.set_reply(reply, 0u);
  cl_uint count = 0u;
  CHECK(av::clproto::rvk_clGetPlatformIDs(transport, 0u, nullptr, &count) != CL_SUCCESS);
  CHECK_EQ(transport.end_calls(), 2u); }

/* A recorded command answers nothing, so it opens no call to end. */
TEST(generated_clients, an_async_command_ends_no_call)
{ MockTransport transport;
  av::vkproto::rvk_vkCmdDraw(transport, nullptr, 3u, 1u, 0u, 0u);
  CHECK_EQ(transport.sends(), 1u);
  CHECK_EQ(transport.end_calls(), 0u); }
