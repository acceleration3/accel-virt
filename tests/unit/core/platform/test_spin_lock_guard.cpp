#include "core/platform/spin_lock.hpp"
#include "core/platform/spin_lock_guard.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

using av::Irql;
using av::LockRank;
using av::MockPlatform;
using av::SpinLock;
using av::SpinLockGuard;

namespace {

class Guarded {
public:
    Guarded(MockPlatform &platform) : lock_(platform, LockRank{10}), platform_(platform) {}

    Irql irql_inside(bool early)
    {
        SpinLockGuard guard(lock_);
        if (early) return platform_.current_irql();
        return platform_.current_irql();
    }

private:
    SpinLock      lock_;
    MockPlatform &platform_;
};

}

TEST(spin_lock_guard, holds_dispatch_for_its_scope)
{ MockPlatform platform; SpinLock lock(platform, LockRank{10});
  { SpinLockGuard guard(lock);
    CHECK(platform.current_irql() == Irql::dispatch);
    CHECK_EQ(platform.locks_held(), 1u); }
  CHECK(platform.current_irql() == Irql::passive);
  CHECK_EQ(platform.locks_held(), 0u); }

TEST(spin_lock_guard, releases_on_an_early_return)
{ MockPlatform platform; Guarded guarded(platform);
  CHECK(guarded.irql_inside(true) == Irql::dispatch);
  CHECK(guarded.irql_inside(false) == Irql::dispatch);
  CHECK_EQ(platform.locks_held(), 0u);
  CHECK(platform.current_irql() == Irql::passive); }

TEST(spin_lock_guard, increasing_ranks_nest_without_violation)
{ MockPlatform platform;
  SpinLock outer(platform, LockRank{10});
  SpinLock inner(platform, LockRank{20});
  { SpinLockGuard first(outer);
    SpinLockGuard second(inner);
    CHECK_EQ(platform.locks_held(), 2u); }
  CHECK_EQ(platform.unclaimed_violations(), 0u);
  CHECK(platform.current_irql() == Irql::passive); }
