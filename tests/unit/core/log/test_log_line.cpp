/* One line composed from text and values, written with the log's prefix
 * (LogLine), and cut rather than overrun when too long. */
#include "core/log/driver_log.hpp"
#include "core/log/log_line.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"

#include <cstring>

using av::DriverLog;
using av::LogLine;
using av::RecordingLog;

TEST(log_line, writes_text_and_values_in_order_after_the_prefix)
{ RecordingLog log;
  DriverLog    lines(log, "virtio-gpu-av: d3d12: ");
  LogLine(lines).text("a heap of ").number(65536u).text(" bytes, key ").hex(0x2Au, 4u).write();
  CHECK_EQ(log.lines(), 1u);
  CHECK(std::strcmp(log.line(0u), "virtio-gpu-av: d3d12: a heap of 65536 bytes, key 0x002A") == 0); }

TEST(log_line, a_line_too_long_is_cut_not_overrun)
{ RecordingLog log;
  DriverLog    lines(log, "avvk: ");
  LogLine      line(lines);
  for (int word = 0; word < 100; word++) line.text("overlong ");
  line.write();
  CHECK_EQ(log.lines(), 1u);
  CHECK(std::strncmp(log.line(0u), "avvk: overlong ", 15u) == 0);
  CHECK(std::strlen(log.line(0u)) < DriverLog::line_bytes); }
