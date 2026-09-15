/* One command at a time through a shared transport: the lock is taken when the
 * writer is lent and let go once the command is sent, whichever send it is and
 * whether it failed; what is sent and answered is the inner transport's. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/transport/mock_transport.hpp"
#include "icd/vulkan/locked_transport.hpp"
#include "proto/client_transport.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

using av::ClientTransport;
using av::LockTrace;
using av::MockTransport;
using av::RecordingThreadLock;
using av::TransportResult;
using av::WireReader;
using av::WireWriter;
using av::icd::LockedTransport;

namespace {

constexpr std::uint32_t command = 0x1234u;

}  // namespace

TEST(locked_transport, holds_the_lock_from_the_writer_until_an_async_send_returns)
{ MockTransport inner;
  LockTrace trace;
  RecordingThreadLock lock(trace, 'S');
  LockedTransport transport(inner, lock);
  WireWriter writer = transport.request_writer();
  CHECK(lock.held());
  writer.write_pod<std::uint32_t>(0xABCDu);
  CHECK(transport.send_async(command, writer) == TransportResult::ok);
  CHECK(!lock.held());
  CHECK_EQ(inner.sends(), 1u);
  CHECK_EQ(inner.last_command(), command);
  CHECK_EQ(inner.last_length(), 4u);
  CHECK(std::strcmp(trace.text(), "Ss") == 0); }

TEST(locked_transport, holds_the_lock_until_a_sync_send_returns_and_answers_the_reply)
{ MockTransport inner;
  const std::uint8_t answer[] = {1u, 2u, 3u};
  inner.set_reply(answer, sizeof answer);
  LockTrace trace;
  RecordingThreadLock lock(trace, 'S');
  LockedTransport transport(inner, lock);
  WireWriter writer = transport.request_writer();
  WireReader reply(nullptr, 0u);
  CHECK(lock.held());
  CHECK(transport.send_sync(command, writer, reply) == TransportResult::ok);
  CHECK(!lock.held());
  CHECK_EQ(reply.remaining(), 3u);
  CHECK_EQ(reply.read_u8(), 1u);
  CHECK_EQ(lock.acquisitions(), 1u); }

TEST(locked_transport, a_failed_send_still_lets_the_lock_go_and_says_it_failed)
{ MockTransport inner;
  LockTrace trace;
  RecordingThreadLock lock(trace, 'S');
  LockedTransport transport(inner, lock);
  inner.fail_next();
  WireWriter writer = transport.request_writer();
  CHECK(transport.send_async(command, writer) == TransportResult::failed);
  CHECK(!lock.held());
  inner.fail_next();
  writer = transport.request_writer();
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_sync(command, writer, reply) == TransportResult::failed);
  CHECK(!lock.held());
  CHECK_EQ(inner.sends(), 0u);
  CHECK(std::strcmp(trace.text(), "SsSs") == 0); }

TEST(locked_transport, a_generated_command_takes_the_lock_once)
{ MockTransport inner;
  std::uint8_t answer[12] = {};
  constexpr std::uint64_t host_instance = 0x1100u;
  std::memcpy(answer + 4, &host_instance, sizeof host_instance);
  inner.set_reply(answer, sizeof answer);
  LockTrace trace;
  RecordingThreadLock lock(trace, 'S');
  LockedTransport transport(inner, lock);
  ClientTransport &seam = transport;
  /* vkCreateInstance names no dispatchable handle, so nothing here needs a root
   * to unwrap one. */
  VkInstanceCreateInfo create = {};
  create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  std::uint64_t instance = 0u;
  const VkResult result = av::vkproto::rvk_vkCreateInstance(seam, &create, nullptr, &instance);
  CHECK_EQ(result, VK_SUCCESS);
  CHECK_EQ(instance, host_instance);
  CHECK_EQ(inner.last_command(), static_cast<std::uint32_t>(av::vkproto::kCmd_vkCreateInstance));
  CHECK(std::strcmp(trace.text(), "Ss") == 0); }

TEST(locked_transport, lends_the_inner_transports_arena_and_blobs)
{ MockTransport inner;
  LockTrace trace;
  RecordingThreadLock lock(trace, 'S');
  LockedTransport transport(inner, lock);
  CHECK(&transport.decode_arena() == &inner.decode_arena());
  CHECK(&transport.blobs() == &inner.blobs());
  CHECK(!lock.held()); }
