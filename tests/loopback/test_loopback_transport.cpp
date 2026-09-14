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

#include <cstdint>

using av::GuestConnection;
using av::KmtTransport;
using av::LoopbackKmt;
using av::LoopbackRig;
using av::Status;
using av::SubApi;

namespace {

/* The one adapter LoopbackKmt enumerates. */
constexpr std::uint32_t loopback_adapter = 1u;

}  // namespace

TEST(loopback_transport, a_vulkan_command_crosses_the_transport_and_comes_back)
{ LoopbackRig  rig;
  CHECK(rig.read_capset());
  LoopbackKmt  kmt(rig.platform(), rig.device());
  GuestConnection connection(kmt);
  CHECK_STATUS(connection.open(loopback_adapter), Status::ok);
  KmtTransport transport(connection, SubApi::vulkan, 12u);
  CHECK_STATUS(transport.initialize(), Status::ok);
  CHECK(transport.established());

  VkInstanceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  std::uint64_t instance = 0u;
  const VkResult result = av::vkproto::rvk_vkCreateInstance(transport, &info, nullptr, &instance);

  /* The round trip completed - the device ran the command and answered - rather
   * than faulting the transport. */
  CHECK(result != VK_ERROR_DEVICE_LOST);
  CHECK(rig.device().executor().executed() >= 1u);

  transport.teardown();
  connection.close(); }
