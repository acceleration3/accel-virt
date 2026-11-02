#include "core/platform/thread_lock_hold.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"

namespace {

bool held_inside(av::RecordingThreadLock &lock)
{
    const av::ThreadLockHold hold(lock);
    return lock.held();
}

}

TEST(thread_lock_hold, holds_the_lock_for_its_scope_and_no_longer)
{ av::LockTrace           trace;
  av::RecordingThreadLock lock(trace, 'h');
  CHECK(!lock.held());
  CHECK(held_inside(lock));
  CHECK(!lock.held());
  CHECK(held_inside(lock));
  CHECK(!lock.held());
  CHECK_EQ(lock.acquisitions(), 2u);
  CHECK(!lock.claim_violation()); }
