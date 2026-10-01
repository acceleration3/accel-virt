/* What a Direct3D session does with a command it refused (CommandRefusal):
 * one whose guest waits on the answer fails the session, and so does one
 * with no device to blame; any other removes the device that sent it. */
#include "device/executor/command_refusal.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

using av::CommandRefusal;
using av::WireWriter;

namespace {

constexpr std::uint32_t device = 7u;

}  // namespace

/* The decoder writes an answer only after the call succeeded: a refused
 * command the guest waits on has none, whatever device sent it. */
TEST(command_refusal, a_command_the_guest_waits_on_fails_the_session)
{ CommandRefusal refusal("d3d11");
  std::uint8_t   bytes[16] = {};
  const WireWriter awaited(bytes, sizeof bytes);
  CHECK(!refusal.removes("Finish11", awaited, device, false));
  CHECK(!refusal.removes("Finish11", awaited, device, true));
  CHECK(!refusal.removes("Finish11", awaited, 0u, false)); }

TEST(command_refusal, a_command_with_no_device_to_blame_fails_the_session)
{ CommandRefusal   refusal("d3d9");
  const WireWriter unanswered(nullptr, 0u);
  CHECK(!refusal.removes("DrawPrimitive9", unanswered, 0u, false)); }

/* Any other is dropped and removes its device - the first time and every
 * time after, which only the first says. */
TEST(command_refusal, any_other_removes_the_device_that_sent_it)
{ CommandRefusal   refusal("d3d12");
  const WireWriter unanswered(nullptr, 0u);
  CHECK(refusal.removes("ExecuteCommandLists12", unanswered, device, false));
  CHECK(refusal.removes("ExecuteCommandLists12", unanswered, device, true)); }
