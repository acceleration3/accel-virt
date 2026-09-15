/* The ICD's test seams are the oracles every ICD test judges locking, logging
 by, so their own rules are tested: a recursive acquire
 * and a stray release are violations a test must claim, a lock left held fails
 * the test, the trace keeps the order, the log keeps and counts lines. */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "icd/vulkan/log_sink.hpp"
#include "icd/vulkan/thread_lock.hpp"

#include <cstdint>
#include <cstring>

using av::LockTrace;
using av::RecordingLog;
using av::RecordingThreadLock;
using av::Status;

TEST(recording_thread_lock, traces_acquire_and_release_of_each_lock_in_order)
{ LockTrace trace;
  RecordingThreadLock outer(trace, 'S');
  RecordingThreadLock inner(trace, 'H');
  av::icd::ThreadLock &seam = outer;
  seam.acquire();
  CHECK(outer.held());
  CHECK(!inner.held());
  inner.acquire();
  inner.release();
  seam.release();
  CHECK(!outer.held());
  CHECK(std::strcmp(trace.text(), "SHhs") == 0);
  CHECK_EQ(outer.acquisitions(), 1u);
  CHECK_EQ(inner.acquisitions(), 1u);
  CHECK(!outer.claim_violation()); }

TEST(recording_thread_lock, a_recursive_acquire_is_a_violation_and_does_not_count)
{ LockTrace trace;
  RecordingThreadLock lock(trace, 'S');
  lock.acquire();
  lock.acquire();
  CHECK_EQ(lock.acquisitions(), 1u);
  CHECK(lock.held());
  lock.release();
  CHECK(!lock.held());
  CHECK(lock.claim_violation());
  CHECK(!lock.claim_violation());
  CHECK(std::strcmp(trace.text(), "SSs") == 0); }

TEST(recording_thread_lock, a_release_of_a_lock_not_held_is_a_violation)
{ LockTrace trace;
  RecordingThreadLock lock(trace, 'H');
  lock.release();
  CHECK(!lock.held());
  CHECK(lock.claim_violation());
  CHECK(std::strcmp(trace.text(), "h") == 0); }

TEST(recording_thread_lock, an_unclaimed_violation_or_a_held_lock_fails_the_test)
{ LockTrace trace;
  { RecordingThreadLock lock(trace, 'S');
    lock.release(); }
  CHECK(av::TestRegistry::instance().take_current_failure());
  { RecordingThreadLock lock(trace, 'S');
    lock.acquire(); }
  CHECK(av::TestRegistry::instance().take_current_failure());
  { RecordingThreadLock lock(trace, 'S');
    lock.acquire();
    lock.release(); }
  CHECK(!av::TestRegistry::instance().take_current_failure()); }

TEST(lock_trace, is_cut_at_its_capacity_and_marks_the_cut)
{ LockTrace trace;
  for (std::uint32_t event = 0u; event < LockTrace::capacity + 3u; event++) trace.append('S');
  CHECK_EQ(std::strlen(trace.text()), LockTrace::capacity);
  CHECK_EQ(trace.text()[LockTrace::capacity - 1u], '~');
  CHECK_EQ(trace.text()[LockTrace::capacity - 2u], 'S');
  trace.clear();
  CHECK_EQ(std::strlen(trace.text()), 0u);
  trace.append('H');
  CHECK(std::strcmp(trace.text(), "H") == 0); }

TEST(recording_log, keeps_each_line_and_finds_what_they_hold)
{ RecordingLog log;
  av::icd::LogSink &sink = log;
  CHECK_EQ(log.lines(), 0u);
  CHECK(std::strcmp(log.line(0u), "") == 0);
  sink.write("rvkvk: refused 4");
  sink.write("rvkvk: refused 3");
  CHECK_EQ(log.lines(), 2u);
  CHECK(std::strcmp(log.line(1u), "rvkvk: refused 3") == 0);
  CHECK_EQ(log.holding("refused"), 2u);
  CHECK_EQ(log.holding("refused 4"), 1u);
  CHECK_EQ(log.holding("accepted"), 0u); }

TEST(recording_log, counts_lines_past_its_bound_and_cuts_long_ones)
{ RecordingLog log;
  char long_line[RecordingLog::line_bytes * 2u] = {};
  std::memset(long_line, 'x', sizeof long_line - 1u);
  log.write(long_line);
  CHECK_EQ(std::strlen(log.line(0u)), RecordingLog::line_bytes - 1u);
  for (std::uint32_t line = 1u; line < RecordingLog::max_lines + 2u; line++) log.write("again");
  CHECK_EQ(log.lines(), RecordingLog::max_lines + 2u);
  CHECK_EQ(log.holding("again"), RecordingLog::max_lines - 1u);
  CHECK(std::strcmp(log.line(RecordingLog::max_lines), "") == 0); }
