#include "../../src/core/debug_report.hpp"
#include "../../src/core/execution_node.hpp"
#include "../../src/core/status.hpp"
#include "../harness/guarded_buffer.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"
#include "../harness/violation.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::DebugReport;
using av::ExecutionNode;
using av::Status;
using av::GuardedBuffer;
using av::Irql;
using av::MockPlatform;
using av::Violation;

TEST(debug_report_write, lays_out_magic_format_state_features_and_node_progress_little_endian)
{ MockPlatform platform; GuardedBuffer buffer(DebugReport::report_bytes); ExecutionNode node(platform);
  platform.advance_time(0x0102u);
  CHECK_STATUS(node.submit(3u), Status::ok);
  const DebugReport report(platform, true, false, 0x0000000100000018ull, node);
  CHECK_EQ(report.write(buffer.data(), DebugReport::report_bytes), DebugReport::report_bytes);
  const std::uint8_t expected[DebugReport::report_bytes] = {
      'R', 'V', 'K', 'D', 2, 0, 1, 0, 0x18, 0, 0, 0, 1, 0, 0, 0,
      3, 0, 0, 0, 0, 0, 0, 0, 0x02, 0x01, 0, 0, 0, 0, 0, 0,
      1, 0, 0, 0, 0, 0, 0, 0};
  CHECK(std::memcmp(buffer.data(), expected, sizeof expected) == 0);
  CHECK(buffer.guards_intact()); }

TEST(debug_report_write, a_short_buffer_gets_the_whole_fields_that_fit)
{ MockPlatform platform; ExecutionNode node(platform);
  const DebugReport report(platform, false, true, ~0ull, node);
  struct Case { std::size_t capacity; std::size_t written; };
  const Case cases[] = {{0u, 0u}, {3u, 0u}, {4u, 4u}, {5u, 4u}, {6u, 6u}, {7u, 7u}, {8u, 8u},
                        {15u, 8u}, {16u, 16u}, {19u, 16u}, {20u, 20u}, {24u, 24u}, {31u, 24u},
                        {32u, 32u}, {36u, 36u}, {39u, 36u}, {40u, 40u}, {63u, 40u}};
  for (const Case &c : cases) {
      GuardedBuffer buffer(64u);
      std::memset(buffer.data(), 0xEE, 64u);
      CHECK_EQ(report.write(buffer.data(), c.capacity), c.written);
      CHECK_EQ(buffer.data()[c.written], 0xEEu);
      CHECK(buffer.guards_intact());
  } }

TEST(debug_report_write, a_null_buffer_gets_nothing)
{ MockPlatform platform; ExecutionNode node(platform);
  CHECK_EQ(DebugReport(platform, true, true, 1u, node).write(nullptr, 64u), 0u); }

TEST(debug_report, is_passive_only)
{ MockPlatform platform; std::uint8_t buffer[DebugReport::report_bytes] = {}; ExecutionNode node(platform);
  const DebugReport report(platform, true, true, 1u, node);
  platform.set_irql(Irql::dispatch);
  static_cast<void>(report.write(buffer, sizeof buffer));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling)); }
