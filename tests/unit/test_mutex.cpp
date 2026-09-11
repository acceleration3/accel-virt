#include "../../src/core/mutex.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"
#include "../harness/violation.hpp"

#include <cstddef>

using av::Irql;
using av::LockRank;
using av::MockPlatform;
using av::Mutex;
using av::Violation;

TEST(mutex, reserves_storage_for_the_kernel_mutex)
{ CHECK(Mutex::storage_bytes >= 56u); }

TEST(mutex_construct, above_dispatch_breaks_its_ceiling)
{ MockPlatform platform;
  platform.set_irql(Irql::isr);
  Mutex mutex(platform, LockRank{10});
  CHECK(platform.claim(Violation::irql_ceiling));
  platform.set_irql(Irql::passive); }

TEST(mutex_acquire, raises_to_apc_and_release_restores_passive)
{ MockPlatform platform; Mutex mutex(platform, LockRank{10});
  mutex.acquire();
  CHECK(platform.current_irql() == Irql::apc);
  mutex.release();
  CHECK(platform.current_irql() == Irql::passive);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(mutex_acquire, at_dispatch_breaks_its_ceiling_and_blocks_at_dispatch)
{ MockPlatform platform; Mutex mutex(platform, LockRank{10});
  platform.set_irql(Irql::dispatch);
  mutex.acquire();
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(platform.claim(Violation::blocking_at_dispatch));
  mutex.release();
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(platform.current_irql() == Irql::dispatch); }

TEST(mutex_acquire, an_equal_rank_breaks_lock_order)
{ MockPlatform platform;
  Mutex first(platform, LockRank{10});
  Mutex second(platform, LockRank{10});
  first.acquire();
  second.acquire();
  CHECK(platform.claim(Violation::lock_order));
  second.release();
  first.release(); }

TEST(mutex_release, out_of_acquisition_order_breaks_release_order)
{ MockPlatform platform;
  Mutex outer(platform, LockRank{10});
  Mutex inner(platform, LockRank{20});
  outer.acquire();
  inner.acquire();
  outer.release();
  CHECK(platform.claim(Violation::release_order));
  inner.release();
  outer.release();
  CHECK_EQ(platform.locks_held(), 0u); }
