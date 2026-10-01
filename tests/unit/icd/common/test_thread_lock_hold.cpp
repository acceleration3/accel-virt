/* ThreadLockHold: the lock taken when the hold is made and let go when it
 * goes out of scope. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "icd/common/thread_lock_hold.hpp"

namespace {

bool held_inside(av::RecordingThreadLock &lock)
{
    const av::icd::ThreadLockHold hold(lock);
    return lock.held();
}

}  // namespace

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
