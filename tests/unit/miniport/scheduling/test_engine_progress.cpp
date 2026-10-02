/* What QueryEngineStatus answers, and why it matters: VidSchiCheckNodeTimeout
 * (dxgmms2.sys @1400c9b50) returns "no timeout this round" whenever the driver
 * says responsive, so an answer that is always yes defers a hung host forever. */
#include "miniport/driver/dxgkrnl_interface.hpp"
#include "miniport/driver/interrupt_notification.hpp"
#include "miniport/scheduling/engine_progress.hpp"
#include "miniport/scheduling/execution_node.hpp"
#include "core/support/status.hpp"
#include "harness/virtio/fake_virtio_gpu_function.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>

using av::DxgkrnlInterface;
using av::EngineProgress;
using av::FakeDxgkrnlAdapter;
using av::FakeVirtioGpuFunction;
using av::InterruptNotification;
using av::ExecutionNode;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;

namespace {

/* A node with somewhere to report completions to, which is all this needs from
 * dxgkrnl. */
class Fixture {
public:
    Fixture() : dxgkrnl_(platform_, function_), interface_(platform_), notification_(platform_, interface_),
                node_(platform_), progress_(platform_, node_)
    {
        if (interface_.initialize(&dxgkrnl_.record()) != Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "interface");
    }

    MockPlatform          &platform() { return platform_; }
    InterruptNotification &notification() { return notification_; }
    ExecutionNode         &node() { return node_; }
    EngineProgress        &progress() { return progress_; }

private:
    MockPlatform          platform_;
    FakeVirtioGpuFunction function_;
    FakeDxgkrnlAdapter    dxgkrnl_;
    DxgkrnlInterface      interface_;
    InterruptNotification notification_;
    ExecutionNode         node_;
    EngineProgress        progress_;
};

}  // namespace

TEST(engine_progress_responsive, an_idle_engine_is_not_a_hung_one)
{ Fixture fixture;
  CHECK(fixture.progress().responsive());
  fixture.platform().advance_time(100u);
  CHECK(fixture.progress().responsive()); }

/* The check that asks has just decided the node is late; one deferral is the
 * round trip a slow host may already be inside. */
TEST(engine_progress_responsive, the_first_answer_about_a_busy_engine_is_a_deferral)
{ Fixture fixture;
  CHECK_STATUS(fixture.node().submit(1u), Status::ok);
  CHECK(fixture.progress().responsive());
  fixture.platform().advance_time(100u);
  CHECK(!fixture.progress().responsive()); }

TEST(engine_progress_responsive, an_engine_that_moved_since_the_last_answer_is_responsive)
{ Fixture fixture;
  CHECK_STATUS(fixture.node().submit(1u), Status::ok);
  CHECK(fixture.progress().responsive());
  fixture.platform().advance_time(100u);
  CHECK_STATUS(fixture.node().complete(fixture.notification()), Status::ok);
  CHECK_STATUS(fixture.node().submit(2u), Status::ok);
  CHECK(fixture.progress().responsive());
  fixture.platform().advance_time(100u);
  CHECK(!fixture.progress().responsive()); }

/* Two answers in the same tick cannot both be "moved": a node that completes
 * and resubmits between them has moved once, and the second answer is about
 * what happened since the first. */
TEST(engine_progress_responsive, an_engine_that_stopped_moving_is_admitted)
{ Fixture fixture;
  CHECK_STATUS(fixture.node().submit(1u), Status::ok);
  CHECK(fixture.progress().responsive());
  CHECK(!fixture.progress().responsive());
  CHECK(!fixture.progress().responsive());
  fixture.platform().advance_time(100u);
  CHECK_STATUS(fixture.node().complete(fixture.notification()), Status::ok);
  CHECK_STATUS(fixture.node().submit(2u), Status::ok);
  CHECK(fixture.progress().responsive());
  CHECK(!fixture.progress().responsive()); }

/* The timeout check runs in a DPC inside dxgkrnl's own timer: nothing here may
 * allocate, block or take a mutex. */
TEST(engine_progress_responsive, answers_at_dispatch_without_allocating_or_blocking)
{ Fixture fixture;
  CHECK_STATUS(fixture.node().submit(1u), Status::ok);
  fixture.platform().set_irql(Irql::dispatch);
  CHECK(fixture.progress().responsive());
  CHECK(!fixture.progress().responsive());
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.platform().unclaimed_violations(), 0u); }
