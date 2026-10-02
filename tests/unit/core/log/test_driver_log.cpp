/* DriverLog: every line carries its driver's prefix. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "core/log/driver_log.hpp"

#include <cstdint>
#include <cstring>

using av::RecordingLog;
using av::DriverLog;

namespace {

const char entry[] = "vkQueueSubmit";

}  // namespace

TEST(driver_log, every_line_carries_the_driver_s_prefix)
{ RecordingLog log;
  DriverLog    lines(log, "avvk: ");
  lines.write("a line");
  lines.note("memory types", 7u);
  lines.note_hex("a key", 0x2au, 4u);
  lines.name("a refusal of", entry);
  CHECK_EQ(log.lines(), 4u);
  CHECK(std::strcmp(log.line(0u), "avvk: a line") == 0);
  CHECK(std::strcmp(log.line(1u), "avvk: memory types 7") == 0);
  CHECK(std::strcmp(log.line(2u), "avvk: a key 0x002A") == 0);
  CHECK(std::strcmp(log.line(3u), "avvk: a refusal of vkQueueSubmit") == 0); }
