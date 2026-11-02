#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"
#include "core/log/keyed_log.hpp"

#include <cstdint>
#include <cstring>

using av::KeyedLog;
using av::MockPlatform;
using av::RecordingLog;

namespace {

const char entry[] = "vkQueueSubmit";
const char other[] = "vkQueuePresentKHR";

}

TEST(keyed_log, a_key_is_first_once_for_each_code)
{ MockPlatform platform;
  RecordingLog log;
  KeyedLog     lines(platform, log, "avumd: ");
  CHECK(lines.first(entry, -5));
  CHECK(!lines.first(entry, -5));
  CHECK(lines.first(entry, -6));
  CHECK(lines.first(other, -5));
  CHECK(!lines.first(other, -5));
  CHECK_EQ(lines.keys(), 3u);
  CHECK_EQ(log.lines(), 0u); }

TEST(keyed_log, once_writes_a_line_the_first_time_only)
{ MockPlatform platform;
  RecordingLog log;
  KeyedLog     lines(platform, log, "avcl: ");
  lines.once("a buffer could not be mapped");
  lines.once("a buffer could not be mapped");
  lines.once_name("slot not implemented:", entry);
  lines.once_name("slot not implemented:", entry);
  lines.once_name("slot not implemented:", other);
  CHECK_EQ(log.lines(), 3u);
  CHECK(std::strcmp(log.line(0u), "avcl: a buffer could not be mapped") == 0);
  CHECK(std::strcmp(log.line(1u), "avcl: slot not implemented: vkQueueSubmit") == 0);
  CHECK(std::strcmp(log.line(2u), "avcl: slot not implemented: vkQueuePresentKHR") == 0); }

TEST(keyed_log, remembers_every_key_past_the_old_bound)
{ MockPlatform platform;
  RecordingLog log;
  KeyedLog     lines(platform, log, "avgl: ");
  constexpr std::uint32_t many = 5000u;
  static char keys[many][2] = {};
  for (std::uint32_t index = 0u; index < many; ++index) CHECK(lines.first(keys[index], 0));
  CHECK_EQ(lines.keys(), many);
  for (std::uint32_t index = 0u; index < many; ++index) CHECK(!lines.first(keys[index], 0));
  CHECK_EQ(lines.keys(), many); }

TEST(keyed_log, a_key_that_cannot_be_remembered_writes_nothing)
{ MockPlatform platform;
  RecordingLog log;
  KeyedLog     lines(platform, log, "avvk: ");
  static const char lost[] = "the host lost the device";
  platform.fail_allocation_at(1u);
  lines.once(lost);
  CHECK_EQ(log.lines(), 0u);
  lines.once(lost);
  lines.once(lost);
  CHECK_EQ(log.lines(), 1u);
  CHECK(std::strcmp(log.line(0u), "avvk: the host lost the device") == 0); }
