#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/transport/mock_transport.hpp"
#include "icd/vulkan/query_wait_probe.hpp"
#include "proto/generated/vulkan/vk_handle_translation.h"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>
#include <cstring>

using av::MockKmt;
using av::MockTransport;
using av::WireReader;
using av::icd::QueryWaitProbe;

namespace {

void reply_with(MockTransport &wire, VkResult result, const std::uint8_t *data, std::size_t size)
{
    std::uint8_t       bytes[64] = {};
    const std::int32_t code = static_cast<std::int32_t>(result);
    std::memcpy(bytes, &code, sizeof code);
    if (size != 0u) std::memcpy(bytes + sizeof code, data, size);
    wire.set_reply(bytes, sizeof code + size);
}

}

TEST(query_wait_probe, a_wait_asks_without_the_wait_bit_and_ends_once_the_results_are_not_not_ready)
{ MockTransport                wire;
  MockKmt                      kmt;
  av::vkproto::HandleIdentity  handles;
  std::uint64_t                results[2] = {};
  QueryWaitProbe probe(wire, handles, kmt, reinterpret_cast<VkDevice>(0x9u), reinterpret_cast<VkQueryPool>(0x61u), 3u, 2u, sizeof results,
                       results, sizeof results[0],
                       VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
  CHECK_EQ(probe.result(), VK_NOT_READY);
  const std::uint8_t pending[sizeof results] = {};
  reply_with(wire, VK_NOT_READY, pending, sizeof pending);
  CHECK(!probe.ask(0u));
  CHECK_EQ(probe.result(), VK_NOT_READY);
  CHECK_EQ(wire.last_command(), static_cast<std::uint32_t>(av::vkproto::kCmd_vkGetQueryPoolResults));
  WireReader     sent(wire.last_payload(), wire.last_length());
  std::uint64_t  device = 0u, pool = 0u, size = 0u, stride = 0u;
  std::uint32_t  first = 0u, count = 0u, flags = 0u;
  CHECK(sent.read_pod(device));
  CHECK(sent.read_pod(pool));
  CHECK(sent.read_pod(first));
  CHECK(sent.read_pod(count));
  CHECK(sent.read_pod(size));
  CHECK(sent.read_pod(stride));
  CHECK(sent.read_pod(flags));
  CHECK_EQ(pool, 0x61u);
  CHECK_EQ(first, 3u);
  CHECK_EQ(count, 2u);
  CHECK_EQ(size, sizeof results);
  CHECK_EQ(flags, static_cast<std::uint32_t>(VK_QUERY_RESULT_64_BIT));
  const std::uint64_t answered[2] = {11u, 22u};
  reply_with(wire, VK_SUCCESS, reinterpret_cast<const std::uint8_t *>(answered), sizeof answered);
  CHECK(probe.ask(0u));
  CHECK_EQ(probe.result(), VK_SUCCESS);
  CHECK_EQ(results[0], 11u);
  CHECK_EQ(results[1], 22u); }

TEST(query_wait_probe, pausing_spins_for_a_while_and_then_rests)
{ MockTransport                wire;
  MockKmt                      kmt;
  kmt.set_frequency(1000000u);
  kmt.reads_clock(nullptr, 500u);
  av::vkproto::HandleIdentity  handles;
  QueryWaitProbe probe(wire, handles, kmt, reinterpret_cast<VkDevice>(0x9u), reinterpret_cast<VkQueryPool>(0x61u), 0u, 1u, 0u, nullptr, 8u, 0u);
  probe.pause(0u);
  probe.pause(0u);
  CHECK_EQ(kmt.sleeps(), 0u);
  probe.pause(0u);
  CHECK_EQ(kmt.sleeps(), 1u);
  probe.pause(0u);
  CHECK_EQ(kmt.sleeps(), 2u); }

TEST(query_wait_probe, a_transport_that_fails_ends_the_wait_with_a_lost_device)
{ MockTransport                wire;
  MockKmt                      kmt;
  av::vkproto::HandleIdentity  handles;
  QueryWaitProbe probe(wire, handles, kmt, reinterpret_cast<VkDevice>(0x9u), reinterpret_cast<VkQueryPool>(0x61u), 0u, 1u, 0u, nullptr, 8u, 0u);
  wire.fail_next();
  CHECK(probe.ask(0u));
  CHECK_EQ(probe.result(), VK_ERROR_DEVICE_LOST); }
