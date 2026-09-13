/* A context nobody can finish, and whether its owner has been told. A stalled
 * or faulted context is lost alone: the other process keeps completing work,
 * and what this records is who to tell (feature 040, US3). */
#include "core/miniport/dxgkrnl_interface.hpp"
#include "core/miniport/interrupt_notification.hpp"
#include "core/scheduling/context_loss.hpp"
#include "core/support/status.hpp"
#include "harness/device/fake_virtio_gpu_function.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>
#include <cstring>

using av::ContextLoss;
using av::DxgkrnlInterface;
using av::FakeDxgkrnlAdapter;
using av::FakeVirtioGpuFunction;
using av::InterruptNotification;
using av::Irql;
using av::MockPlatform;
using av::Status;

namespace {

/* The handle dxgkrnl would have given a context: an identity, not an address
 * anything dereferences. */
void *handle(std::uint32_t id)
{
    return reinterpret_cast<void *>(static_cast<std::uintptr_t>(id) + 0x1000u);
}

class Fixture {
public:
    Fixture() : dxgkrnl_(platform_, function_), interface_(platform_), notification_(platform_, interface_),
                losses_(platform_)
    {
        if (interface_.initialize(&dxgkrnl_.record()) != Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "interface");
    }

    MockPlatform          &platform() { return platform_; }
    FakeDxgkrnlAdapter    &dxgkrnl() { return dxgkrnl_; }
    InterruptNotification &notification() { return notification_; }
    ContextLoss           &losses() { return losses_; }

private:
    MockPlatform          platform_;
    FakeVirtioGpuFunction function_;
    FakeDxgkrnlAdapter    dxgkrnl_;
    DxgkrnlInterface      interface_;
    InterruptNotification notification_;
    ContextLoss           losses_;
};

}  // namespace

TEST(context_loss_lose, records_a_context_and_says_so_once)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  fixture.losses().lose(handle(7u), 3u, Status::not_found);
  CHECK_EQ(fixture.losses().pending(), 1u);
  CHECK_EQ(fixture.platform().logged(), 1u);
  /* The same context lost twice is one loss, and one line. */
  fixture.losses().lose(handle(7u), 4u, Status::invalid_state);
  CHECK_EQ(fixture.losses().pending(), 1u);
  CHECK_EQ(fixture.platform().logged(), 1u);
  fixture.losses().lose(handle(9u), 5u, Status::not_found);
  CHECK_EQ(fixture.losses().pending(), 2u);
  CHECK_EQ(fixture.platform().logged(), 2u);
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.platform().unclaimed_violations(), 0u); }

TEST(context_loss_report, tells_the_owner_of_every_loss_once)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  fixture.losses().lose(handle(1u), 11u, Status::not_found);
  fixture.losses().lose(handle(2u), 22u, Status::invalid_state);
  CHECK_STATUS(fixture.losses().report(fixture.notification()), Status::ok);
  CHECK_EQ(fixture.losses().pending(), 0u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 2u);
  /* Nothing new to say: nobody is told again. */
  CHECK_STATUS(fixture.losses().report(fixture.notification()), Status::ok);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 2u);
  fixture.platform().set_irql(Irql::passive); }

/* A report dxgkrnl refuses leaves the loss pending: the owner has not been
 * told, and saying so later is the whole point of keeping it. */
TEST(context_loss_report, a_refused_report_leaves_the_loss_to_tell)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  fixture.losses().lose(handle(4u), 44u, Status::not_found);
  fixture.dxgkrnl().refuse_synchronize(0xC0000001u);
  CHECK(fixture.losses().report(fixture.notification()) != Status::ok);
  CHECK_EQ(fixture.losses().pending(), 1u);
  fixture.dxgkrnl().refuse_synchronize(0u);
  CHECK_STATUS(fixture.losses().report(fixture.notification()), Status::ok);
  CHECK_EQ(fixture.losses().pending(), 0u);
  fixture.platform().set_irql(Irql::passive); }

TEST(context_loss_lose, a_table_full_of_losses_keeps_the_first_ones)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  for (std::uint32_t context = 0u; context < ContextLoss::max_lost + 4u; context++)
      fixture.losses().lose(handle(context), context, Status::not_found);
  CHECK_EQ(fixture.losses().pending(), ContextLoss::max_lost);
  CHECK_STATUS(fixture.losses().report(fixture.notification()), Status::ok);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), ContextLoss::max_lost);
  fixture.platform().set_irql(Irql::passive); }
