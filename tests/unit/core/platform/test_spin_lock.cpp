#include "core/platform/spin_lock.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

using av::Irql;
using av::LockRank;
using av::MockPlatform;
using av::SpinLock;
using av::Violation;

TEST(spin_lock_acquire, raises_to_dispatch_and_release_restores_passive)
{ MockPlatform platform; SpinLock lock(platform, LockRank{10});
  const Irql previous = lock.acquire();
  CHECK(previous == Irql::passive);
  CHECK(platform.current_irql() == Irql::dispatch);
  lock.release(previous);
  CHECK(platform.current_irql() == Irql::passive); }

TEST(spin_lock_acquire, from_dispatch_restores_dispatch)
{ MockPlatform platform; SpinLock lock(platform, LockRank{10});
  platform.set_irql(Irql::dispatch);
  const Irql previous = lock.acquire();
  lock.release(previous);
  CHECK(platform.current_irql() == Irql::dispatch); }

TEST(spin_lock_acquire, above_dispatch_breaks_its_ceiling)
{ MockPlatform platform; SpinLock lock(platform, LockRank{10});
  platform.set_irql(Irql::isr);
  const Irql previous = lock.acquire();
  CHECK(platform.claim(Violation::irql_ceiling));
  lock.release(previous);
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(platform.current_irql() == Irql::isr); }

TEST(spin_lock_acquire, a_lower_rank_under_a_higher_one_breaks_lock_order)
{ MockPlatform platform;
  SpinLock outer(platform, LockRank{20});
  SpinLock inner(platform, LockRank{10});
  const Irql first = outer.acquire();
  const Irql second = inner.acquire();
  CHECK(platform.claim(Violation::lock_order));
  inner.release(second);
  outer.release(first); }

TEST(spin_lock_release, out_of_acquisition_order_breaks_release_order)
{ MockPlatform platform;
  SpinLock outer(platform, LockRank{10});
  SpinLock inner(platform, LockRank{20});
  const Irql first = outer.acquire();
  const Irql second = inner.acquire();
  outer.release(first);
  CHECK(platform.claim(Violation::release_order));
  inner.release(second);
  outer.release(first);
  CHECK_EQ(platform.locks_held(), 0u); }
