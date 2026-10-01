/* KeyedLog: a key written once per code and never again, and past the
 * bound nothing written at all - the lines a driver would otherwise write on
 * every call of an application that keeps calling. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "icd/common/keyed_log.hpp"

#include <cstdint>
#include <cstring>

using av::RecordingLog;
using av::icd::KeyedLog;

namespace av::icd {

/* A row another thread has taken and not yet written. */
struct KeyedLogProbe {
    static void take_a_row(KeyedLog &log) { log.count_++; }
};

}  // namespace av::icd

namespace {

const char entry[] = "vkQueueSubmit";
const char other[] = "vkQueuePresentKHR";

}  // namespace

/* A key is its pointer and its code: one key failing two ways is two lines,
 * and the same failure again is none. */
TEST(keyed_log, a_key_is_first_once_for_each_code)
{ RecordingLog log;
  KeyedLog     lines(log, "avumd: ");
  CHECK(lines.first(entry, -5));
  CHECK(!lines.first(entry, -5));
  CHECK(lines.first(entry, -6));
  CHECK(lines.first(other, -5));
  CHECK(!lines.first(other, -5));
  CHECK_EQ(lines.keys(), 3u);
  CHECK_EQ(log.lines(), 0u); }

TEST(keyed_log, once_writes_a_line_the_first_time_only)
{ RecordingLog log;
  KeyedLog     lines(log, "avcl: ");
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
TEST(keyed_log, past_the_bound_nothing_is_first)
{ RecordingLog log;
  KeyedLog     lines(log, "avgl: ");
  static char keys[KeyedLog::max_keys + 2u][2] = {};
  for (std::uint32_t index = 0u; index < KeyedLog::max_keys; ++index) CHECK(lines.first(keys[index], 0));
  CHECK_EQ(lines.keys(), KeyedLog::max_keys);
  CHECK(!lines.first(keys[KeyedLog::max_keys], 0));
  CHECK(!lines.first(keys[KeyedLog::max_keys + 1u], 0));
  CHECK_EQ(lines.keys(), KeyedLog::max_keys);
  /* What was remembered still is. */
  CHECK(!lines.first(keys[0], 0)); }

/* A row another thread is still writing is passed over, not read: its key may
 * be half there. The key after it is first, in the next row. */
TEST(keyed_log, a_row_still_being_written_is_passed_over)
{ RecordingLog log;
  KeyedLog     lines(log, "avvk: ");
  CHECK(lines.first(entry, 1));
  av::icd::KeyedLogProbe::take_a_row(lines);
  CHECK(lines.first(other, 1));
  CHECK_EQ(lines.keys(), 3u);
  CHECK(!lines.first(other, 1));
  CHECK(!lines.first(entry, 1)); }
