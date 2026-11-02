#include "core/log/driver_log.hpp"
#include "executor/d3d/command_refusal.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"

#include <cstdint>
#include <cstring>

using av::CommandRefusal;
using av::DriverLog;
using av::RecordingLog;

namespace {

constexpr std::uint32_t device = 7u;

}

TEST(command_refusal, a_command_with_no_device_to_blame_fails_the_session)
{ RecordingLog   log;
  DriverLog      lines(log, "virtio-gpu-av: d3d9: ");
  CommandRefusal refusal(lines);
  CHECK(!refusal.removes("QueryCaps9", 0u, false));
  CHECK_EQ(log.lines(), 1u);
  CHECK(std::strcmp(log.line(0u),
                    "virtio-gpu-av: d3d9: QueryCaps9 refused with no device selected; the session faults") == 0); }

TEST(command_refusal, a_command_from_a_device_removes_that_device)
{ RecordingLog   log;
  DriverLog      lines(log, "virtio-gpu-av: d3d11: ");
  CommandRefusal refusal(lines);
  CHECK(refusal.removes("ReadSubresource", device, false));
  CHECK(refusal.removes("Draw", device, true));
  CHECK_EQ(log.lines(), 1u);
  CHECK(std::strcmp(log.line(0u), "virtio-gpu-av: d3d11: ReadSubresource refused; device 7 is removed") == 0); }
