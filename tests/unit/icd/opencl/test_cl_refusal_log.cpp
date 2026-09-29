/* ClRefusalLog: an entry point this ICD does not serve is named in the log the
 * first time it is called and never again, under the root's lock, and a
 * refusal past the bound is dropped rather than written. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "icd/opencl/cl_refusal_log.hpp"

#include <cstdint>
#include <cstring>

using av::clicd::ClRefusalLog;

namespace {

const char image_2d[] = "clCreateImage2D";
const char mem_info[] = "clGetMemObjectInfo";

}  // namespace

TEST(cl_refusal_log, each_refused_entry_point_is_named_once_and_in_the_order_called)
{ av::LockTrace           trace;
  av::RecordingThreadLock lock(trace, 'r');
  av::RecordingLog        log;
  ClRefusalLog            refusals(log, lock);
  refusals.note(image_2d);
  refusals.note(mem_info);
  refusals.note(image_2d);
  refusals.note(mem_info);
  CHECK_EQ(log.lines(), 2u);
  CHECK_EQ(refusals.named(), 2u);
  CHECK(std::strcmp(log.line(0u), "rvkcl: an entry point this ICD does not serve was called: clCreateImage2D") == 0);
  CHECK(std::strcmp(log.line(1u), "rvkcl: an entry point this ICD does not serve was called: clGetMemObjectInfo") == 0);
  /* Every look at the list is under the lock, and none is left held. */
  CHECK_EQ(lock.acquisitions(), 4u);
  CHECK(!lock.held());
  CHECK(!lock.claim_violation()); }

TEST(cl_refusal_log, nothing_is_named_for_no_entry_point_or_past_the_bound)
{ av::LockTrace           trace;
  av::RecordingThreadLock lock(trace, 'r');
  av::RecordingLog        log;
  ClRefusalLog            refusals(log, lock);
  refusals.note(nullptr);
  CHECK_EQ(log.lines(), 0u);
  CHECK_EQ(lock.acquisitions(), 0u);
  static char names[ClRefusalLog::max_named + 1u][4] = {};
  for (std::uint32_t index = 0u; index < ClRefusalLog::max_named; ++index) refusals.note(names[index]);
  CHECK_EQ(log.lines(), ClRefusalLog::max_named);
  CHECK_EQ(refusals.named(), ClRefusalLog::max_named);
  /* One more than it can remember is not written: a line per call is what
   * the bound is there to prevent. */
  refusals.note(names[ClRefusalLog::max_named]);
  refusals.note(names[ClRefusalLog::max_named]);
  CHECK_EQ(log.lines(), ClRefusalLog::max_named);
  CHECK_EQ(refusals.named(), ClRefusalLog::max_named); }

TEST(cl_refusal_log, each_error_an_entry_point_answers_is_named_once_and_success_never)
{ av::LockTrace           trace;
  av::RecordingThreadLock lock(trace, 'r');
  av::RecordingLog        log;
  ClRefusalLog            refusals(log, lock);
  refusals.note_failure(image_2d, -38);
  refusals.note_failure(image_2d, -38);
  refusals.note_failure(image_2d, -30);
  refusals.note_failure(mem_info, -38);
  /* Not errors: success, and the positive counts some calls answer. */
  refusals.note_failure(mem_info, 0);
  refusals.note_failure(mem_info, 5);
  refusals.note_failure(nullptr, -38);
  CHECK_EQ(log.lines(), 3u);
  CHECK(std::strcmp(log.line(0u), "rvkcl: clCreateImage2D answered -38") == 0);
  CHECK(std::strcmp(log.line(1u), "rvkcl: clCreateImage2D answered -30") == 0);
  CHECK(std::strcmp(log.line(2u), "rvkcl: clGetMemObjectInfo answered -38") == 0);
  /* A refusal and a failure of one entry point are two things to name. */
  refusals.note(image_2d);
  CHECK_EQ(log.lines(), 4u);
  CHECK(!lock.held()); }
