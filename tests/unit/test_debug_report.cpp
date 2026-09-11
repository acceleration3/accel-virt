#include "../../src/core/debug_report.hpp"
#include "../harness/guarded_buffer.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"
#include "../harness/violation.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::DebugReport;
using av::GuardedBuffer;
using av::Irql;
using av::MockPlatform;
using av::Violation;

TEST(debug_report_write, lays_out_magic_format_state_and_features_little_endian)
{ MockPlatform platform; GuardedBuffer buffer(DebugReport::report_bytes);
  const DebugReport report(platform, true, false, 0x0000000100000018ull);
  CHECK_EQ(report.write(buffer.data(), DebugReport::report_bytes), DebugReport::report_bytes);
  const std::uint8_t expected[DebugReport::report_bytes] = {
      'R', 'V', 'K', 'D', 1, 0, 1, 0, 0x18, 0, 0, 0, 1, 0, 0, 0};
  CHECK(std::memcmp(buffer.data(), expected, sizeof expected) == 0);
  CHECK(buffer.guards_intact()); }

TEST(debug_report_write, a_short_buffer_gets_the_whole_fields_that_fit)
{ MockPlatform platform;
  const DebugReport report(platform, false, true, ~0ull);
  struct Case { std::size_t capacity; std::size_t written; };
  const Case cases[] = {{0u, 0u}, {3u, 0u}, {4u, 4u}, {5u, 4u}, {6u, 6u}, {7u, 7u}, {8u, 8u},
                        {15u, 8u}, {16u, 16u}, {64u, 16u}};
  for (const Case &c : cases) {
      GuardedBuffer buffer(64u);
      std::memset(buffer.data(), 0xEE, 64u);
      CHECK_EQ(report.write(buffer.data(), c.capacity), c.written);
      CHECK_EQ(buffer.data()[c.written], 0xEEu);
      CHECK(buffer.guards_intact());
  } }

TEST(debug_report_write, a_null_buffer_gets_nothing)
{ MockPlatform platform;
  CHECK_EQ(DebugReport(platform, true, true, 1u).write(nullptr, 64u), 0u); }

TEST(debug_report, is_passive_only)
{ MockPlatform platform; std::uint8_t buffer[DebugReport::report_bytes] = {};
  const DebugReport report(platform, true, true, 1u);
  platform.set_irql(Irql::dispatch);
  static_cast<void>(report.write(buffer, sizeof buffer));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling)); }
