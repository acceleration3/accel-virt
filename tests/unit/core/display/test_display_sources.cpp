#include "core/ddi/dxgk_multiplane_overlay_plane_record.hpp"
#include "core/ddi/dxgk_multiplane_overlay_vsync_info_record.hpp"
#include "core/display/display_mode.hpp"
#include "core/display/display_sources.hpp"
#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

using av::DisplaySources;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;

TEST(display_sources_set_private_format, remembers_it_per_source)
{ MockPlatform platform; DisplaySources sources(platform);
  CHECK_EQ(sources.private_format(0u), 0u);
  CHECK_STATUS(sources.set_private_format(0u, 0x1234u), Status::ok);
  CHECK_EQ(sources.private_format(0u), 0x1234u); }

TEST(display_sources_set_private_format, a_source_the_adapter_does_not_have_is_invalid)
{ MockPlatform platform; DisplaySources sources(platform);
  CHECK_STATUS(sources.set_private_format(sources.count(), 1u), Status::invalid_parameter);
  CHECK_EQ(sources.private_format(sources.count()), 0u); }

TEST(display_sources_commit, remembers_the_mode_until_another_or_none)
{ MockPlatform platform; DisplaySources sources(platform);
  const av::DisplayMode first(640u, 480u), second(800u, 600u);
  CHECK(sources.committed_mode(0u) == nullptr);
  CHECK_STATUS(sources.commit(0u, &first), Status::ok);
  CHECK(sources.committed_mode(0u) == &first);
  CHECK_STATUS(sources.commit(0u, &second), Status::ok);
  CHECK(sources.committed_mode(0u) == &second);
  CHECK_STATUS(sources.commit(0u, nullptr), Status::ok);
  CHECK(sources.committed_mode(0u) == nullptr); }

TEST(display_sources_commit, a_source_the_adapter_does_not_have_is_invalid)
{ MockPlatform platform; DisplaySources sources(platform);
  const av::DisplayMode mode(640u, 480u);
  CHECK_STATUS(sources.commit(sources.count(), &mode), Status::invalid_parameter);
  CHECK(sources.committed_mode(sources.count()) == nullptr); }

TEST(display_sources_set_vsync, records_whether_vsync_interrupts_are_wanted)
{ MockPlatform platform; DisplaySources sources(platform);
  CHECK(!sources.vsync_enabled());
  sources.set_vsync(true);
  CHECK(sources.vsync_enabled());
  sources.set_vsync(false);
  CHECK(!sources.vsync_enabled()); }

TEST(display_sources_set_scanout, remembers_the_primary_address_and_allocation_until_cleared)
{ MockPlatform platform; DisplaySources sources(platform);
  std::uint32_t allocation = 0u;
  CHECK_EQ(sources.scanout_address(0u), 0u);
  CHECK(sources.primary(0u) == nullptr);
  CHECK_STATUS(sources.set_scanout(0u, 0x123456789000ll, &allocation), Status::ok);
  CHECK_EQ(sources.scanout_address(0u), 0x123456789000ull);
  CHECK(sources.primary(0u) == &allocation);
  CHECK_STATUS(sources.set_scanout(0u, 0, nullptr), Status::ok);
  CHECK_EQ(sources.scanout_address(0u), 0u);
  CHECK(sources.primary(0u) == nullptr); }

/* A multiplane flip's base plane is the primary, and the source remembers the
 * flip was multiplane so the vsync reports it as one; a flip with no base
 * plane shows nothing. SetVidPnSourceAddress makes it a plain flip again. The
 * layer handed back carries the flip's address and attributes, and every call
 * is usable at device IRQL, where dxgkrnl makes them. */
TEST(display_sources_set_multiplane_scanout, remembers_the_base_plane_and_that_the_flip_was_multiplane)
{ MockPlatform platform; DisplaySources sources(platform);
  CHECK(sources.flip(0u) == DisplaySources::Flip::address);
  std::uint32_t allocation = 0u;
  av::DxgkMultiplaneOverlayPlaneRecord base = {};
  base.Enabled = 1u;
  base.AllocationAddress = 0x4000ll;
  base.hAllocation = &allocation;
  base.PlaneAttributes[0] = 5u;
  base.PlaneAttributes[av::multiplane_overlay_attribute_words - 1u] = 9u;
  platform.set_irql(Irql::isr);
  CHECK_STATUS(sources.set_multiplane_scanout(0u, &base), Status::ok);
  CHECK(sources.flip(0u) == DisplaySources::Flip::base_plane);
  CHECK_EQ(sources.scanout_address(0u), 0x4000u);
  platform.set_irql(Irql::dispatch);
  av::DxgkMultiplaneOverlayVsyncInfoRecord layer = {};
  layer.LayerIndex = 3u;
  sources.shown(0u, 0u).layer(layer);
  platform.set_irql(Irql::isr);
  CHECK_EQ(layer.LayerIndex, 0u);
  CHECK_EQ(layer.Enabled, 1u);
  CHECK_EQ(layer.PhysicalAddress, 0x4000ll);
  CHECK_EQ(layer.PlaneAttributes[0], 5u);
  CHECK_EQ(layer.PlaneAttributes[av::multiplane_overlay_attribute_words - 1u], 9u);
  CHECK_STATUS(sources.set_multiplane_scanout(0u, nullptr), Status::ok);
  CHECK(sources.flip(0u) == DisplaySources::Flip::planes_disabled);
  CHECK_EQ(sources.scanout_address(0u), 0u);
  CHECK_STATUS(sources.set_scanout(0u, 0x8000ll, &allocation), Status::ok);
  CHECK(sources.flip(0u) == DisplaySources::Flip::address);
  CHECK_STATUS(sources.set_multiplane_scanout(1u, &base), Status::invalid_parameter);
  CHECK(sources.flip(1u) == DisplaySources::Flip::address);
  platform.set_irql(Irql::passive);
  CHECK(sources.shown(sources.count(), 0u).kind() == DisplaySources::Flip::address);   /* no such source */
  CHECK_EQ(sources.shown(sources.count(), 0u).address(), 0u);
  CHECK(sources.invariants_hold()); }

TEST(display_sources_set_scanout, a_source_the_adapter_does_not_have_is_invalid)
{ MockPlatform platform; DisplaySources sources(platform);
  CHECK_STATUS(sources.set_scanout(sources.count(), 0x1000, nullptr), Status::invalid_parameter);
  CHECK_EQ(sources.scanout_address(sources.count()), 0u);
  CHECK(sources.primary(sources.count()) == nullptr); }

/* The DPC reads the handle; an allocation being destroyed must not leave one
 * behind for it to follow. */
TEST(display_sources_forget_primary, clears_the_source_showing_that_allocation)
{ MockPlatform platform; DisplaySources sources(platform);
  std::uint32_t allocation = 0u;
  std::uint32_t other = 0u;
  CHECK_STATUS(sources.set_scanout(0u, 0x4000, &allocation), Status::ok);
  sources.forget_primary(&other);
  CHECK_EQ(sources.scanout_address(0u), 0x4000u);
  sources.forget_primary(&allocation);
  CHECK_EQ(sources.scanout_address(0u), 0u);
  CHECK(sources.primary(0u) == nullptr); }

TEST(display_sources_set_visible, remembers_visibility_per_source)
{ MockPlatform platform; DisplaySources sources(platform);
  CHECK(!sources.visible(0u));
  CHECK_STATUS(sources.set_visible(0u, true), Status::ok);
  CHECK(sources.visible(0u));
  CHECK_STATUS(sources.set_visible(sources.count(), true), Status::invalid_parameter);
  CHECK(!sources.visible(sources.count())); }

TEST(display_sources, scanout_is_readable_and_writable_at_device_irql)
{ MockPlatform platform; DisplaySources sources(platform);
  platform.set_irql(Irql::isr);
  CHECK_STATUS(sources.set_scanout(0u, 0x2000, nullptr), Status::ok);
  CHECK_EQ(sources.scanout_address(0u), 0x2000u);
  platform.set_irql(Irql::passive);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(display_sources, is_passive_only)
{ MockPlatform platform; DisplaySources sources(platform);
  platform.set_irql(Irql::dispatch);
  static_cast<void>(sources.set_private_format(0u, 1u));
  static_cast<void>(sources.private_format(0u));
  static_cast<void>(sources.commit(0u, nullptr));
  static_cast<void>(sources.committed_mode(0u));
  sources.set_vsync(true);
  static_cast<void>(sources.vsync_enabled());
  static_cast<void>(sources.set_visible(0u, true));
  /* visible() is asked from the DPC that carries the frame, so DISPATCH is its
   * ceiling and this one is not a violation. */
  static_cast<void>(sources.visible(0u));
  platform.set_irql(Irql::passive);
  for (int call = 0; call < 6; call++) CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(!platform.claim(Violation::irql_ceiling)); }

/* The device says how many displays it has; the driver's arrays say how many it
 * can offer, and a count outside that is clamped with the value answered. */
TEST(display_sources_set_count, follows_the_device_up_to_what_the_arrays_hold)
{ MockPlatform platform; DisplaySources sources(platform);
  CHECK_EQ(sources.count(), 1u);
  CHECK_STATUS(sources.set_count(2u), Status::ok);
  CHECK_EQ(sources.count(), 2u);
  CHECK_STATUS(sources.set_count(DisplaySources::max_sources), Status::ok);
  CHECK_EQ(sources.count(), DisplaySources::max_sources);
  CHECK_STATUS(sources.set_count(DisplaySources::max_sources + 1u), Status::too_large);
  CHECK_EQ(sources.count(), DisplaySources::max_sources);
  CHECK_STATUS(sources.set_count(0u), Status::invalid_parameter);
  CHECK_EQ(sources.count(), DisplaySources::max_sources);
  CHECK(sources.invariants_hold()); }

/* Every source the device reports keeps its own mode, primary and visibility. */
TEST(display_sources, keeps_each_output_apart)
{ MockPlatform platform; DisplaySources sources(platform);
  CHECK_STATUS(sources.set_count(2u), Status::ok);
  const av::DisplayMode first(1280u, 800u);
  const av::DisplayMode second(1024u, 768u);
  CHECK_STATUS(sources.commit(0u, &first), Status::ok);
  CHECK_STATUS(sources.commit(1u, &second), Status::ok);
  CHECK_STATUS(sources.set_scanout(0u, 0x1000, nullptr), Status::ok);
  CHECK_STATUS(sources.set_visible(1u, true), Status::ok);
  CHECK_EQ(sources.committed_mode(0u)->width(), 1280u);
  CHECK_EQ(sources.committed_mode(1u)->width(), 1024u);
  CHECK_EQ(sources.scanout_address(0u), 0x1000u);
  CHECK_EQ(sources.scanout_address(1u), 0u);
  CHECK(!sources.visible(0u));
  CHECK(sources.visible(1u));
  CHECK_STATUS(sources.commit(2u, &first), Status::invalid_parameter); }

namespace av {

/* The state the invariant guards, which correct code never reaches. */
struct DisplaySourcesProbe {
    static void offer_nothing(DisplaySources &sources) { sources.count_ = 0u; }
    static void offer_more_than_it_holds(DisplaySources &sources)
    { sources.count_ = DisplaySources::max_sources + 1u; }
    /* The count as the four billionth flip leaves it. */
    static void number_from(DisplaySources &sources, std::uint32_t source, std::uint64_t frame)
    { sources.frames_[source] = frame; }
    /* An entry caught while a flip at device IRQL rewrites it. */
    static void tear(DisplaySources &sources, std::uint32_t source, std::uint32_t frame)
    { sources.numbered_frames_[source][frame % DisplaySources::remembered_flips] = 0u; }
};

}  // namespace av

TEST(display_sources_invariants, no_outputs_and_more_than_it_holds_are_detected)
{ MockPlatform platform; DisplaySources sources(platform);
  CHECK(sources.invariants_hold());
  av::DisplaySourcesProbe::offer_nothing(sources);
  CHECK(!sources.invariants_hold());
  DisplaySources other(platform);
  av::DisplaySourcesProbe::offer_more_than_it_holds(other);
  CHECK(!other.invariants_hold()); }

TEST(display_sources_report, writes_each_source_s_format_mode_scanout_and_visibility)
{ MockPlatform platform; DisplaySources sources(platform);
  const av::DisplayMode mode(1280u, 800u);
  std::uint32_t primary = 0u;
  CHECK_STATUS(sources.set_count(2u), Status::ok);
  CHECK_STATUS(sources.set_private_format(0u, 0x15u), Status::ok);
  CHECK_STATUS(sources.commit(0u, &mode), Status::ok);
  CHECK_STATUS(sources.set_scanout(0u, 0x7000, &primary), Status::ok);
  CHECK_STATUS(sources.set_visible(1u, true), Status::ok);
  sources.set_vsync(true);
  char bytes[512];
  av::StateWriter writer(bytes, sizeof bytes);
  sources.report(writer);
  char expected[512];
  std::snprintf(expected, sizeof expected,
                "display_sources/0 count=2 vsync=true\n"
                "display_sources/0/source/0 format=21 width=1280 height=800 scanout=0x0000000000007000"
                " primary=0x%016llX flip=0 visible=false\n"
                "display_sources/0/source/1 format=0 width=0 height=0 scanout=0x0000000000000000"
                " primary=0x0000000000000000 flip=0 visible=true",
                static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(&primary)));
  CHECK(std::strcmp(writer.dump(), expected) == 0); }

/* Every flip is a numbered frame, the number the device's display clock shows
 * it as: counted from 1 per source, and the last remembered_flips kept as they
 * were made, so a vsync can name the one the device latched. One no longer
 * remembered, 0, or one being rewritten reads as the flip the source has now. */
TEST(display_sources_frames, each_flip_is_numbered_and_the_last_few_are_remembered_as_they_were)
{ MockPlatform platform; DisplaySources sources(platform);
  CHECK_EQ(sources.frame(0u), 0u);
  std::uint32_t allocation = 0u;
  platform.set_irql(Irql::isr);
  for (std::uint32_t flip = 1u; flip <= 12u; flip++) {
      CHECK_STATUS(sources.set_scanout(0u, static_cast<std::int64_t>(flip) * 0x1000, &allocation), Status::ok);
      CHECK_EQ(sources.frame(0u), flip);
  }
  CHECK_EQ(sources.frame(1u), 0u);   /* each source counts its own */
  CHECK_EQ(sources.frame(sources.count()), 0u);
  platform.set_irql(Irql::dispatch);
  CHECK_EQ(sources.shown(0u, 12u).address(), 0xC000u);
  CHECK_EQ(sources.shown(0u, 5u).address(), 0x5000u);   /* twelve flips keep the last eight */
  CHECK_EQ(sources.shown(0u, 4u).address(), 0xC000u);   /* forgotten: the source as it is */
  CHECK_EQ(sources.shown(0u, 13u).address(), 0xC000u);  /* never made */
  CHECK_EQ(sources.shown(0u, 0u).address(), 0xC000u);
  av::DisplaySourcesProbe::tear(sources, 0u, 9u);
  CHECK_EQ(sources.shown(0u, 9u).address(), 0xC000u);
  CHECK_EQ(sources.shown(0u, 10u).address(), 0xA000u);
  platform.set_irql(Irql::passive);
  CHECK(sources.invariants_hold()); }

TEST(display_sources_frames, a_multiplane_flip_is_remembered_with_its_own_attributes_and_kind)
{ MockPlatform platform; DisplaySources sources(platform);
  std::uint32_t allocation = 0u;
  av::DxgkMultiplaneOverlayPlaneRecord base = {};
  base.Enabled = 1u;
  base.AllocationAddress = 0x4000ll;
  base.hAllocation = &allocation;
  base.PlaneAttributes[0] = 5u;
  platform.set_irql(Irql::isr);
  CHECK_STATUS(sources.set_multiplane_scanout(0u, &base), Status::ok);
  base.AllocationAddress = 0x6000ll;
  base.PlaneAttributes[0] = 7u;
  CHECK_STATUS(sources.set_multiplane_scanout(0u, &base), Status::ok);
  CHECK_STATUS(sources.set_multiplane_scanout(0u, nullptr), Status::ok);
  platform.set_irql(Irql::dispatch);
  av::DxgkMultiplaneOverlayVsyncInfoRecord layer = {};
  const av::FlipView first = sources.shown(0u, 1u);
  CHECK(first.kind() == DisplaySources::Flip::base_plane);
  first.layer(layer);
  CHECK_EQ(layer.PhysicalAddress, 0x4000ll);
  CHECK_EQ(layer.PlaneAttributes[0], 5u);   /* not the second flip's */
  sources.shown(0u, 2u).layer(layer);
  CHECK_EQ(layer.PlaneAttributes[0], 7u);
  CHECK(sources.shown(0u, 3u).kind() == DisplaySources::Flip::planes_disabled);
  CHECK(sources.shown(0u, 0u).kind() == DisplaySources::Flip::planes_disabled);
  platform.set_irql(Irql::passive); }

TEST(display_sources_frames, the_count_wraps_past_zero_which_is_no_frame)
{ MockPlatform platform; DisplaySources sources(platform);
  av::DisplaySourcesProbe::number_from(sources, 0u, 0xFFFFFFFEu);
  std::uint32_t allocation = 0u;
  CHECK_STATUS(sources.set_scanout(0u, 0x1000, &allocation), Status::ok);
  CHECK_EQ(sources.frame(0u), 0xFFFFFFFFu);
  CHECK_STATUS(sources.set_scanout(0u, 0x2000, &allocation), Status::ok);
  CHECK_EQ(sources.frame(0u), 1u);
  CHECK_EQ(sources.shown(0u, 0xFFFFFFFFu).address(), 0x1000u);
  CHECK_EQ(sources.shown(0u, 1u).address(), 0x2000u); }
