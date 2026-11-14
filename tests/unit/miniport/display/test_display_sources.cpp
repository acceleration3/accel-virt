#include "miniport/ddi/dxgk_multiplane_overlay_plane_record.hpp"
#include "miniport/ddi/dxgk_multiplane_overlay_vsync_info_record.hpp"
#include "miniport/ddi/dxgk_multiplane_overlay_vsync_info2_record.hpp"
#include "miniport/ddi/dxgk_multiplane_overlay_attributes3_record.hpp"
#include "miniport/display/overlay_plane.hpp"
#include "miniport/display/display_mode.hpp"
#include "miniport/display/display_sources.hpp"
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
  CHECK(sources.shown(sources.count(), 0u).kind() == DisplaySources::Flip::address);
  CHECK_EQ(sources.shown(sources.count(), 0u).address(), 0u);
  CHECK(sources.invariants_hold()); }

TEST(display_sources_set_scanout, a_source_the_adapter_does_not_have_is_invalid)
{ MockPlatform platform; DisplaySources sources(platform);
  CHECK_STATUS(sources.set_scanout(sources.count(), 0x1000, nullptr), Status::invalid_parameter);
  CHECK_EQ(sources.scanout_address(sources.count()), 0u);
  CHECK(sources.primary(sources.count()) == nullptr); }

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
  static_cast<void>(sources.visible(0u));
  platform.set_irql(Irql::passive);
  for (int call = 0; call < 6; call++) CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(!platform.claim(Violation::irql_ceiling)); }

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
struct DisplaySourcesProbe {
    static void offer_nothing(DisplaySources &sources) { sources.count_ = 0u; }
    static void offer_more_than_it_holds(DisplaySources &sources)
    { sources.count_ = DisplaySources::max_sources + 1u; }
    static void number_from(DisplaySources &sources, std::uint32_t source, std::uint64_t frame)
    { sources.frames_[source] = frame; }
    static void tear(DisplaySources &sources, std::uint32_t source, std::uint32_t frame)
    { sources.numbered_frames_[source][frame % DisplaySources::remembered_flips] = 0u; }
};

}

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
  char bytes[1024];
  av::StateWriter writer(bytes, sizeof bytes);
  sources.report(writer);
  char expected[1024];
  std::snprintf(expected, sizeof expected,
                "display_sources/0 count=2 vsync=true\n"
                "display_sources/0/programmed/0 frequency=10000000 count=1\n"
                "display_sources/0/programmed/0/at/0 counter=0 id=0 mark=1 extra=0\n"
                "display_sources/0/reported/0 frequency=10000000 count=0\n"
                "display_sources/0/source/0 format=21 width=1280 height=800 scanout=0x0000000000007000"
                " primary=0x%016llX flip=0 visible=false\n"
                "display_sources/0/source/1 format=0 width=0 height=0 scanout=0x0000000000000000"
                " primary=0x0000000000000000 flip=0 visible=true",
                static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(&primary)));
  CHECK(std::strcmp(writer.dump(), expected) == 0); }

TEST(display_sources_report, writes_each_flipped_plane_after_its_source)
{ MockPlatform platform; DisplaySources sources(platform);
  int overlay = 0;
  av::DxgkMultiplaneOverlayAttributes3Record attributes = {};
  attributes.DstRect = {160, 120, 640, 440};
  attributes.Blend = av::overlay_blend_alpha;
  const av::OverlayPlane planes[] = {av::OverlayPlane(0u, 41u), av::OverlayPlane(1u, 5u, &overlay, 0x8000ll, attributes)};
  platform.set_irql(Irql::isr);
  CHECK_STATUS(sources.present_planes(0u, planes, 2u), Status::ok);
  platform.set_irql(Irql::passive);
  char bytes[768];
  av::StateWriter writer(bytes, sizeof bytes);
  sources.report(writer);
  char expected[160];
  std::snprintf(expected, sizeof expected,
                "display_sources/0/source/0/plane/1 present_id=5 shown=true allocation=0x%016llX left=160 top=120"
                " right=640 bottom=440 blended=true",
                static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(&overlay)));
  CHECK(std::strstr(writer.dump(), "display_sources/0/source/0/plane/0 present_id=41 shown=false") != nullptr);
  CHECK(std::strstr(writer.dump(), expected) != nullptr); }

TEST(display_sources_frames, each_flip_is_numbered_and_the_last_few_are_remembered_as_they_were)
{ MockPlatform platform; DisplaySources sources(platform);
  CHECK_EQ(sources.frame(0u), 0u);
  std::uint32_t allocation = 0u;
  platform.set_irql(Irql::isr);
  for (std::uint32_t flip = 1u; flip <= 12u; flip++) {
      CHECK_STATUS(sources.set_scanout(0u, static_cast<std::int64_t>(flip) * 0x1000, &allocation), Status::ok);
      CHECK_EQ(sources.frame(0u), flip);
  }
  CHECK_EQ(sources.frame(1u), 0u);
  CHECK_EQ(sources.frame(sources.count()), 0u);
  platform.set_irql(Irql::dispatch);
  CHECK_EQ(sources.shown(0u, 12u).address(), 0xC000u);
  CHECK_EQ(sources.shown(0u, 5u).address(), 0x5000u);
  CHECK_EQ(sources.shown(0u, 4u).address(), 0xC000u);
  CHECK_EQ(sources.shown(0u, 13u).address(), 0xC000u);
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
  CHECK_EQ(layer.PlaneAttributes[0], 5u);
  sources.shown(0u, 2u).layer(layer);
  CHECK_EQ(layer.PlaneAttributes[0], 7u);
  CHECK(sources.shown(0u, 3u).kind() == DisplaySources::Flip::planes_disabled);
  CHECK(sources.shown(0u, 0u).kind() == DisplaySources::Flip::planes_disabled);
  platform.set_irql(Irql::passive); }

TEST(display_sources_immediate, only_the_newest_flip_counts_and_a_later_flip_clears_it)
{ MockPlatform platform; DisplaySources sources(platform);
  std::uint32_t allocation = 0u;
  av::DxgkMultiplaneOverlayAttributes3Record attributes = {};
  const av::OverlayPlane plane(0u, 9u, &allocation, 0x4000ll, attributes);
  platform.set_irql(Irql::isr);
  CHECK_STATUS(sources.present_planes(0u, &plane, 1u), Status::ok);
  sources.mark_immediate(0u);
  platform.set_irql(Irql::dispatch);
  CHECK(sources.immediate(0u));
  CHECK(!sources.immediate(sources.count()));
  platform.set_irql(Irql::isr);
  CHECK_STATUS(sources.present_planes(0u, &plane, 1u), Status::ok);
  platform.set_irql(Irql::dispatch);
  CHECK(!sources.immediate(0u));
  platform.set_irql(Irql::isr);
  sources.mark_immediate(0u);
  sources.mark_immediate(sources.count());
  CHECK_STATUS(sources.set_scanout(0u, 0x5000, &allocation), Status::ok);
  platform.set_irql(Irql::dispatch);
  CHECK(!sources.immediate(0u));
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

TEST(display_sources_present_planes, replaces_the_layers_named_and_remembers_each_flip)
{ MockPlatform platform; DisplaySources sources(platform);
  int base = 0, overlay = 0;
  av::DxgkMultiplaneOverlayAttributes3Record attributes = {};
  attributes.DstRect = {10, 20, 110, 220};
  platform.set_irql(Irql::isr);
  const av::OverlayPlane first[] = {av::OverlayPlane(0u, 41u, &base, 0x4000ll, attributes),
                                    av::OverlayPlane(1u, 5u, &overlay, 0x8000ll, attributes)};
  CHECK_STATUS(sources.present_planes(0u, first, 2u), Status::ok);
  CHECK(sources.flip(0u) == DisplaySources::Flip::presented_plane);
  CHECK_EQ(sources.scanout_address(0u), 0x4000u);
  const av::OverlayPlane second[] = {av::OverlayPlane(1u, 6u)};
  CHECK_STATUS(sources.present_planes(0u, second, 1u), Status::ok);
  platform.set_irql(Irql::dispatch);
  CHECK(sources.primary(0u) == &base);
  av::OverlayPlane now[av::OverlayPlane::max_planes];
  sources.planes(0u, now);
  CHECK(now[0].shown());
  CHECK(!now[1].shown());
  CHECK_EQ(now[1].present_id(), 6u);
  av::DxgkMultiplaneOverlayVsyncInfo2Record layers[av::OverlayPlane::max_planes] = {};
  CHECK_EQ(sources.shown(0u, 1u).layers(layers), 2u);
  CHECK_EQ(layers[1].PresentId, 5u);
  CHECK(sources.shown(0u, 1u).plane(1u).allocation() == &overlay);
  CHECK_EQ(sources.shown(0u, 1u).plane(1u).destination().bottom, 220);
  CHECK_EQ(sources.shown(0u, 0u).layers(layers), 2u);
  CHECK_EQ(layers[1].PresentId, 6u);
  platform.set_irql(Irql::passive);
  sources.forget_primary(&base);
  sources.planes(0u, now);
  CHECK(!now[0].shown());
  CHECK_EQ(now[0].present_id(), 41u);
  CHECK(sources.primary(0u) == nullptr);
  platform.set_irql(Irql::isr);
  const av::OverlayPlane past[] = {av::OverlayPlane(av::OverlayPlane::max_planes, 9u)};
  CHECK_STATUS(sources.present_planes(0u, past, 1u), Status::invalid_parameter);
  CHECK_STATUS(sources.present_planes(1u, first, 1u), Status::invalid_parameter);
  platform.set_irql(Irql::passive); }

TEST(display_sources_present_planes, refuses_a_list_that_is_missing_or_names_a_layer_there_is_not)
{ MockPlatform platform; DisplaySources sources(platform);
  platform.set_irql(Irql::isr);
  CHECK_STATUS(sources.present_planes(0u, nullptr, 1u), Status::invalid_parameter);
  const av::OverlayPlane beyond[] = {av::OverlayPlane(av::OverlayPlane::max_planes, 1u)};
  CHECK_STATUS(sources.present_planes(0u, beyond, 1u), Status::invalid_parameter);
  CHECK_STATUS(sources.present_planes(sources.count(), nullptr, 0u), Status::invalid_parameter);
  CHECK_STATUS(sources.present_planes(0u, nullptr, 0u), Status::ok);
  platform.set_irql(Irql::passive);
  CHECK(sources.invariants_hold()); }

TEST(display_sources_planes, a_source_there_is_not_has_no_plane_flipped)
{ MockPlatform platform; DisplaySources sources(platform);
  av::OverlayPlane planes[av::OverlayPlane::max_planes];
  int allocation = 0;
  planes[1] = av::OverlayPlane(1u, 4u, &allocation, 0x1000, {});
  platform.set_irql(Irql::dispatch);
  sources.planes(sources.count(), planes);
  platform.set_irql(Irql::passive);
  for (const av::OverlayPlane &plane : planes) CHECK(!plane.flipped()); }

namespace {

struct Tearing {
    av::DisplaySources *sources;
    std::uint32_t       loads;
};

}

TEST(display_sources_frames, a_remembered_flip_overwritten_while_it_is_read_is_the_current_one_instead)
{ MockPlatform platform; DisplaySources sources(platform);
  std::uint32_t allocation = 0u;
  platform.set_irql(Irql::isr);
  for (std::uint32_t flip = 1u; flip <= 12u; flip++)
      CHECK_STATUS(sources.set_scanout(0u, static_cast<std::int64_t>(flip) * 0x1000, &allocation), Status::ok);
  platform.set_irql(Irql::dispatch);
  CHECK_EQ(sources.shown(0u, 11u).address(), 0xB000u);
  Tearing tearing = {&sources, 0u};
  platform.on_load(+[](void *context) {
      auto *const torn = static_cast<Tearing *>(context);
      if (++torn->loads == 2u) av::DisplaySourcesProbe::tear(*torn->sources, 0u, 11u);
  }, &tearing);
  CHECK_EQ(sources.shown(0u, 11u).address(), 0xC000u);
  platform.on_load(nullptr, nullptr);
  platform.set_irql(Irql::passive); }
