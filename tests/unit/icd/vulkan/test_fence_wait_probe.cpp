#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/transport/mock_transport.hpp"
#include "icd/vulkan/fence_wait_probe.hpp"
#include "proto/generated/vulkan/vk_handle_translation.h"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>
#include <cstring>

using av::MockKmt;
using av::MockTransport;
using av::WireReader;
using av::icd::FenceWaitProbe;

namespace {

void reply_with(MockTransport &wire, VkResult result)
{
    const std::int32_t code = static_cast<std::int32_t>(result);
    std::uint8_t       bytes[sizeof code] = {};
    std::memcpy(bytes, &code, sizeof code);
    wire.set_reply(bytes, sizeof bytes);
}

}

TEST(fence_wait_probe, a_host_that_times_out_leaves_the_wait_open_with_a_timeout_result)
{ MockTransport                 wire;
  MockKmt                       kmt;
  av::vkproto::HandleIdentity   handles;
  const VkFence                 fences[2] = {reinterpret_cast<VkFence>(0x41u), reinterpret_cast<VkFence>(0x42u)};
  FenceWaitProbe                probe(wire, handles, kmt, reinterpret_cast<VkDevice>(0x9u), 2u, fences, VK_TRUE);
  CHECK_EQ(probe.result(), VK_TIMEOUT);
  reply_with(wire, VK_TIMEOUT);
  CHECK(!probe.ask(500u));
  CHECK_EQ(probe.result(), VK_TIMEOUT);
  CHECK_EQ(wire.last_command(), static_cast<std::uint32_t>(av::vkproto::kCmd_vkWaitForFences)); }

TEST(fence_wait_probe, every_fence_crosses_to_the_host_with_no_timeout_and_any_other_answer_ends_the_wait)
{ MockTransport                 wire;
  MockKmt                       kmt;
  av::vkproto::HandleIdentity   handles;
  const VkFence                 fences[2] = {reinterpret_cast<VkFence>(0x41u), reinterpret_cast<VkFence>(0x42u)};
  FenceWaitProbe                probe(wire, handles, kmt, reinterpret_cast<VkDevice>(0x9u), 2u, fences, VK_TRUE);
  reply_with(wire, VK_SUCCESS);
  CHECK(probe.ask(123456u));
  CHECK_EQ(probe.result(), VK_SUCCESS);
  WireReader     sent(wire.last_payload(), wire.last_length());
  std::uint64_t  device = 0u, first = 0u, second = 0u, timeout = 0u;
  std::uint32_t  count = 0u, wait_all = 0u;
  CHECK(sent.read_pod(device));
  CHECK(sent.read_pod(count));
  CHECK(sent.read_pod(first));
  CHECK(sent.read_pod(second));
  CHECK(sent.read_pod(wait_all));
  CHECK(sent.read_pod(timeout));
  CHECK_EQ(device, 0x9u);
  CHECK_EQ(count, 2u);
  CHECK_EQ(first, 0x41u);
  CHECK_EQ(second, 0x42u);
  CHECK_EQ(wait_all, 1u);
  CHECK_EQ(timeout, 0u);
  reply_with(wire, VK_ERROR_DEVICE_LOST);
  CHECK(probe.ask(1u));
  CHECK_EQ(probe.result(), VK_ERROR_DEVICE_LOST); }

TEST(fence_wait_probe, pausing_between_asks_sends_nothing_and_waits_on_the_guest_side_and_a_transport_that_fails_is_device_lost)
{ MockTransport                 wire;
  MockKmt                       kmt;
  av::vkproto::HandleIdentity   handles;
  const VkFence                 fence = reinterpret_cast<VkFence>(0x41u);
  FenceWaitProbe                probe(wire, handles, kmt, reinterpret_cast<VkDevice>(0x9u), 1u, &fence, VK_FALSE);
  reply_with(wire, VK_TIMEOUT);
  CHECK(!probe.ask(10u));
  const std::uint32_t sends = wire.sends();
  probe.pause(10u);
  CHECK_EQ(wire.sends(), sends);
  CHECK_EQ(probe.result(), VK_TIMEOUT);
  wire.fail_next();
  CHECK(probe.ask(10u));
  CHECK_EQ(probe.result(), VK_ERROR_DEVICE_LOST); }

TEST(fence_wait_probe, pausing_spins_for_a_while_and_then_rests_with_the_transport_unlocked)
{ MockTransport                 wire;
  MockKmt                       kmt;
  kmt.set_frequency(1000000u);
  kmt.reads_clock(nullptr, 500u);
  av::vkproto::HandleIdentity   handles;
  const VkFence                 fence = reinterpret_cast<VkFence>(0x41u);
  FenceWaitProbe                probe(wire, handles, kmt, reinterpret_cast<VkDevice>(0x9u), 1u, &fence, VK_TRUE);
  probe.pause(500000000u);
  probe.pause(500000000u);
  CHECK_EQ(kmt.sleeps(), 0u);
  probe.pause(500000000u);
  CHECK_EQ(kmt.sleeps(), 1u);
  CHECK_EQ(wire.sends(), 0u); }
