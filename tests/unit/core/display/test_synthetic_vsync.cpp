#include "core/ddi/entry_points.hpp"
#include "core/display/display_sources.hpp"
#include "core/display/synthetic_vsync.hpp"
#include "core/miniport/dxgkrnl_interface.hpp"
#include "core/support/status.hpp"
#include "harness/device/fake_virtio_gpu_function.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>
#include <cstring>

using av::DisplaySources;
using av::DxgkrnlInterface;
using av::FakeDxgkrnlAdapter;
using av::FakeVirtioGpuFunction;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::SyntheticVsync;
using av::Violation;

namespace {

constexpr std::uint64_t frame = 10000000u / 60u;
constexpr std::int32_t  crtc_vsync = 3;
constexpr std::uint32_t no_memory = 0xC0000017u;

class Fixture {
public:
    Fixture() : dxgkrnl_(platform_, function_), interface_(platform_), sources_(platform_),
        vsync_(platform_, interface_, sources_)
    {
        if (interface_.initialize(&dxgkrnl_.record()) != Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "interface");
    }

    MockPlatform       &platform() { return platform_; }
    FakeDxgkrnlAdapter &dxgkrnl() { return dxgkrnl_; }
    DisplaySources     &sources() { return sources_; }
    SyntheticVsync     &vsync() { return vsync_; }

private:
    MockPlatform          platform_;
    FakeVirtioGpuFunction function_;
    FakeDxgkrnlAdapter    dxgkrnl_;
    DxgkrnlInterface      interface_;
    DisplaySources        sources_;
    SyntheticVsync        vsync_;
};

}  // namespace

TEST(synthetic_vsync_enable, reports_crtc_vsync_with_the_scanout_every_frame)
{ Fixture fixture;
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  CHECK_STATUS(fixture.sources().set_scanout(0u, 0x40000000, nullptr), Status::ok);
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  fixture.platform().run_timers(frame * 3u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 3u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(2u).InterruptType, crtc_vsync);
  CHECK_EQ(fixture.dxgkrnl().interrupt(2u).FenceId, 0u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(2u).Argument2, 0x40000000u);
  CHECK_EQ(fixture.dxgkrnl().dpc_notifications(), 3u);
  CHECK_EQ(fixture.platform().unclaimed_violations(), 0u); }

TEST(synthetic_vsync_disable, stops_the_reports)
{ Fixture fixture;
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  fixture.platform().run_timers(frame);
  fixture.vsync().disable();
  fixture.platform().run_timers(frame * 10u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u);
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  fixture.platform().run_timers(frame);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 2u); }

TEST(synthetic_vsync_enable, needs_the_timer)
{ Fixture fixture;
  CHECK_STATUS(fixture.vsync().enable(), Status::invalid_state);
  fixture.vsync().disable();
  fixture.platform().fail_allocation_at(1u);
  CHECK_STATUS(fixture.vsync().initialize(), Status::no_memory);
  CHECK_STATUS(fixture.vsync().enable(), Status::invalid_state); }

TEST(synthetic_vsync_destroy, no_tick_follows)
{ Fixture fixture;
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  fixture.vsync().destroy();
  fixture.platform().run_timers(frame * 5u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 0u);
  fixture.vsync().destroy();
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok); }

TEST(synthetic_vsync_tick, a_refused_report_asks_for_no_dpc)
{ Fixture fixture;
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  fixture.dxgkrnl().refuse_synchronize(no_memory);
  fixture.platform().run_timers(frame);
  CHECK_EQ(fixture.dxgkrnl().dpc_notifications(), 0u);
  CHECK(fixture.platform().logged() != 0u); }

TEST(synthetic_vsync_tick, the_timer_entry_point_reaches_it)
{ Fixture fixture;
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  fixture.platform().set_irql(Irql::dispatch);
  AvVsyncTick(nullptr, &fixture.vsync());
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u); }

/* The timer keeps its phase while dxgkrnl has the blanks off, and a tick then
 * reports nothing. */
TEST(synthetic_vsync_disable, leaves_the_timer_running)
{ Fixture fixture;
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  fixture.vsync().disable();
  fixture.platform().run_timers(frame * 4u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 0u);
  CHECK_EQ(fixture.platform().logged(), 0u);
  /* Enabling again does not re-arm it: the next blank is the timer's own. */
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  fixture.platform().run_timers(frame);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u); }

TEST(synthetic_vsync, ceilings)
{ Fixture fixture;
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.vsync().initialize());
  static_cast<void>(fixture.vsync().enable());
  fixture.vsync().disable();
  fixture.vsync().destroy();
  fixture.dxgkrnl().refuse_synchronize(no_memory);
  fixture.platform().set_irql(Irql::isr);
  fixture.vsync().tick();
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  while (fixture.platform().claim(Violation::allocation_at_dispatch)) {} }

/* dxgkrnl turns vertical blanks off when nothing waits and on again when
 * something does, and each turn on costs a period: what the driver can see is
 * how many times it was asked. */
TEST(synthetic_vsync_enabled_count, counts_what_dxgkrnl_asked_for)
{ Fixture fixture;
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  CHECK_EQ(fixture.vsync().enabled_count(), 0u);
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  fixture.vsync().disable();
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  CHECK_EQ(fixture.vsync().enabled_count(), 2u); }

/* Only the driver can see the period its own timer keeps, so it says so every
 * two seconds of blanks. */
TEST(synthetic_vsync_reported, says_the_rate_it_kept_every_window)
{ Fixture fixture;
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  CHECK_EQ(fixture.vsync().reported(), 0u);
  fixture.platform().run_timers(frame * 119u);
  CHECK_EQ(fixture.vsync().reported(), 119u);
  CHECK_EQ(fixture.platform().logged(), 0u);
  fixture.platform().run_timers(frame);
  CHECK_EQ(fixture.vsync().reported(), 0u);
  CHECK(std::strstr(fixture.platform().last_log(), "vsync 120 blanks in") != nullptr);
  CHECK(std::strstr(fixture.platform().last_log(), "enabled 1 times") != nullptr); }
