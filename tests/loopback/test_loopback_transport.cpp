/* L4, the MVP round trip (SC-001): a batch encoded by the generated Vulkan
 * client crosses KmtTransport, over a LoopbackKmt, to the device core, which
 * runs it through the executor and answers; the client reads the reply back.
 * One process, both sides, no real hardware. */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/loopback/loopback_kmt.hpp"
#include "harness/loopback/loopback_rig.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "proto/context/sub_api.hpp"
#include "vk_dispatch_client.h"

#include <cstddef>
#include <cstdint>

using av::GuestConnection;
using av::KmtTransport;
using av::LoopbackKmt;
using av::LoopbackRig;
using av::Status;
using av::SubApi;

namespace {

/* The av suite's transport: 1 KiB commands and a page of reply (feature 044). */
constexpr std::size_t   staging_bytes = 1024u;
constexpr std::uint32_t reply_bytes = 4096u;

/* The one adapter LoopbackKmt enumerates. */
constexpr std::uint32_t loopback_adapter = 1u;

}  // namespace

TEST(loopback_transport, a_vulkan_command_crosses_the_transport_and_comes_back)
{ LoopbackRig  rig;
  CHECK(rig.read_capset());
  LoopbackKmt  kmt(rig.platform(), rig.device());
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(loopback_adapter), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, SubApi::vulkan, 12u, reply_bytes, transport_staging, sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CHECK(transport.established());

  VkInstanceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  std::uint64_t instance = 0u;
  const VkResult result = av::vkproto::av_vkCreateInstance(transport, &info, nullptr, &instance);

  /* The round trip completed - the device ran the command and answered - rather
   * than faulting the transport. */
  CHECK(result != VK_ERROR_DEVICE_LOST);
  CHECK(rig.device().executor().executed() >= 1u);

  transport.teardown();
  connection.close(); }

namespace {

/* A Vulkan id the mock executor echoes: its payload comes back as its answer. */
constexpr std::uint32_t echoed = 0x00010042u;

/* Bytes no two offsets of a long payload share in a row, so a piece gathered
 * twice, dropped or out of order shows. */
std::uint8_t pattern_at(std::size_t offset)
{
    return static_cast<std::uint8_t>((offset * 131u) ^ (offset >> 9u));
}

}  // namespace

/* Docs/protocol.md, "Long commands": a command many times the staging, the
 * ring and the reply slot - what a program's source of several hundred
 * kilobytes is to the OpenCL ICD - crosses in pieces, is gathered and run
 * whole on the host, and its answer, as long, comes back in parts. Both
 * directions are bounded by nothing but memory, and the context is as
 * healthy afterwards as before. */
TEST(loopback_transport, a_command_and_an_answer_longer_than_every_room_cross_whole)
{ LoopbackRig  rig;
  CHECK(rig.read_capset());
  LoopbackKmt  kmt(rig.platform(), rig.device());
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(loopback_adapter), Status::ok);
  std::uint8_t transport_staging[staging_bytes] = {};
  KmtTransport transport(connection, rig.platform(), SubApi::vulkan, 12u, reply_bytes, transport_staging,
                         sizeof transport_staging);
  CHECK_STATUS(transport.initialize(), Status::ok);

  constexpr std::size_t long_bytes = 300u * 1024u + 17u;
  for (std::uint32_t round = 0u; round < 2u; round++) {
      av::WireWriter writer = transport.request_writer();
      for (std::size_t offset = 0u; offset < long_bytes; offset++) writer.write_u8(pattern_at(offset));
      CHECK_STATUS(writer.status(), Status::ok);
      CHECK(writer.spilled());
      av::WireReader reply(nullptr, 0u);
      CHECK(transport.send_sync(echoed, writer, reply) == av::TransportResult::ok);
      CHECK_EQ(reply.remaining(), long_bytes);
      std::size_t wrong = 0u;
      for (std::size_t offset = 0u; offset < long_bytes; offset++)
          if (reply.read_u8() != pattern_at(offset)) wrong++;
      CHECK_EQ(wrong, 0u);
      transport.end_call();
      CHECK(!transport.faulted());
  }
  /* The executor ran each long command once, with every byte, and the
   * transport's own commands - pieces, parts - never reached it. */
  CHECK_EQ(rig.device().executor().executed(), 2u);
  CHECK_EQ(rig.device().executor().size_at(0u), static_cast<std::uint32_t>(long_bytes));
  CHECK_EQ(rig.device().executor().size_at(1u), static_cast<std::uint32_t>(long_bytes));
  /* And a short command after them is a short command. */
  av::WireWriter small = transport.request_writer();
  small.write_u32(0xA5A5A5A5u);
  av::WireReader echo(nullptr, 0u);
  CHECK(transport.send_sync(echoed, small, echo) == av::TransportResult::ok);
  CHECK_EQ(echo.remaining(), 4u);
  CHECK_EQ(echo.read_u32(), 0xA5A5A5A5u);
  transport.end_call();
  CHECK(transport.invariants_hold());

  transport.teardown();
  connection.close(); }
