#include "miniport/display/flip_log.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>
#include <cstring>

using av::FlipLog;
using av::MockPlatform;
using av::StateWriter;

TEST(flip_log_note, counts_every_entry_and_reports_them_in_order)
{ MockPlatform platform; FlipLog log(platform);
  CHECK_EQ(log.count(), 0u);
  log.note(7u, 1u, 2u);
  log.note(8u, 3u, 4u);
  CHECK_EQ(log.count(), 2u);
  char buffer[4096]; StateWriter writer(buffer, sizeof buffer);
  log.report(writer, "programmed");
  const char *dump = writer.dump();
  CHECK(std::strstr(dump, "programmed/0/at/0 counter=") != nullptr);
  CHECK(std::strstr(dump, " id=7 mark=1 extra=2") != nullptr);
  CHECK(std::strstr(dump, " id=8 mark=3 extra=4") != nullptr);
  CHECK(!writer.truncated()); }

TEST(flip_log_note, keeps_only_the_newest_capacity_entries)
{ MockPlatform platform; FlipLog log(platform);
  for (std::uint64_t entry = 0u; entry < FlipLog::capacity + 5u; entry++) log.note(entry, entry, 0u);
  CHECK_EQ(log.count(), FlipLog::capacity + 5u);
  static char buffer[64 * 1024]; StateWriter writer(buffer, sizeof buffer);
  log.report(writer, "reported");
  CHECK(!writer.truncated());
  CHECK(std::strstr(writer.dump(), "/at/4 ") == nullptr);
  CHECK(std::strstr(writer.dump(), "/at/5 ") != nullptr);
  CHECK(std::strstr(writer.dump(), "/at/516 ") != nullptr); }
