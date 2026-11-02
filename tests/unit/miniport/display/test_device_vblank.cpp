#include "miniport/ddi/dxgk_multiplane_overlay_plane_record.hpp"
#include "miniport/display/device_vblank.hpp"
#include "miniport/display/display_mode.hpp"
#include "miniport/display/display_sources.hpp"
#include "miniport/driver/dxgkrnl_interface.hpp"
#include "core/support/status.hpp"
#include "harness/virtio/fake_virtio_gpu_function.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/pages/clock_page_header.hpp"
#include "proto/pages/vblank_record.hpp"

#include <cstdint>
#include <cstring>

using av::DeviceVblank;
using av::DisplaySources;
using av::DxgkrnlInterface;
using av::FakeDxgkrnlAdapter;
using av::FakeVirtioGpuFunction;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::VblankRecord;

namespace {

constexpr std::int32_t  crtc_vsync = 3;
constexpr std::int32_t  crtc_vsync_multiplane = 7;
constexpr std::uint32_t no_memory = 0xC0000017u;

class Fixture {
public:
    Fixture() : dxgkrnl_(platform_, function_), interface_(platform_), sources_(platform_),
        vsync_(platform_, interface_, sources_)
    {
        if (interface_.initialize(&dxgkrnl_.record()) != Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "interface");
    }

    void take_page()
    {
        std::memset(page(), 0, av::ClockPageHeader::page_size);
        static_cast<void>(av::ClockPageHeader(1u).encode(page(), av::ClockPageHeader::encoded_size));
    }
    void blank(std::uint64_t sequence, std::uint32_t latched = 0u, std::uint32_t refresh = 60000u)
    {
        std::uint8_t *const record = page() + 32u;
        std::uint32_t       generation = 0u;
        std::memcpy(&generation, record, sizeof generation);
        generation += 2u;
        static_cast<void>(VblankRecord(latched, sequence, sequence * 16666666u, refresh)
                              .encode(record + VblankRecord::fields_at, VblankRecord::encoded_size));
        std::memcpy(record, &generation, sizeof generation);
    }
    std::uint32_t interrupt_every()
    {
        std::uint32_t every = 0u;
        std::memcpy(&every, page() + 32u + VblankRecord::interrupt_every_at, sizeof every);
        return every;
    }
    void tick(std::uint64_t now, bool latching = false)
    {
        platform_.set_irql(Irql::dispatch);
        vsync_.tick(now, latching);
        platform_.set_irql(Irql::passive);
    }

    MockPlatform       &platform() { return platform_; }
    FakeDxgkrnlAdapter &dxgkrnl() { return dxgkrnl_; }
    DisplaySources     &sources() { return sources_; }
    DeviceVblank       &vsync() { return vsync_; }

private:
    std::uint8_t *page()
    {
        return static_cast<std::uint8_t *>(platform_.translate_physical(platform_.last_contiguous_address(),
                                                                        av::ClockPageHeader::page_size));
    }

    MockPlatform          platform_;
    FakeVirtioGpuFunction function_;
    FakeDxgkrnlAdapter    dxgkrnl_;
    DxgkrnlInterface      interface_;
    DisplaySources        sources_;
    DeviceVblank          vsync_;
};

}

TEST(device_vblank_enable, each_blank_is_reported_once_while_asked_and_not_before)
{ Fixture fixture;
  CHECK_STATUS(fixture.vsync().enable(), Status::invalid_state);
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  fixture.take_page();
  CHECK_STATUS(fixture.sources().set_scanout(0u, 0x40000000, nullptr), Status::ok);
  fixture.blank(3u);
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  CHECK_EQ(fixture.interrupt_every(), 1u);
  fixture.tick(100u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 0u);
  fixture.blank(4u);
  fixture.tick(200u);
  fixture.tick(250u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).InterruptType, crtc_vsync);
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).Argument2, 0x40000000u);
  CHECK_EQ(fixture.vsync().reported(), 1u);
  CHECK_EQ(fixture.vsync().enabled_count(), 1u);
  fixture.vsync().disable();
  CHECK_EQ(fixture.interrupt_every(), DeviceVblank::sweep_every);
  fixture.blank(5u);
  fixture.tick(300u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u);
  CHECK(fixture.vsync().invariants_hold()); }

TEST(device_vblank_tick, a_posted_source_reports_the_flip_the_device_latched_and_others_the_newest)
{ Fixture fixture;
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  fixture.take_page();
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  std::uint32_t allocation = 0u;
  av::DxgkMultiplaneOverlayPlaneRecord base = {};
  base.Enabled = 1u;
  base.hAllocation = &allocation;
  base.AllocationAddress = 0x1000ll;
  base.PlaneAttributes[0] = 1u;
  fixture.platform().set_irql(Irql::isr);
  CHECK_STATUS(fixture.sources().set_multiplane_scanout(0u, &base), Status::ok);
  base.AllocationAddress = 0x2000ll;
  base.PlaneAttributes[0] = 2u;
  CHECK_STATUS(fixture.sources().set_multiplane_scanout(0u, &base), Status::ok);
  fixture.platform().set_irql(Irql::passive);
  fixture.blank(1u, 1u);
  fixture.tick(100u, true);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).InterruptType, crtc_vsync_multiplane);
  const auto *const layer = reinterpret_cast<const av::DxgkMultiplaneOverlayVsyncInfoRecord *>(
      static_cast<std::uintptr_t>(fixture.dxgkrnl().interrupt(0u).UnionRemainder[0]) |
      (static_cast<std::uintptr_t>(fixture.dxgkrnl().interrupt(0u).UnionRemainder[1]) << 32u));
  CHECK(layer != nullptr);
  if (layer != nullptr) {
      CHECK_EQ(layer->PhysicalAddress, 0x1000ll);
      CHECK_EQ(layer->PlaneAttributes[0], 1u);
  }
  fixture.blank(2u, 1u);
  fixture.tick(200u, false);
  const auto *const newest = reinterpret_cast<const av::DxgkMultiplaneOverlayVsyncInfoRecord *>(
      static_cast<std::uintptr_t>(fixture.dxgkrnl().interrupt(1u).UnionRemainder[0]) |
      (static_cast<std::uintptr_t>(fixture.dxgkrnl().interrupt(1u).UnionRemainder[1]) << 32u));
  CHECK(newest != nullptr);
  if (newest != nullptr) CHECK_EQ(newest->PhysicalAddress, 0x2000ll);
  fixture.platform().set_irql(Irql::isr);
  CHECK_STATUS(fixture.sources().set_multiplane_scanout(0u, nullptr), Status::ok);
  CHECK_STATUS(fixture.sources().set_scanout(0u, 0x3000, &allocation), Status::ok);
  fixture.platform().set_irql(Irql::passive);
  fixture.blank(3u, 3u);
  fixture.tick(300u, true);
  CHECK_EQ(fixture.dxgkrnl().interrupt(2u).InterruptType, crtc_vsync_multiplane);
  CHECK_EQ(fixture.dxgkrnl().interrupt(2u).Argument2, 0u);
  fixture.blank(4u, 4u);
  fixture.tick(400u, true);
  CHECK_EQ(fixture.dxgkrnl().interrupt(3u).InterruptType, crtc_vsync);
  CHECK_EQ(fixture.dxgkrnl().interrupt(3u).Argument2, 0x3000u); }

TEST(device_vblank_tick, a_refused_report_is_said_and_not_counted)
{ Fixture fixture;
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  fixture.take_page();
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  fixture.dxgkrnl().refuse_synchronize(no_memory);
  fixture.blank(1u);
  fixture.tick(100u);
  CHECK(fixture.platform().logged_line("avgpu: a vsync was refused"));
  CHECK_EQ(fixture.vsync().reported(), 0u); }

TEST(device_vblank_tick, a_page_the_device_never_took_or_wrote_wrong_reports_nothing)
{ Fixture fixture;
  fixture.tick(100u);
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  fixture.tick(200u);
  fixture.take_page();
  fixture.blank(1u, 0u, 999u);
  fixture.tick(300u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 0u);
  fixture.blank(1u);
  fixture.tick(400u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u);
  fixture.blank(1u);
  fixture.tick(500u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u); }

TEST(device_vblank_scan_line, the_line_follows_the_blanks_phase_once_one_is_seen)
{ Fixture fixture;
  const av::DisplayMode mode(1920u, 1080u);
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  fixture.take_page();
  CHECK_EQ(fixture.vsync().scan_line(mode, 5000000u), mode.scan_line(5000000u));
  fixture.blank(10u);
  fixture.tick(1000000u);
  CHECK_EQ(fixture.vsync().scan_line(mode, 1000000u), 1080u);
  CHECK_EQ(fixture.vsync().scan_line(mode, 1000000u + 166666u / 2u), mode.scan_line_within(83333u, 166666u));
  CHECK(fixture.vsync().invariants_hold()); }

TEST(device_vblank_power, nothing_is_reported_across_a_suspend_and_the_asking_comes_back)
{ Fixture fixture;
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  fixture.take_page();
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  fixture.vsync().suspend();
  CHECK(fixture.vsync().invariants_hold());
  fixture.blank(1u);
  fixture.tick(100u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 0u);
  fixture.take_page();
  CHECK_STATUS(fixture.vsync().resume(), Status::ok);
  CHECK_EQ(fixture.interrupt_every(), 1u);
  fixture.blank(2u);
  fixture.tick(200u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u);
  fixture.vsync().disable();
  fixture.vsync().suspend();
  CHECK_STATUS(fixture.vsync().resume(), Status::ok);
  CHECK_EQ(fixture.interrupt_every(), DeviceVblank::sweep_every);
  fixture.vsync().destroy();
  CHECK_STATUS(fixture.vsync().resume(), Status::invalid_state);
  CHECK_EQ(fixture.platform().live_allocations(), 0u); }

TEST(device_vblank_report_rate, every_hundred_and_twenty_blanks_a_line_says_how_the_clock_kept)
{ Fixture fixture;
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  fixture.take_page();
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  fixture.blank(1u);
  fixture.tick(1000u);
  fixture.blank(4u);
  fixture.tick(51000u);
  for (std::uint64_t blank = 5u; blank <= DeviceVblank::blanks_per_report; blank++) {
      fixture.blank(blank);
      fixture.tick(1000u + blank * 166667u);
  }
  CHECK(fixture.platform().logged_line(
      "avgpu: vsync 120 blanks in 20000040 ticks, 2 folded, 118 reported, enabled 1 times"));
  CHECK_EQ(fixture.vsync().reported(), 0u);
  CHECK(fixture.vsync().invariants_hold()); }
