/* DriverLog: every line carries its driver's prefix, a key written once per
 * code and never again, and past the bound nothing written at all - the lines
 * a driver would otherwise write on every call of an application that keeps
 * calling. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "icd/common/driver_log.hpp"

#include <cstdint>
#include <cstring>

using av::RecordingLog;
using av::icd::DriverLog;

namespace {

const char entry[] = "vkQueueSubmit";
const char other[] = "vkQueuePresentKHR";

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

/* A key is its pointer and its code: one key failing two ways is two lines,
 * and the same failure again is none. */
TEST(driver_log, a_key_is_first_once_for_each_code)
{ RecordingLog log;
  DriverLog    lines(log, "avumd: ");
  CHECK(lines.first(entry, -5));
  CHECK(!lines.first(entry, -5));
  CHECK(lines.first(entry, -6));
  CHECK(lines.first(other, -5));
  CHECK(!lines.first(other, -5));
  CHECK_EQ(lines.keys(), 3u);
  CHECK_EQ(log.lines(), 0u); }

TEST(driver_log, once_writes_a_line_the_first_time_only)
{ RecordingLog log;
  DriverLog    lines(log, "avcl: ");
  lines.once("a buffer could not be mapped");
  lines.once("a buffer could not be mapped");
  lines.once_name("slot not implemented:", entry);
  lines.once_name("slot not implemented:", entry);
  lines.once_name("slot not implemented:", other);
  CHECK_EQ(log.lines(), 3u);
  CHECK(std::strcmp(log.line(0u), "avcl: a buffer could not be mapped") == 0);
  CHECK(std::strcmp(log.line(1u), "avcl: slot not implemented: vkQueueSubmit") == 0);
  CHECK(std::strcmp(log.line(2u), "avcl: slot not implemented: vkQueuePresentKHR") == 0); }

/* Past max_keys a new key is not first: a line per call is what the bound is
 * there to prevent, and the table never grows past it. */
TEST(driver_log, past_the_bound_nothing_is_first)
{ RecordingLog log;
  DriverLog    lines(log, "avgl: ");
  static char keys[DriverLog::max_keys + 2u][2] = {};
  for (std::uint32_t index = 0u; index < DriverLog::max_keys; ++index) CHECK(lines.first(keys[index], 0));
  CHECK_EQ(lines.keys(), DriverLog::max_keys);
  CHECK(!lines.first(keys[DriverLog::max_keys], 0));
  CHECK(!lines.first(keys[DriverLog::max_keys + 1u], 0));
  CHECK_EQ(lines.keys(), DriverLog::max_keys);
  /* What was remembered still is. */
  CHECK(!lines.first(keys[0], 0)); }
