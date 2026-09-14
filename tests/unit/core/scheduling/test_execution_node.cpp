#include "core/miniport/dxgkrnl_interface.hpp"
#include "core/miniport/interrupt_notification.hpp"
#include "core/scheduling/execution_node.hpp"
#include "core/support/status.hpp"
#include "harness/device/fake_virtio_gpu_function.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>

namespace av {

struct ExecutionNodeProbe {
    explicit ExecutionNodeProbe(ExecutionNode &target) : node(target) {}
    void set_last_completed(std::uint32_t fence) { node.last_completed_ = fence; }
    void set_count(std::uint32_t count) { node.count_ = count; }
    void set_last_submitted(std::uint32_t fence) { node.last_submitted_ = fence; }
    void set_pending(std::uint32_t index, std::uint32_t fence) { node.pending_[index] = fence; }
    /* A packet waiting on a host fence with nobody to say it finished. */
    void orphan_awaited(std::uint32_t index) { node.owners_[index] = nullptr; }
    ExecutionNode &node;
};

}  // namespace av

using av::DxgkrnlInterface;
using av::ExecutionNode;
using av::ExecutionNodeProbe;
using av::FakeDxgkrnlAdapter;
using av::FakeVirtioGpuFunction;
using av::InterruptNotification;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;

namespace {

class Fixture {
public:
    Fixture() : dxgkrnl_(platform_, function_), interface_(platform_), notification_(platform_, interface_), node_(platform_)
    {
        if (interface_.initialize(&dxgkrnl_.record()) != Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "interface");
        platform_.set_irql(Irql::dispatch);
    }
    ~Fixture() { platform_.set_irql(Irql::passive); }

    MockPlatform          &platform() { return platform_; }
    FakeDxgkrnlAdapter    &dxgkrnl() { return dxgkrnl_; }
    InterruptNotification &notification() { return notification_; }
    ExecutionNode         &node() { return node_; }

private:
    MockPlatform          platform_;
    FakeVirtioGpuFunction function_;
    FakeDxgkrnlAdapter    dxgkrnl_;
    DxgkrnlInterface      interface_;
    InterruptNotification notification_;
    ExecutionNode         node_;
};

}  // namespace

TEST(execution_node_submit, queues_increasing_fences)
{ Fixture fixture;
  CHECK_STATUS(fixture.node().submit(1u), Status::ok);
  CHECK(fixture.node().invariants_hold());
  CHECK_STATUS(fixture.node().submit(3u), Status::ok);
  CHECK(fixture.node().invariants_hold());
  CHECK_EQ(fixture.node().pending(), 2u);
  CHECK_EQ(fixture.node().current_fence(), 0u); }

TEST(execution_node_submit, refuses_a_fence_not_above_the_last_or_a_full_queue)
{ Fixture fixture;
  CHECK_STATUS(fixture.node().submit(0u), Status::invalid_parameter);
  CHECK_STATUS(fixture.node().submit(5u), Status::ok);
  CHECK_STATUS(fixture.node().submit(5u), Status::invalid_parameter);
  CHECK_STATUS(fixture.node().submit(4u), Status::invalid_parameter);
  for (std::uint32_t fence = 6u; fence < 5u + ExecutionNode::capacity; fence++)
      CHECK_STATUS(fixture.node().submit(fence), Status::ok);
  CHECK_STATUS(fixture.node().submit(5u + ExecutionNode::capacity), Status::invalid_parameter);
  CHECK_EQ(fixture.node().pending(), ExecutionNode::capacity);
  CHECK(fixture.node().invariants_hold()); }

TEST(execution_node_complete, reports_every_pending_fence_in_order_then_the_current_fence_moves)
{ Fixture fixture;
  CHECK_STATUS(fixture.node().submit(1u), Status::ok);
  CHECK_STATUS(fixture.node().submit(2u), Status::ok);
  CHECK_STATUS(fixture.node().submit(4u), Status::ok);
  CHECK_STATUS(fixture.node().complete(fixture.notification()), Status::ok);
  CHECK(fixture.node().invariants_hold());
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 3u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(2u).FenceId, 4u);
  CHECK_EQ(fixture.node().current_fence(), 4u);
  CHECK_EQ(fixture.node().pending(), 0u);
  CHECK_STATUS(fixture.node().complete(fixture.notification()), Status::ok);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 3u); }

/* Feature 043: an rvk submission is finished by the host, not by the DPC. It
 * waits at the head of the node until its context says the host completed the
 * wire fence it carried, and everything queued behind it waits too - dxgkrnl
 * takes a completed fence as the completion of every lower one on the node. */
TEST(execution_node_complete, a_packet_awaiting_the_host_holds_the_node_until_its_owner_finishes_it)
{ Fixture fixture;
  int first = 0;
  int second = 0;
  CHECK_STATUS(fixture.node().submit(1u, &first, 7u), Status::ok);
  CHECK(fixture.node().invariants_hold());
  CHECK_STATUS(fixture.node().submit(2u, &second, 3u), Status::ok);
  CHECK_STATUS(fixture.node().submit(3u), Status::ok);   /* a plain packet behind them */
  CHECK_STATUS(fixture.node().complete(fixture.notification()), Status::ok);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 0u);
  CHECK_EQ(fixture.node().current_fence(), 0u);

  fixture.node().finish(&second, 3u);   /* the second is done; the first still holds the node */
  CHECK(fixture.node().invariants_hold());
  CHECK_STATUS(fixture.node().complete(fixture.notification()), Status::ok);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 0u);

  fixture.node().finish(&first, 6u);    /* not yet the fence it waits for */
  CHECK_STATUS(fixture.node().complete(fixture.notification()), Status::ok);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 0u);

  fixture.node().finish(&first, 7u);
  CHECK(fixture.node().invariants_hold());
  CHECK_STATUS(fixture.node().complete(fixture.notification()), Status::ok);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 3u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(2u).FenceId, 3u);
  CHECK_EQ(fixture.node().pending(), 0u); }

TEST(execution_node_finish, only_the_owner_s_packets_are_finished)
{ Fixture fixture;
  int owner = 0;
  int stranger = 0;
  CHECK_STATUS(fixture.node().submit(4u, nullptr, 2u), Status::invalid_parameter);   /* nobody could finish it */
  CHECK_STATUS(fixture.node().submit(4u, &owner, 0u), Status::invalid_parameter);    /* no host fence is 0 */
  CHECK_EQ(fixture.node().pending(), 0u);
  CHECK_STATUS(fixture.node().submit(4u, &owner, 2u), Status::ok);
  fixture.node().finish(&stranger, 100u);
  fixture.node().finish(nullptr, 100u);
  CHECK_STATUS(fixture.node().complete(fixture.notification()), Status::ok);
  CHECK_EQ(fixture.node().pending(), 1u);
  void *aborted_owner = nullptr;
  CHECK_EQ(fixture.node().abort_pending(aborted_owner), 4u);
  CHECK(aborted_owner == &owner);
  CHECK(fixture.node().invariants_hold()); }

TEST(execution_node_complete, a_full_queue_wraps_and_empties)
{ Fixture fixture;
  for (std::uint32_t round = 0u; round < 3u; round++) {
      for (std::uint32_t i = 1u; i <= ExecutionNode::capacity - 1u; i++)
          CHECK_STATUS(fixture.node().submit(round * ExecutionNode::capacity + i), Status::ok);
      CHECK_STATUS(fixture.node().complete(fixture.notification()), Status::ok);
      CHECK(fixture.node().invariants_hold());
  }
  CHECK_EQ(fixture.node().current_fence(), 3u * ExecutionNode::capacity - 1u); }

TEST(execution_node_preempt, the_next_completion_reports_the_preemption_after_the_pending_packets)
{ Fixture fixture;
  CHECK_STATUS(fixture.node().submit(1u), Status::ok);
  CHECK_STATUS(fixture.node().preempt(8u), Status::ok);
  CHECK_STATUS(fixture.node().preempt(9u), Status::ok);
  CHECK(fixture.node().invariants_hold());
  CHECK_STATUS(fixture.node().complete(fixture.notification()), Status::ok);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 2u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).InterruptType, 1);
  CHECK_EQ(fixture.dxgkrnl().interrupt(1u).InterruptType, 2);
  CHECK_EQ(fixture.dxgkrnl().interrupt(1u).FenceId, 9u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(1u).Argument1, 1u);
  CHECK_STATUS(fixture.node().complete(fixture.notification()), Status::ok);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 2u); }

TEST(execution_node_complete, stops_at_the_first_refused_report)
{ Fixture fixture;
  CHECK_STATUS(fixture.node().submit(1u), Status::ok);
  CHECK_STATUS(fixture.node().preempt(2u), Status::ok);
  fixture.dxgkrnl().refuse_synchronize(0xC0000017u);
  CHECK_STATUS(fixture.node().complete(fixture.notification()), Status::no_memory);
  CHECK_EQ(fixture.node().current_fence(), 1u);
  CHECK(fixture.node().invariants_hold()); }

TEST(execution_node, is_dispatch_at_most_and_allocates_nothing)
{ Fixture fixture;
  const std::uint32_t allocations = fixture.platform().allocations_made();
  fixture.platform().set_irql(Irql::isr);
  static_cast<void>(fixture.node().submit(1u));
  static_cast<void>(fixture.node().preempt(1u));
  static_cast<void>(fixture.node().current_fence());
  static_cast<void>(fixture.node().complete(fixture.notification()));
  fixture.platform().set_irql(Irql::dispatch);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  while (fixture.platform().claim(Violation::lock_order)) {}
  CHECK_EQ(fixture.platform().allocations_made(), allocations); }

TEST(execution_node_invariants, detect_a_regressed_or_unordered_queue)
{ int owner = 0;
  for (int knob = 0; knob < 6; knob++) {
      Fixture fixture;
      CHECK_STATUS(fixture.node().submit(2u), Status::ok);
      CHECK_STATUS(fixture.node().submit(3u, &owner, 1u), Status::ok);
      ExecutionNodeProbe probe(fixture.node());
      if (knob == 0) probe.set_last_completed(4u);                      /* completed past submitted */
      if (knob == 1) probe.set_count(ExecutionNode::capacity + 1u);
      if (knob == 2) probe.set_pending(1u, 2u);                         /* not increasing */
      if (knob == 3) probe.set_last_completed(2u);                      /* a pending fence already completed */
      if (knob == 4) probe.set_last_submitted(9u);                      /* a submission missing from the queue */
      if (knob == 5) probe.orphan_awaited(1u);                          /* awaited, and nobody can finish it */
      CHECK(!fixture.node().invariants_hold());
  } }

/* ---- Reset and restart (feature 010) ------------------------------------------ */

TEST(execution_node_reset, drops_the_queue_silently_and_refuses_submissions_until_restart)
{ Fixture fixture;
  CHECK_STATUS(fixture.node().submit(1u), Status::ok);
  CHECK_STATUS(fixture.node().submit(2u), Status::ok);
  CHECK_STATUS(fixture.node().preempt(7u), Status::ok);
  fixture.node().reset();
  CHECK(fixture.node().invariants_hold());
  CHECK_EQ(fixture.node().pending(), 0u);
  CHECK_STATUS(fixture.node().submit(3u), Status::invalid_state);
  CHECK_STATUS(fixture.node().complete(fixture.notification()), Status::ok);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 0u);
  fixture.node().restart();
  CHECK(fixture.node().invariants_hold());
  CHECK_STATUS(fixture.node().submit(2u), Status::invalid_parameter);
  CHECK_STATUS(fixture.node().submit(3u), Status::ok);
  CHECK_STATUS(fixture.node().complete(fixture.notification()), Status::ok);
  CHECK_EQ(fixture.node().current_fence(), 3u);
  CHECK_EQ(fixture.node().last_submitted(), 3u); }

TEST(execution_node_abort_pending, drops_the_queue_and_names_what_was_executing)
{ Fixture fixture;
  CHECK_STATUS(fixture.node().submit(1u), Status::ok);
  CHECK_STATUS(fixture.node().complete(fixture.notification()), Status::ok);
  CHECK_EQ(fixture.node().abort_pending(), 1u);
  CHECK_STATUS(fixture.node().submit(2u), Status::ok);
  CHECK_STATUS(fixture.node().submit(3u), Status::ok);
  CHECK_EQ(fixture.node().abort_pending(), 2u);
  CHECK(fixture.node().invariants_hold());
  CHECK_EQ(fixture.node().pending(), 0u);
  CHECK_EQ(fixture.node().current_fence(), 1u);
  CHECK_STATUS(fixture.node().submit(4u), Status::ok);
  CHECK_STATUS(fixture.node().complete(fixture.notification()), Status::ok);
  CHECK_EQ(fixture.node().current_fence(), 4u);
  fixture.platform().set_irql(Irql::isr);
  static_cast<void>(fixture.node().abort_pending());
  fixture.platform().set_irql(Irql::dispatch);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {} }

TEST(execution_node_progress_time, moves_on_a_submission_to_an_idle_queue_and_on_each_completion)
{ Fixture fixture;
  CHECK(fixture.node().progress_time() == 0u);
  fixture.platform().advance_time(100u);
  CHECK_STATUS(fixture.node().submit(1u), Status::ok);
  CHECK(fixture.node().progress_time() == 100u);
  fixture.platform().advance_time(50u);
  CHECK_STATUS(fixture.node().submit(2u), Status::ok);
  CHECK(fixture.node().progress_time() == 100u);
  fixture.platform().advance_time(25u);
  CHECK_STATUS(fixture.node().complete(fixture.notification()), Status::ok);
  CHECK(fixture.node().progress_time() == 175u); }

TEST(execution_node_reset, is_dispatch_at_most)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::isr);
  fixture.node().reset();
  fixture.node().restart();
  static_cast<void>(fixture.node().last_submitted());
  static_cast<void>(fixture.node().progress_time());
  fixture.platform().set_irql(Irql::dispatch);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  while (fixture.platform().claim(Violation::lock_order)) {} }

TEST(execution_node_invariants, a_stopped_node_with_pending_packets_is_detected)
{ Fixture fixture;
  fixture.node().reset();
  ExecutionNodeProbe probe(fixture.node());
  probe.set_last_submitted(1u);
  probe.set_pending(0u, 1u);
  probe.set_count(1u);
  CHECK(!fixture.node().invariants_hold()); }
