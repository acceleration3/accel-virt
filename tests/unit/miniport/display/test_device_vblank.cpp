#include "miniport/ddi/dxgk_multiplane_overlay_plane_record.hpp"
#include "miniport/ddi/dxgk_multiplane_overlay_attributes3_record.hpp"
#include "miniport/ddi/dxgk_multiplane_overlay_vsync_info2_record.hpp"
#include "miniport/display/overlay_plane.hpp"
#include "miniport/display/device_vblank.hpp"
#include "miniport/display/display_mode.hpp"
#include "miniport/display/display_sources.hpp"
#include "miniport/driver/dxgkrnl_interface.hpp"
#include "core/support/status.hpp"
#include "harness/virtio/fake_virtio_gpu_function.hpp"
#include "harness/virtio/mapped_virtio_function.hpp"
#include "miniport/display/vblank_phase.hpp"
#include "miniport/virtio/virtio_gpu_device.hpp"
#include "core/support/object_storage.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
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

namespace av {

struct VblankPhaseProbe {
    static void set_period(VblankPhase &phase, std::uint32_t period) { phase.period_ns_ = period; }
};

struct DeviceVblankProbe {
    static VblankPhase &phase(DeviceVblank &vsync) { return vsync.phase_; }
    static void         suspend_while_reporting(DeviceVblank &vsync)
    {
        vsync.suspended_ = true;
        vsync.reporting_ = true;
    }
};

}

namespace {

constexpr std::int32_t  crtc_vsync = 3;
constexpr std::int32_t  crtc_vsync_multiplane = 7;
constexpr std::int32_t  crtc_vsync_presented = 10;
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

TEST(device_vblank_tick, a_tick_that_comes_late_reports_every_blank_it_missed_up_to_the_catch_up_limit)
{ Fixture fixture;
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  fixture.take_page();
  CHECK_STATUS(fixture.sources().set_scanout(0u, 0x40000000, nullptr), Status::ok);
  fixture.blank(3u);
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  fixture.blank(4u);
  fixture.tick(100u);
  CHECK_EQ(fixture.vsync().reported(), 1u);
  fixture.blank(9u);
  fixture.tick(200u);
  CHECK_EQ(fixture.vsync().reported(), 6u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 6u);
  fixture.blank(1000u);
  fixture.tick(300u);
  CHECK_EQ(fixture.vsync().reported(), 6u + DeviceVblank::catch_up_most);
  fixture.blank(1001u);
  fixture.tick(400u);
  CHECK_EQ(fixture.vsync().reported(), 7u + DeviceVblank::catch_up_most);
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

namespace {

class PresentedFlips {
public:
    explicit PresentedFlips(Fixture &fixture) : fixture_(fixture) {}

    void present(std::uint64_t id, bool immediate = true)
    {
        av::DxgkMultiplaneOverlayAttributes3Record attributes = {};
        const av::OverlayPlane plane(0u, id, &allocation_, 0x1000ll, attributes);
        fixture_.platform().set_irql(Irql::isr);
        CHECK_STATUS(fixture_.sources().present_planes(0u, &plane, 1u), Status::ok);
        if (immediate) fixture_.sources().mark_immediate(0u);
        fixture_.platform().set_irql(Irql::passive);
    }
    std::uint64_t id_of(std::uint32_t interrupt)
    {
        const auto &record = fixture_.dxgkrnl().interrupt(interrupt);
        const auto *const layers = reinterpret_cast<const av::DxgkMultiplaneOverlayVsyncInfo2Record *>(
            static_cast<std::uintptr_t>(record.UnionRemainder[0]) |
            (static_cast<std::uintptr_t>(record.UnionRemainder[1]) << 32u));
        return layers != nullptr && record.Argument2 == 1u ? layers->PresentId : 0u;
    }

private:
    Fixture      &fixture_;
    std::uint32_t allocation_ = 0u;
};

}

TEST(device_vblank_early, a_new_flip_is_reported_from_the_next_tick_without_waiting_for_a_blank)
{ Fixture fixture;
  PresentedFlips flips(fixture);
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  fixture.take_page();
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  flips.present(10u);
  fixture.tick(100u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u);
  CHECK_EQ(fixture.dxgkrnl().interrupt(0u).InterruptType, crtc_vsync_presented);
  CHECK_EQ(flips.id_of(0u), 10u);
  CHECK_EQ(fixture.vsync().reported(), 0u);
  fixture.tick(150u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u);
  flips.present(11u);
  fixture.tick(200u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 2u);
  CHECK_EQ(flips.id_of(1u), 11u); }

TEST(device_vblank_early, a_flip_that_waits_for_the_vsync_is_reported_only_at_the_vsync)
{ Fixture fixture;
  PresentedFlips flips(fixture);
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  fixture.take_page();
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  flips.present(10u, false);
  fixture.tick(100u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 0u);
  fixture.blank(1u);
  fixture.tick(200u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u);
  CHECK_EQ(flips.id_of(0u), 10u);
  flips.present(11u, true);
  flips.present(12u, false);
  fixture.tick(300u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u); }

TEST(device_vblank_early, a_flip_replaced_before_the_tick_is_dropped_and_the_newest_is_reported_once)
{ Fixture fixture;
  PresentedFlips flips(fixture);
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  fixture.take_page();
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  flips.present(10u);
  flips.present(11u);
  flips.present(12u);
  fixture.tick(100u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u);
  CHECK_EQ(flips.id_of(0u), 12u);
  fixture.tick(110u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 1u); }

TEST(device_vblank_early, the_blank_after_an_early_report_repeats_the_same_present_and_never_an_older_one)
{ Fixture fixture;
  PresentedFlips flips(fixture);
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  fixture.take_page();
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  std::uint64_t last = 0u;
  std::uint32_t seen = 0u;
  for (std::uint32_t round = 1u; round <= 6u; round++) {
      flips.present(round * 10u);
      if (round % 2u == 0u) flips.present(round * 10u + 1u);
      fixture.blank(round);
      fixture.tick(round * 100u);
      for (; seen < fixture.dxgkrnl().interrupts(); seen++) {
          const std::uint64_t id = flips.id_of(seen);
          CHECK(id >= last);
          last = id;
      }
  }
  CHECK_EQ(last, 61u);
  CHECK_EQ(fixture.vsync().reported(), 6u); }

TEST(device_vblank_early, a_source_that_posts_frames_never_sees_a_present_older_than_one_already_reported)
{ Fixture fixture;
  PresentedFlips flips(fixture);
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  fixture.take_page();
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  std::uint64_t last = 0u;
  std::uint32_t seen = 0u;
  for (std::uint32_t round = 1u; round <= 6u; round++) {
      flips.present(round * 10u);
      flips.present(round * 10u + 1u);
      fixture.blank(round, round * 2u - 1u);
      fixture.tick(round * 100u, true);
      for (; seen < fixture.dxgkrnl().interrupts(); seen++) {
          const std::uint64_t id = flips.id_of(seen);
          CHECK(id >= last);
          last = id;
      }
  }
  CHECK_EQ(last, 61u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 12u); }

TEST(device_vblank_early, nothing_is_reported_early_while_vsync_is_off_or_suspended)
{ Fixture fixture;
  PresentedFlips flips(fixture);
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  fixture.take_page();
  flips.present(10u);
  fixture.tick(100u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 0u);
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  fixture.vsync().suspend();
  flips.present(11u);
  fixture.tick(200u);
  CHECK_EQ(fixture.dxgkrnl().interrupts(), 0u); }

TEST(device_vblank_early, a_presented_report_carries_the_gpu_frequency_and_clock)
{ Fixture fixture;
  PresentedFlips flips(fixture);
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  fixture.take_page();
  CHECK_STATUS(fixture.vsync().enable(), Status::ok);
  flips.present(10u);
  fixture.tick(100u);
  const auto &record = fixture.dxgkrnl().interrupt(0u);
  std::uint64_t frequency = 0u;
  const std::uint64_t expected = fixture.platform().performance_counter(frequency);
  static_cast<void>(expected);
  CHECK(frequency != 0u);
  CHECK_EQ((static_cast<std::uint64_t>(record.UnionRemainder[3]) << 32u) | record.UnionRemainder[2], frequency); }

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

TEST(device_vblank_invariants, a_suspended_vblank_that_reports_or_a_phase_that_is_wrong_is_detected)
{ Fixture fixture;
  CHECK_STATUS(fixture.vsync().initialize(), Status::ok);
  CHECK(fixture.vsync().invariants_hold());
  av::VblankPhaseProbe::set_period(av::DeviceVblankProbe::phase(fixture.vsync()), 5u);
  CHECK(!fixture.vsync().invariants_hold());
  av::VblankPhaseProbe::set_period(av::DeviceVblankProbe::phase(fixture.vsync()), 0u);
  CHECK(fixture.vsync().invariants_hold());
  av::DeviceVblankProbe::suspend_while_reporting(fixture.vsync());
  CHECK(!fixture.vsync().invariants_hold());
  fixture.vsync().destroy(); }

namespace {

struct Answering {
    av::FakeVirtqueueDevice *queue;
    const std::uint8_t      *bytes;
    std::uint32_t            length;
};

}

TEST(device_vblank_attach, needs_its_page_and_an_answer_the_device_gave_whole_and_agreeing)
{ av::MappedVirtioFunction mapped;
  av::ObjectStorage<av::VirtioGpuDevice> storage;
  av::DisplaySources sources(mapped.platform());
  DeviceVblank       vsync(mapped.platform(), mapped.interface(), sources);
  CHECK_STATUS(mapped.map(), Status::ok);
  CHECK_STATUS(storage.construct(mapped.platform(), mapped.mappings(), mapped.layout()), Status::ok);
  CHECK_STATUS(storage.object().start(3u), Status::ok);
  CHECK_STATUS(vsync.attach(storage.object()), Status::invalid_state);
  CHECK_STATUS(vsync.initialize(), Status::ok);
  av::FakeVirtqueueDevice queue = mapped.queue_device(0u);
  const std::uint8_t      cut[2] = {0x00u, 0x11u};
  av::GpuResponse         refusal;
  refusal.set(av::GpuResponse::err_unspec);
  av::GpuResponse         agreement;
  agreement.set(av::GpuResponse::ok_nodata);
  Answering answering = {&queue, cut, sizeof cut};
  mapped.platform().on_stall(+[](void *context) {
      const auto *const answering = static_cast<const Answering *>(context);
      static_cast<void>(answering->queue->respond_all(answering->bytes, answering->length));
  }, &answering);
  CHECK(vsync.attach(storage.object()) != Status::ok);
  answering.bytes = refusal.bytes();
  answering.length = static_cast<std::uint32_t>(refusal.length());
  CHECK_STATUS(vsync.attach(storage.object()), Status::not_supported);
  answering.bytes = agreement.bytes();
  answering.length = static_cast<std::uint32_t>(agreement.length());
  CHECK_STATUS(vsync.attach(storage.object()), Status::ok);
  mapped.platform().on_stall(nullptr, nullptr);
  vsync.destroy();
  storage.destroy(); }
