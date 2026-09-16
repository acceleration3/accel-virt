#include "core/platform/lock_marks.hpp"
#include "core/platform/mutex.hpp"
#include "core/platform/mutex_try_guard.hpp"
#include "core/platform/spin_lock.hpp"
#include "core/platform/spin_lock_try_guard.hpp"
#include "core/platform/wait_mutex.hpp"
#include "core/platform/wait_mutex_try_guard.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/scope_binding.hpp"
#include "core/trace/trace_snapshot.hpp"
#include "core/trace/trace_table.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "harness/trace/mock_trace_host.hpp"

#include <cstdint>

using av::Irql;
using av::LockMarks;
using av::LockRank;
using av::MockPlatform;
using av::Mutex;
using av::MutexTryGuard;
using av::ScopeName;
using av::SpinLock;
using av::SpinLockTryGuard;
using av::Status;
using av::TraceSnapshot;
using av::Violation;
using av::WaitMutex;
using av::WaitMutexTryGuard;

namespace {

constexpr std::size_t one_slot = 96u + 824u;

/* A bound table and one open scope, so the calling thread has a slot the
 * marks land in. */
struct Marked {
    av::MockTraceHost host;
    alignas(8) std::uint8_t bytes[one_slot] = {};
    av::TraceTable table{host};

    Marked()
    {
        CHECK_STATUS(table.initialize(bytes, sizeof bytes, "test", 1u), Status::ok);
        av::scope_binding.bind(table);
    }
    ~Marked() { av::scope_binding.unbind(); }
    std::uint64_t waiting() const { return snapshot().slot(0u).waiting_on(); }
    std::uint64_t holding() const { return snapshot().slot(0u).holding(); }
    TraceSnapshot snapshot() const
    {
        TraceSnapshot read;
        CHECK_STATUS(read.read(bytes, sizeof bytes), Status::ok);
        return read;
    }
};

std::uint64_t address_of(const void *lock) { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(lock)); }

struct Seen {
    const Marked *marked;
    std::uint64_t waiting;
    std::uint64_t holding;
};

void look(void *context)
{
    auto *const seen = static_cast<Seen *>(context);
    seen->waiting = seen->marked->waiting();
    seen->holding = seen->marked->holding();
}

}  // namespace

TEST(lock_marks, name_the_lock_while_waiting_then_while_held_then_nothing)
{ MockPlatform platform;
  Marked marked;
  const av::FunctionScope scope(av::scope_binding, ScopeName("test::locks"));
  SpinLock spin(platform, LockRank{10});
  Mutex mutex(platform, LockRank{20});
  WaitMutex wait(platform, LockRank{30});
  Seen seen{&marked, 0u, 0u};
  platform.on_acquire(&look, &seen);

  const Irql previous = spin.acquire();
  CHECK_EQ(seen.waiting, address_of(&spin));
  CHECK_EQ(marked.waiting(), 0u);
  CHECK_EQ(marked.holding(), address_of(&spin));
  spin.release(previous);
  CHECK_EQ(marked.holding(), 0u);

  mutex.acquire();
  CHECK_EQ(seen.waiting, address_of(&mutex));
  CHECK_EQ(marked.holding(), address_of(&mutex));
  mutex.release();
  CHECK_EQ(marked.holding(), 0u);

  wait.acquire();
  CHECK_EQ(seen.waiting, address_of(&wait));
  CHECK_EQ(seen.holding, 0u);
  CHECK_EQ(marked.holding(), address_of(&wait));
  /* A nested lock is the one named as held; releasing it clears the name
   * rather than restoring the outer one. */
  Mutex inner(platform, LockRank{40});
  inner.acquire();
  CHECK_EQ(marked.holding(), address_of(&inner));
  CHECK_EQ(seen.holding, address_of(&wait));
  inner.release();
  CHECK_EQ(marked.holding(), 0u);
  wait.release();
  CHECK_EQ(marked.holding(), 0u);
  CHECK(marked.table.invariants_hold());
  CHECK_EQ(platform.locks_held(), 0u); }

TEST(lock_marks, unbound_write_nothing)
{ MockPlatform platform;
  const LockMarks marks(&platform);
  marks.waiting();
  marks.acquired();
  marks.released();
  SpinLock spin(platform, LockRank{10});
  spin.release(spin.acquire());
  CHECK(av::scope_binding.table() == nullptr);
  CHECK_EQ(platform.locks_held(), 0u); }

TEST(spin_lock_try_acquire, takes_a_free_lock_and_marks_it_held)
{ MockPlatform platform;
  Marked marked;
  const av::FunctionScope scope(av::scope_binding, ScopeName("test::try"));
  SpinLock spin(platform, LockRank{10});
  Irql previous = Irql::isr;
  CHECK(spin.try_acquire(previous));
  CHECK(previous == Irql::passive);
  CHECK(platform.current_irql() == Irql::dispatch);
  CHECK_EQ(marked.holding(), address_of(&spin));
  CHECK_EQ(marked.waiting(), 0u);
  spin.release(previous);
  CHECK(platform.current_irql() == Irql::passive); }

TEST(spin_lock_try_acquire, a_lock_held_elsewhere_is_refused_and_nothing_changes)
{ MockPlatform platform;
  Marked marked;
  const av::FunctionScope scope(av::scope_binding, ScopeName("test::try"));
  SpinLock spin(platform, LockRank{10});
  platform.hold_elsewhere(LockRank{10});
  Irql previous = Irql::isr;
  CHECK(!spin.try_acquire(previous));
  CHECK(previous == Irql::isr);
  CHECK(platform.current_irql() == Irql::passive);
  CHECK_EQ(platform.locks_held(), 0u);
  CHECK_EQ(marked.holding(), 0u);
  /* Another rank is not the one held elsewhere. */
  SpinLock other(platform, LockRank{11});
  CHECK(other.try_acquire(previous));
  other.release(previous);
  platform.released_elsewhere();
  CHECK(spin.try_acquire(previous));
  spin.release(previous); }

TEST(spin_lock_try_acquire, above_dispatch_breaks_its_ceiling)
{ MockPlatform platform; SpinLock spin(platform, LockRank{10});
  platform.set_irql(Irql::isr);
  Irql previous = Irql::passive;
  platform.hold_elsewhere(LockRank{10});
  CHECK(!spin.try_acquire(previous));
  CHECK(platform.claim(Violation::irql_ceiling));
  platform.set_irql(Irql::passive); }

TEST(mutex_try_acquire, takes_a_free_mutex_and_refuses_one_held_elsewhere)
{ MockPlatform platform;
  Marked marked;
  const av::FunctionScope scope(av::scope_binding, ScopeName("test::try"));
  Mutex mutex(platform, LockRank{20});
  CHECK(mutex.try_acquire());
  CHECK_EQ(marked.holding(), address_of(&mutex));
  CHECK(platform.current_irql() == Irql::apc);
  mutex.release();
  platform.hold_elsewhere(LockRank{20});
  CHECK(!mutex.try_acquire());
  CHECK_EQ(platform.locks_held(), 0u);
  CHECK_EQ(marked.holding(), 0u);
  platform.set_irql(Irql::dispatch);
  CHECK(!mutex.try_acquire());
  CHECK(platform.claim(Violation::irql_ceiling));
  platform.set_irql(Irql::passive); }

TEST(wait_mutex_try_acquire, takes_a_free_mutex_and_refuses_one_held_elsewhere)
{ MockPlatform platform;
  Marked marked;
  const av::FunctionScope scope(av::scope_binding, ScopeName("test::try"));
  WaitMutex wait(platform, LockRank{30});
  CHECK(wait.try_acquire());
  CHECK_EQ(marked.holding(), address_of(&wait));
  CHECK(platform.current_irql() == Irql::passive);
  wait.release();
  platform.hold_elsewhere(LockRank{30});
  CHECK(!wait.try_acquire());
  CHECK_EQ(platform.locks_held(), 0u);
  platform.set_irql(Irql::apc);
  CHECK(!wait.try_acquire());
  CHECK(platform.claim(Violation::irql_ceiling));
  platform.set_irql(Irql::passive); }

TEST(spin_lock_try_guard, releases_only_what_it_took)
{ MockPlatform platform; SpinLock spin(platform, LockRank{10});
  {
      const SpinLockTryGuard guard(spin);
      CHECK(guard.held());
      CHECK(platform.current_irql() == Irql::dispatch);
      CHECK_EQ(platform.locks_held(), 1u);
  }
  CHECK(platform.current_irql() == Irql::passive);
  CHECK_EQ(platform.locks_held(), 0u);
  platform.hold_elsewhere(LockRank{10});
  {
      const SpinLockTryGuard guard(spin);
      CHECK(!guard.held());
  }
  CHECK_EQ(platform.locks_held(), 0u);
  CHECK(platform.current_irql() == Irql::passive); }

TEST(mutex_try_guard, releases_only_what_it_took)
{ MockPlatform platform; Mutex mutex(platform, LockRank{20});
  {
      const MutexTryGuard guard(mutex);
      CHECK(guard.held());
      CHECK_EQ(platform.locks_held(), 1u);
  }
  CHECK_EQ(platform.locks_held(), 0u);
  platform.hold_elsewhere(LockRank{20});
  {
      const MutexTryGuard guard(mutex);
      CHECK(!guard.held());
  }
  CHECK_EQ(platform.locks_held(), 0u); }

TEST(wait_mutex_try_guard, releases_only_what_it_took)
{ MockPlatform platform; WaitMutex wait(platform, LockRank{30});
  {
      const WaitMutexTryGuard guard(wait);
      CHECK(guard.held());
      CHECK_EQ(platform.locks_held(), 1u);
  }
  CHECK_EQ(platform.locks_held(), 0u);
  platform.hold_elsewhere(LockRank{30});
  {
      const WaitMutexTryGuard guard(wait);
      CHECK(!guard.held());
  }
  CHECK_EQ(platform.locks_held(), 0u); }
