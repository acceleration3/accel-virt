#include "core/platform/mutex.hpp"
#include "core/platform/mutex_guard.hpp"
#include "core/platform/spin_lock.hpp"
#include "core/platform/spin_lock_guard.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

using av::Irql;
using av::LockRank;
using av::MockPlatform;
using av::Mutex;
using av::MutexGuard;
using av::SpinLock;
using av::SpinLockGuard;

namespace {

int count_under(Mutex &mutex, MockPlatform &platform, bool early)
{
    MutexGuard guard(mutex);
    if (early) return static_cast<int>(platform.locks_held());
    return static_cast<int>(platform.locks_held()) + 1;
}

}  // namespace

TEST(mutex_guard, holds_apc_for_its_scope)
{ MockPlatform platform; Mutex mutex(platform, LockRank{10});
  { MutexGuard guard(mutex);
    CHECK(platform.current_irql() == Irql::apc); }
  CHECK(platform.current_irql() == Irql::passive);
  CHECK_EQ(platform.locks_held(), 0u); }

TEST(mutex_guard, releases_on_an_early_return)
{ MockPlatform platform; Mutex mutex(platform, LockRank{10});
  CHECK_EQ(count_under(mutex, platform, true), 1);
  CHECK_EQ(count_under(mutex, platform, false), 2);
  CHECK_EQ(platform.locks_held(), 0u); }

TEST(mutex_guard, a_spin_lock_nests_inside_a_mutex_of_lower_rank)
{ MockPlatform platform;
  Mutex mutex(platform, LockRank{10});
  SpinLock lock(platform, LockRank{20});
  { MutexGuard outer(mutex);
    SpinLockGuard inner(lock);
    CHECK(platform.current_irql() == Irql::dispatch); }
  CHECK(platform.current_irql() == Irql::passive);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }
