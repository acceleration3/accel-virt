#include "../../src/core/ddi/dxgkargcb_notify_interrupt_data_record.hpp"
#include "../../src/core/ddi/entry_points.hpp"
#include "../../src/core/dxgkrnl_interface.hpp"
#include "../../src/core/interrupt_notification.hpp"
#include "../../src/core/status.hpp"
#include "../harness/fake_dxgkrnl_adapter.hpp"
#include "../harness/fake_virtio_gpu_function.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"
#include "../harness/violation.hpp"

using av::DxgkrnlInterface;
using av::FakeDxgkrnlAdapter;
using av::FakeVirtioGpuFunction;
using av::InterruptNotification;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;

namespace {

constexpr std::int32_t dma_completed = 1, dma_preempted = 2;

class Fixture {
public:
    Fixture() : dxgkrnl_(platform_, function_), interface_(platform_), notification_(platform_, interface_)
    {
        if (interface_.initialize(&dxgkrnl_.record()) != Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "interface");
    }

    MockPlatform          &platform() { return platform_; }
    FakeDxgkrnlAdapter    &dxgkrnl() { return dxgkrnl_; }
    InterruptNotification &notification() { return notification_; }

private:
    MockPlatform          platform_;
    FakeVirtioGpuFunction function_;
    FakeDxgkrnlAdapter    dxgkrnl_;
    DxgkrnlInterface      interface_;
    InterruptNotification notification_;
};

}  // namespace

TEST(interrupt_notification_report_completed, notifies_dma_completed_at_device_irql)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.notification().report_completed(7u), Status::ok);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).InterruptType, dma_completed);
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).FenceId, 7u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).Argument1, 0u);   /* node */
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).Argument2, 0u);   /* engine */ }

TEST(interrupt_notification_report_preempted, notifies_the_preemption_and_last_completed_fences)
{ Fixture fixture;
  CHECK_STATUS(fixture.notification().report_preempted(9u, 5u), Status::ok);
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).InterruptType, dma_preempted);
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).FenceId, 9u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).Argument1, 5u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).Argument2, 0u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).Argument3, 0u); }

TEST(interrupt_notification_report_completed, a_refused_synchronization_is_returned)
{ Fixture fixture;
  fixture.dxgkrnl().refuse_synchronize(0xC0000017u);
  CHECK_STATUS(fixture.notification().report_completed(1u), Status::no_memory);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 0u); }

TEST(interrupt_notification_notify, is_the_synchronized_routine_and_needs_device_irql_at_most)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::isr);
  CHECK_EQ(AvSynchronizedNotify(&fixture.notification()), 1u);
  CHECK_EQ(fixture.notification().notify(), 1u);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 2u);
  CHECK_EQ(fixture.platform().unclaimed_violations(), 0u); }

TEST(interrupt_notification, reports_are_dispatch_at_most)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::isr);
  static_cast<void>(fixture.notification().report_completed(1u));
  static_cast<void>(fixture.notification().report_preempted(1u, 0u));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {} }

TEST(interrupt_notification_report_vsync, notifies_crtc_vsync_with_the_target_and_address)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  CHECK_STATUS(fixture.notification().report_vsync(0u, 0x0000000123456000ull), Status::ok);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u);
  const av::DxgkargcbNotifyInterruptDataRecord &data = fixture.dxgkrnl().interrupt(0u);
  CHECK_EQ(data.InterruptType, 3);   /* DXGK_INTERRUPT_CRTC_VSYNC */
  CHECK_EQ(data.FenceId, 0u);        /* VidPnTargetId */
  CHECK_EQ(data.Argument2, 0x23456000u);
  CHECK_EQ(data.Argument3, 0x1u);
  CHECK_EQ(data.UnionRemainder[0], 1u);   /* PhysicalAdapterMask */
  CHECK_EQ(data.Flags, 0u);
  fixture.platform().set_irql(Irql::isr);
  static_cast<void>(fixture.notification().report_vsync(0u, 0u));
  fixture.platform().set_irql(Irql::passive);
  while (fixture.platform().claim(Violation::irql_ceiling)) {} }
