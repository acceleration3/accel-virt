/* A context faulted on purpose (feature 045, the probe's lost step): the
 * transport carries a vkCreateInstance whose create-info stops after its
 * structure type, which no host decoder takes, as one synchronous call. Over
 * MockTransport playing the host: a transport that refuses the call faulted,
 * and one that answered it did not. */
#include "core/support/status.hpp"
#include "guest/transport/context_fault.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/transport/mock_transport.hpp"
#include "proto/generated/vulkan/vk_protocol_ids.h"

#include <cstdint>
#include <cstring>

using av::ContextFault;
using av::MockTransport;
using av::Status;

namespace {

constexpr std::uint32_t instance_create_info = 1u;

}  // namespace

/* The host not answering the call is the fault the caller asked for, and
 * the call is ended all the same. */
TEST(context_fault, a_refused_call_is_the_fault)
{ MockTransport transport;
  transport.fail_next();
  ContextFault fault(transport);
  CHECK_STATUS(fault.send(), Status::ok);
  CHECK_EQ(transport.end_calls(), 1u); }

/* A host that answers has not faulted the context: conflict, the call ended
 * once, and what it answered was the frame - a vkCreateInstance holding its
 * create-info's structure type and nothing after it. */
TEST(context_fault, an_answered_frame_is_not_a_fault)
{ MockTransport transport;
  const std::uint8_t reply[4] = {};
  transport.set_reply(reply, sizeof reply);
  ContextFault fault(transport);
  CHECK_STATUS(fault.send(), Status::conflict);
  CHECK_EQ(transport.sends(), 1u);
  CHECK_EQ(transport.end_calls(), 1u);
  CHECK_EQ(transport.last_command(), av::vkproto::kCmd_vkCreateInstance);
  CHECK_EQ(transport.last_length(), sizeof instance_create_info);
  std::uint32_t sent = 0u;
  std::memcpy(&sent, transport.last_payload(), sizeof sent);
  CHECK_EQ(sent, instance_create_info); }

/* An OpenGL context skips a Vulkan command rather than faulting on it, so its
 * fault names a command of its own API and sends it with no arguments at
 * all, which that API's decoder cannot take (feature 050, T040). */
TEST(context_fault, a_named_command_is_sent_with_nothing_after_it)
{ MockTransport transport;
  const std::uint8_t reply[4] = {};
  transport.set_reply(reply, sizeof reply);
  ContextFault fault(transport, 0x00030000u);
  CHECK_STATUS(fault.send(), Status::conflict);
  CHECK_EQ(transport.sends(), 1u);
  CHECK_EQ(transport.end_calls(), 1u);
  CHECK_EQ(transport.last_command(), 0x00030000u);
  CHECK_EQ(transport.last_length(), 0u);
  MockTransport refusing;
  refusing.fail_next();
  ContextFault refused(refusing, 0x00030000u);
  CHECK_STATUS(refused.send(), Status::ok);
  CHECK_EQ(refusing.end_calls(), 1u); }
