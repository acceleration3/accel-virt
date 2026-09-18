/* What each hardware queue runs on, read at DISPATCH (feature 049).
 *
 * The queue table answers this too, behind a mutex a submission may not take,
 * so what these tests judge is the part that makes the copy worth having: a
 * handle nobody bound is answered with nothing rather than followed, and a
 * queue destroyed is one the next submission cannot reach.
 */
#include "core/platform/lock_rank.hpp"
#include "core/scheduling/hw_queue_contexts.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>

using av::HwQueueContexts;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;

namespace {

/* The contexts are only ever compared and answered, so what they point at is
 * the test's own memory: nothing here reads a Context. */
av::Context &as_context(int &stand_in)
{
    return *reinterpret_cast<av::Context *>(&stand_in);
}

}  // namespace

TEST(hw_queue_contexts, answers_the_context_a_queue_runs_on_and_nothing_for_one_nobody_bound)
{ MockPlatform    platform;
  HwQueueContexts bound(platform);
  CHECK(bound.invariants_hold());
  CHECK_EQ(bound.bound(), 0u);
  int first_context = 0;
  int second_context = 0;
  int first_queue = 0;
  int second_queue = 0;
  int never_bound = 0;
  CHECK_STATUS(bound.bind(&first_queue, as_context(first_context)), Status::ok);
  CHECK_STATUS(bound.bind(&second_queue, as_context(second_context)), Status::ok);
  CHECK_EQ(bound.bound(), 2u);
  CHECK(bound.context_of(&first_queue) == &as_context(first_context));
  CHECK(bound.context_of(&second_queue) == &as_context(second_context));
  CHECK(bound.context_of(&never_bound) == nullptr);
  CHECK(bound.context_of(nullptr) == nullptr);
  CHECK(bound.invariants_hold());

  /* A queue destroyed is one the next submission cannot reach, and the other
   * is untouched. */
  bound.forget(&first_queue);
  CHECK(bound.context_of(&first_queue) == nullptr);
  CHECK(bound.context_of(&second_queue) == &as_context(second_context));
  CHECK_EQ(bound.bound(), 1u);
  bound.forget(&never_bound);
  bound.forget(nullptr);
  CHECK_EQ(bound.bound(), 1u);
  CHECK(bound.invariants_hold()); }

TEST(hw_queue_contexts, refuses_a_queue_that_is_no_queue_and_a_table_with_no_room)
{ MockPlatform    platform;
  HwQueueContexts bound(platform);
  int             context = 0;
  CHECK_STATUS(bound.bind(nullptr, as_context(context)), Status::invalid_parameter);
  /* One slot per queue a process may hold; past that there is nowhere to
   * write the pair, and a submission that could not find its context would
   * fault a context at random. */
  int queues[HwQueueContexts::capacity + 1u] = {};
  for (std::uint32_t index = 0u; index < HwQueueContexts::capacity; index++)
      CHECK_STATUS(bound.bind(&queues[index], as_context(context)), Status::ok);
  CHECK_STATUS(bound.bind(&queues[HwQueueContexts::capacity], as_context(context)), Status::no_space);
  CHECK_EQ(bound.bound(), HwQueueContexts::capacity);
  CHECK(bound.invariants_hold());
  bound.forget(&queues[0]);
  CHECK_STATUS(bound.bind(&queues[HwQueueContexts::capacity], as_context(context)), Status::ok); }

TEST(hw_queue_contexts, runs_at_the_levels_it_declares)
{ MockPlatform    platform;
  HwQueueContexts bound(platform);
  int             context = 0;
  int             queue = 0;
  platform.set_irql(Irql::dispatch);
  static_cast<void>(bound.bind(&queue, as_context(context)));
  /* A submission reads it at DISPATCH, and nothing reads it above that. */
  static_cast<void>(bound.context_of(&queue));
  bound.forget(&queue);
  static_cast<void>(bound.bound());
  platform.set_irql(Irql::isr);
  static_cast<void>(bound.context_of(&queue));
  platform.set_irql(Irql::passive);
  /* The bind at DISPATCH, and the read above it with the lock it takes. */
  while (platform.claim(Violation::irql_ceiling)) {}
  CHECK_EQ(platform.unclaimed_violations(), 0u); }
