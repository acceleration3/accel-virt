#include "core/miniport/dxgkrnl_interface.hpp"
#include "core/miniport/interrupt_notification.hpp"
#include "core/scheduling/hw_queue_submissions.hpp"
#include "core/support/status.hpp"
#include "harness/device/fake_virtio_gpu_function.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>

using av::DxgkrnlInterface;
using av::FakeDxgkrnlAdapter;
using av::FakeVirtioGpuFunction;
using av::HwQueueSubmissions;
using av::InterruptNotification;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;

namespace av {

struct HwQueueSubmissionsProbe {
    static void overfill(HwQueueSubmissions &submissions) { submissions.count_ = HwQueueSubmissions::capacity + 1u; }
};

}  // namespace av

namespace {

int queue_a = 0;
int queue_b = 0;

class Fixture {
public:
    Fixture()
        : fence_a_(0u), fence_b_(0u), dxgkrnl_(platform_, function_), interface_(platform_),
          notification_(platform_, interface_), submissions_(platform_)
    {
        if (interface_.initialize(&dxgkrnl_.record()) != Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "interface");
    }
    ~Fixture() { platform_.set_irql(Irql::passive); }

    MockPlatform          &platform() { return platform_; }
    FakeDxgkrnlAdapter    &dxgkrnl() { return dxgkrnl_; }
    InterruptNotification &notification() { return notification_; }
    HwQueueSubmissions    &submissions() { return submissions_; }
    std::uint64_t          fence_a_, fence_b_;

private:
    MockPlatform          platform_;
    FakeVirtioGpuFunction function_;
    FakeDxgkrnlAdapter    dxgkrnl_;
    DxgkrnlInterface      interface_;
    InterruptNotification notification_;
    HwQueueSubmissions    submissions_;
};

}  // namespace

TEST(hw_queue_submissions_complete, writes_each_fence_in_order_then_signals_once)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.submissions().submit(&queue_a, 1u, &fixture.fence_a_), Status::ok);
  CHECK_STATUS(fixture.submissions().submit(&queue_b, 7u, &fixture.fence_b_), Status::ok);
  CHECK_STATUS(fixture.submissions().submit(&queue_a, 2u, &fixture.fence_a_), Status::ok);
  CHECK(fixture.submissions().invariants_hold());
  CHECK_EQ(fixture.submissions().in_flight(), 3u);
  CHECK_EQ(fixture.fence_a_, 0u);
  CHECK_STATUS(fixture.submissions().complete(fixture.notification()), Status::ok);
  CHECK(fixture.submissions().invariants_hold());
  CHECK_EQ(fixture.fence_a_, 2u);
  CHECK_EQ(fixture.fence_b_, 7u);
  CHECK_EQ(fixture.submissions().in_flight(), 0u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).InterruptType, 11);
  CHECK_STATUS(fixture.submissions().complete(fixture.notification()), Status::ok);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u);   /* nothing written, nothing signalled */ }

TEST(hw_queue_submissions_submit, refuses_no_queue_no_fence_or_a_full_set)
{ Fixture fixture;
  CHECK_STATUS(fixture.submissions().submit(nullptr, 1u, &fixture.fence_a_), Status::invalid_parameter);
  CHECK_STATUS(fixture.submissions().submit(&queue_a, 1u, nullptr), Status::invalid_parameter);
  for (std::uint32_t fence = 1u; fence <= HwQueueSubmissions::capacity; fence++)
      CHECK_STATUS(fixture.submissions().submit(&queue_a, fence, &fixture.fence_a_), Status::ok);
  CHECK_STATUS(fixture.submissions().submit(&queue_a, 65u, &fixture.fence_a_), Status::invalid_parameter);
  CHECK_EQ(fixture.submissions().in_flight(), HwQueueSubmissions::capacity); }

TEST(hw_queue_submissions_forget, leaves_other_queues_in_flight_in_order)
{ Fixture fixture;
  CHECK_STATUS(fixture.submissions().submit(&queue_a, 1u, &fixture.fence_a_), Status::ok);
  CHECK_STATUS(fixture.submissions().submit(&queue_b, 4u, &fixture.fence_b_), Status::ok);
  CHECK_STATUS(fixture.submissions().submit(&queue_a, 2u, &fixture.fence_a_), Status::ok);
  CHECK_STATUS(fixture.submissions().submit(&queue_b, 5u, &fixture.fence_b_), Status::ok);
  fixture.submissions().forget(&queue_a);
  CHECK(fixture.submissions().invariants_hold());
  CHECK_EQ(fixture.submissions().in_flight(), 2u);
  CHECK_STATUS(fixture.submissions().complete(fixture.notification()), Status::ok);
  CHECK_EQ(fixture.fence_a_, 0u);
  CHECK_EQ(fixture.fence_b_, 5u); }

TEST(hw_queue_submissions_abandon, drops_everything_unwritten)
{ Fixture fixture;
  CHECK_STATUS(fixture.submissions().submit(&queue_a, 1u, &fixture.fence_a_), Status::ok);
  fixture.submissions().abandon();
  CHECK_STATUS(fixture.submissions().complete(fixture.notification()), Status::ok);
  CHECK_EQ(fixture.fence_a_, 0u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 0u); }

TEST(hw_queue_submissions_complete, a_refused_signal_is_answered)
{ Fixture fixture;
  CHECK_STATUS(fixture.submissions().submit(&queue_a, 1u, &fixture.fence_a_), Status::ok);
  fixture.dxgkrnl().refuse_synchronize(0xC0000017u);
  CHECK(fixture.submissions().complete(fixture.notification()) != Status::ok);
  CHECK_EQ(fixture.fence_a_, 1u); }

TEST(hw_queue_submissions_invariants, too_many_in_flight_is_detected)
{ Fixture fixture;
  av::HwQueueSubmissionsProbe::overfill(fixture.submissions());
  CHECK(!fixture.submissions().invariants_hold()); }

TEST(hw_queue_submissions, is_legal_up_to_dispatch)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.submissions().submit(nullptr, 0u, nullptr));
  fixture.submissions().forget(nullptr);
  fixture.submissions().abandon();
  static_cast<void>(fixture.submissions().in_flight());
  static_cast<void>(fixture.submissions().complete(fixture.notification()));
  CHECK_EQ(fixture.platform().unclaimed_violations(), 0u);
  fixture.platform().set_irql(Irql::isr);
  static_cast<void>(fixture.submissions().in_flight());
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {} }
