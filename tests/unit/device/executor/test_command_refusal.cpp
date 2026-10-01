/* What a Direct3D session does with a command it refused (CommandRefusal):
 * it removes the device that sent it, and fails the session only when there
 * is no device to blame. */
#include "device/executor/command_refusal.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::CommandRefusal;

namespace {

constexpr std::uint32_t device = 7u;

}  // namespace

TEST(command_refusal, a_command_with_no_device_to_blame_fails_the_session)
{ CommandRefusal refusal("d3d9");
  CHECK(!refusal.removes("QueryCaps9", 0u, false)); }

/* The first refusal on a device and every one after remove it; only the
 * first says why. */
TEST(command_refusal, a_command_from_a_device_removes_that_device)
{ CommandRefusal refusal("d3d11");
  CHECK(refusal.removes("ReadSubresource", device, false));
  CHECK(refusal.removes("Draw", device, true)); }
