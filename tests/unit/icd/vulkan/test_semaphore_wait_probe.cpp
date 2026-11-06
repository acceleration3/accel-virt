#include "harness/framework/test_registry.hpp"
#include "harness/transport/mock_transport.hpp"
#include "icd/vulkan/semaphore_wait_probe.hpp"
#include "proto/generated/vulkan/vk_handle_translation.h"
#include "proto/generated/vulkan/vk_protocol_ids.h"

#include <cstdint>
#include <cstring>

using av::MockTransport;
using av::icd::SemaphoreWaitProbe;

namespace {

void reply_with(MockTransport &wire, VkResult result)
{
    const std::int32_t code = static_cast<std::int32_t>(result);
    std::uint8_t       bytes[sizeof code] = {};
    std::memcpy(bytes, &code, sizeof code);
    wire.set_reply(bytes, sizeof bytes);
}

std::uint64_t trailing_timeout(const MockTransport &wire)
{
    std::uint64_t timeout = 0u;
    std::memcpy(&timeout, wire.last_payload() + wire.last_length() - sizeof timeout, sizeof timeout);
    return timeout;
}

}

TEST(semaphore_wait_probe, a_wait_times_out_until_the_host_says_the_semaphores_reached_their_values)
{ MockTransport                 wire;
  av::vkproto::HandleIdentity   handles;
  const VkSemaphore             semaphore = reinterpret_cast<VkSemaphore>(0x51u);
  const std::uint64_t           value = 7u;
  VkSemaphoreWaitInfo           info = {};
  info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
  info.semaphoreCount = 1u;
  info.pSemaphores = &semaphore;
  info.pValues = &value;
  SemaphoreWaitProbe probe(wire, handles, reinterpret_cast<VkDevice>(0x9u), &info);
  CHECK_EQ(probe.result(), VK_TIMEOUT);
  reply_with(wire, VK_TIMEOUT);
  CHECK(!probe.ask(2000u));
  CHECK_EQ(probe.result(), VK_TIMEOUT);
  CHECK_EQ(wire.last_command(), static_cast<std::uint32_t>(av::vkproto::kCmd_vkWaitSemaphores));
  CHECK_EQ(trailing_timeout(wire), 2000u);
  probe.pause(2000u);
  CHECK_EQ(probe.result(), VK_TIMEOUT);
  reply_with(wire, VK_SUCCESS);
  CHECK(probe.ask(3000u));
  CHECK_EQ(probe.result(), VK_SUCCESS);
  CHECK_EQ(trailing_timeout(wire), 3000u); }

TEST(semaphore_wait_probe, a_lost_device_ends_the_wait_with_that_result)
{ MockTransport                 wire;
  av::vkproto::HandleIdentity   handles;
  const VkSemaphore             semaphore = reinterpret_cast<VkSemaphore>(0x51u);
  const std::uint64_t           value = 1u;
  VkSemaphoreWaitInfo           info = {};
  info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
  info.semaphoreCount = 1u;
  info.pSemaphores = &semaphore;
  info.pValues = &value;
  SemaphoreWaitProbe probe(wire, handles, reinterpret_cast<VkDevice>(0x9u), &info);
  wire.fail_next();
  CHECK(probe.ask(10u));
  CHECK_EQ(probe.result(), VK_ERROR_DEVICE_LOST); }
