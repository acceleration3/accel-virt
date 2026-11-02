#include "core/platform/wait_mutex.hpp"
#include "core/platform/wait_mutex_guard.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

using av::Irql;
using av::LockRank;
using av::MockPlatform;
using av::Violation;
using av::WaitMutex;
using av::WaitMutexGuard;

TEST(wait_mutex, reserves_storage_for_the_kernel_mutex)
{ CHECK(WaitMutex::storage_bytes >= 56u); }

TEST(wait_mutex_acquire, keeps_passive_so_its_holder_may_wait)
{ MockPlatform platform; WaitMutex mutex(platform, LockRank{10});
  mutex.acquire();
  CHECK(platform.current_irql() == Irql::passive);
  CHECK_EQ(platform.locks_held(), 1u);
  platform.require_irql_at_most(Irql::passive, "a wait for an answer");
  mutex.release();
  CHECK_EQ(platform.locks_held(), 0u);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(wait_mutex_acquire, above_passive_breaks_its_ceiling_and_blocks_at_dispatch)
{ MockPlatform platform; WaitMutex mutex(platform, LockRank{10});
  platform.set_irql(Irql::apc);
  mutex.acquire();
  CHECK(platform.claim(Violation::irql_ceiling));
  mutex.release();
  CHECK(platform.claim(Violation::irql_ceiling));
  platform.set_irql(Irql::dispatch);
  mutex.acquire();
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(platform.claim(Violation::blocking_at_dispatch));
  mutex.release();
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(platform.current_irql() == Irql::dispatch);
  platform.set_irql(Irql::passive); }

TEST(wait_mutex_acquire, is_ranked_like_every_other_lock)
{ MockPlatform platform;
  WaitMutex outer(platform, LockRank{20});
  WaitMutex inner(platform, LockRank{10});
  outer.acquire();
  inner.acquire();
  CHECK(platform.claim(Violation::lock_order));
  inner.release();
  outer.release();
  outer.acquire();
  inner.release();
  CHECK(platform.claim(Violation::release_order));
  outer.release(); }

TEST(wait_mutex_construct, above_dispatch_breaks_its_ceiling)
{ MockPlatform platform;
  platform.set_irql(Irql::isr);
  WaitMutex mutex(platform, LockRank{10});
  CHECK(platform.claim(Violation::irql_ceiling));
  platform.set_irql(Irql::passive); }

namespace {

std::uint32_t held_inside(WaitMutex &mutex, MockPlatform &platform, bool early)
{
    WaitMutexGuard guard(mutex);
    if (early) return platform.locks_held();
    return platform.locks_held() + 10u;
}

}

TEST(wait_mutex_guard, holds_for_its_scope_on_every_return)
{ MockPlatform platform; WaitMutex mutex(platform, LockRank{10});
  CHECK_EQ(held_inside(mutex, platform, true), 1u);
  CHECK_EQ(platform.locks_held(), 0u);
  CHECK_EQ(held_inside(mutex, platform, false), 11u);
  CHECK_EQ(platform.locks_held(), 0u);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }
