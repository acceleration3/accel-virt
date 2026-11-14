#include "miniport/ddi/dxgkarg_enum_vidpn_cofunc_modality_record.hpp"
#include "miniport/display/display_mode_list.hpp"
#include "miniport/display/display_sources.hpp"
#include "core/platform/log_budget.hpp"
#include "core/support/nt_status.hpp"
#include "miniport/vidpn/cofunc_modality.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/violation.hpp"
#include "harness/vidpn/vidpn_fixture.hpp"
#include "proto/virtio_gpu/display_mode_set.hpp"

#include <cstdint>
#include <cstring>
#include <initializer_list>

using av::CofuncModality;
using av::D3dkmdtVidPnSourceModeRecord;
using av::D3dkmdtVidPnTargetModeRecord;
using av::DisplayMode;
using av::DisplayModeList;
using av::DxgkargEnumVidPnCofuncModalityRecord;
using av::Irql;
using av::VidPnCall;
using av::VidPnFixture;
using av::Violation;

namespace {

constexpr std::int32_t no_memory = static_cast<std::int32_t>(0xC0000017u);
constexpr std::uint32_t invalid_parameter = 0xC000000Du;
constexpr std::int32_t pivot_source = 1;
constexpr std::int32_t pivot_target = 2;
constexpr std::int32_t pivot_scaling = 3;
constexpr std::int32_t pivot_rotation = 4;
constexpr std::int32_t no_pivot = 5;
constexpr std::uint32_t preferred = 1u;
constexpr std::uint32_t not_preferred = 2u;
constexpr std::uint32_t rgb_8_bits = 0x02u << 2u;
constexpr std::int32_t identity = 1;
constexpr std::uint32_t identity_supported = 1u;
constexpr std::uint32_t rotation_supported = 0x11u;

D3dkmdtVidPnSourceModeRecord source_mode(std::uint32_t width, std::uint32_t height)
{
    D3dkmdtVidPnSourceModeRecord mode = {};
    DisplayMode(width, height).describe(mode, DisplayMode::source_formats[0]);
    return mode;
}

class Enumeration {
public:
    Enumeration() : fixture_(), budget_(fixture_.platform()), modes_(), sources_(fixture_.platform()), request_{},
                    path_(fixture_.vidpn().add_path(0u, 0u))
    {
        request_.hConstrainingVidPn = fixture_.handle();
        request_.EnumPivotType = no_pivot;
    }
    VidPnFixture                          &fixture() { return fixture_; }
    av::LogBudget                         &budget() { return budget_; }
    av::D3dkmdtVidPnPresentPathRecord     &path() { return path_; }
    av::FakeVidPn                         &vidpn() { return fixture_.vidpn(); }
    DisplayModeList                       &modes() { return modes_; }
    DxgkargEnumVidPnCofuncModalityRecord &request() { return request_; }
    std::uint32_t run()
    {
        return static_cast<std::uint32_t>(
            CofuncModality(fixture_.platform(), fixture_.dxgkrnl(), budget_, modes_, sources_, request_).enumerate().value());
    }

private:
    VidPnFixture                          fixture_;
    av::LogBudget                         budget_;
    DisplayModeList                       modes_;
    av::DisplaySources                    sources_;
    DxgkargEnumVidPnCofuncModalityRecord  request_;
    av::D3dkmdtVidPnPresentPathRecord     &path_;
};

}

TEST(cofunc_modality_enumerate, nothing_pinned_offers_every_mode_on_both_ends)
{ Enumeration enumeration;
  CHECK_EQ(enumeration.run(), 0u);
  const auto &sources = enumeration.vidpn().source_modes(0u);
  const auto &targets = enumeration.vidpn().target_modes(0u);
  CHECK_EQ(sources.modes(), DisplayModeList::standard_count * 2u);
  CHECK_EQ(targets.modes(), DisplayModeList::standard_count);
  CHECK_EQ(sources.mode(0u).PrimSurfSizeCx, 640u);
  CHECK_EQ(sources.mode(0u).PixelFormat, 21);
  CHECK_EQ(sources.mode(DisplayModeList::standard_count).PrimSurfSizeCx, 640u);
  CHECK_EQ(sources.mode(DisplayModeList::standard_count).PixelFormat, 32);
  CHECK_EQ(sources.mode(DisplayModeList::standard_count - 1u).PixelFormat, 21);
  for (std::uint32_t index = 0u; index < targets.modes(); index++) {
      const D3dkmdtVidPnTargetModeRecord &mode = targets.mode(index);
      const bool is_preferred = mode.VideoSignalInfo.ActiveSizeCx == 1280u && mode.VideoSignalInfo.ActiveSizeCy == 800u;
      CHECK_EQ(mode.WireFormatAndPreference, (is_preferred ? preferred : not_preferred) | rgb_8_bits);
      CHECK_EQ(mode.VideoSignalInfo.VSyncFreqNumerator, 60u);
  }
  CHECK_EQ(enumeration.vidpn().path(0u).ContentTransformationScalingSupport, identity_supported);
  CHECK_EQ(enumeration.vidpn().path(0u).ContentTransformationRotationSupport, rotation_supported);
  CHECK_EQ(enumeration.vidpn().path_support_updates(), 1u); }

TEST(cofunc_modality_visit, a_pinned_source_mode_leaves_its_set_and_constrains_the_target)
{ Enumeration enumeration;
  enumeration.vidpn().source_modes(0u).seed(source_mode(1024u, 768u), true);
  CHECK_EQ(enumeration.run(), 0u);
  CHECK_EQ(enumeration.vidpn().source_modes(0u).modes(), 1u);
  const auto &targets = enumeration.vidpn().target_modes(0u);
  CHECK_EQ(targets.modes(), 1u);
  CHECK_EQ(targets.mode(0u).VideoSignalInfo.ActiveSizeCx, 1024u);
  CHECK_EQ(targets.mode(0u).WireFormatAndPreference, not_preferred | rgb_8_bits); }

TEST(cofunc_modality_visit, a_pinned_source_mode_the_adapter_lacks_leaves_the_target_set_alone)
{ Enumeration enumeration;
  D3dkmdtVidPnTargetModeRecord existing = {};
  DisplayMode(800u, 600u).describe(existing.VideoSignalInfo);
  enumeration.vidpn().source_modes(0u).seed(source_mode(1000u, 700u), true);
  enumeration.vidpn().target_modes(0u).seed(existing, false);
  CHECK_EQ(enumeration.run(), 0u);
  const auto &targets = enumeration.vidpn().target_modes(0u);
  CHECK_EQ(targets.modes(), 1u);
  CHECK_EQ(targets.mode(0u).VideoSignalInfo.ActiveSizeCx, 800u);
  CHECK_EQ(enumeration.vidpn().path_support_updates(), 1u); }

TEST(cofunc_modality_visit, a_pinned_target_mode_leaves_its_set)
{ Enumeration enumeration;
  D3dkmdtVidPnTargetModeRecord pinned = {};
  DisplayMode(800u, 600u).describe(pinned.VideoSignalInfo);
  enumeration.vidpn().target_modes(0u).seed(pinned, true);
  CHECK_EQ(enumeration.run(), 0u);
  CHECK_EQ(enumeration.vidpn().target_modes(0u).modes(), 1u);
  CHECK_EQ(enumeration.vidpn().source_modes(0u).modes(), DisplayModeList::standard_count * 2u); }

TEST(cofunc_modality_visit, the_pivot_mode_set_is_left_alone)
{ Enumeration source_pivot;
  source_pivot.request().EnumPivotType = pivot_source;
  CHECK_EQ(source_pivot.run(), 0u);
  CHECK_EQ(source_pivot.vidpn().source_modes(0u).modes(), 0u);
  CHECK_EQ(source_pivot.vidpn().target_modes(0u).modes(), DisplayModeList::standard_count);
  Enumeration target_pivot;
  target_pivot.request().EnumPivotType = pivot_target;
  CHECK_EQ(target_pivot.run(), 0u);
  CHECK_EQ(target_pivot.vidpn().source_modes(0u).modes(), DisplayModeList::standard_count * 2u);
  CHECK_EQ(target_pivot.vidpn().target_modes(0u).modes(), 0u);
  Enumeration other_ids;
  other_ids.request().EnumPivotType = pivot_source;
  other_ids.request().EnumPivotVidPnSourceId = 1u;
  CHECK_EQ(other_ids.run(), 0u);
  CHECK_EQ(other_ids.vidpn().source_modes(0u).modes(), DisplayModeList::standard_count * 2u);
  other_ids.request().EnumPivotType = pivot_target;
  other_ids.request().EnumPivotVidPnTargetId = 1u;
  CHECK_EQ(other_ids.run(), 0u);
  CHECK_EQ(other_ids.vidpn().target_modes(0u).modes(), DisplayModeList::standard_count); }

TEST(cofunc_modality_visit, a_pivot_does_not_stop_the_path_being_told_what_is_supported)
{ const std::int32_t pivots[] = {pivot_source, pivot_target, pivot_scaling, pivot_rotation, no_pivot};
  for (std::int32_t pivot : pivots) {
      Enumeration enumeration;
      enumeration.request().EnumPivotType = pivot;
      CHECK_EQ(enumeration.run(), 0u);
      CHECK_EQ(enumeration.vidpn().path_support_updates(), 1u);
      CHECK_EQ(enumeration.vidpn().path(0u).ContentTransformationScalingSupport, identity_supported);
      CHECK_EQ(enumeration.vidpn().path(0u).ContentTransformationRotationSupport, rotation_supported);
  } }

TEST(cofunc_modality_visit, every_path_is_offered_identity_whatever_it_carries)
{ const std::int32_t carried[] = {0, 254, identity};
  for (std::int32_t transformation : carried) {
      Enumeration enumeration;
      enumeration.path().ContentTransformationScaling = transformation;
      enumeration.path().ContentTransformationRotation = transformation;
      CHECK_EQ(enumeration.run(), 0u);
      CHECK_EQ(enumeration.vidpn().path_support_updates(), 1u);
      CHECK_EQ(enumeration.vidpn().path(0u).ContentTransformationScalingSupport, identity_supported);
      CHECK_EQ(enumeration.vidpn().path(0u).ContentTransformationRotationSupport, rotation_supported);
  } }

TEST(cofunc_modality_visit, each_path_says_what_it_was_asked_and_what_was_pinned)
{ Enumeration unpinned;
  CHECK_EQ(unpinned.run(), 0u);
  CHECK(std::strcmp(unpinned.fixture().platform().last_log(),
                    "avgpu: cofunc path 0->0 pivot 5 scaling 254 rotation 254 source unpinned target unpinned") == 0);
  Enumeration pinned;
  pinned.vidpn().source_modes(0u).seed(source_mode(1024u, 768u), true);
  D3dkmdtVidPnTargetModeRecord target = {};
  DisplayMode(1024u, 768u).describe(target.VideoSignalInfo);
  pinned.vidpn().target_modes(0u).seed(target, true);
  CHECK_EQ(pinned.run(), 0u);
  CHECK(std::strcmp(pinned.fixture().platform().last_log(),
                    "avgpu: cofunc path 0->0 pivot 5 scaling 254 rotation 254 source 1024x768 target pinned") == 0); }

TEST(cofunc_modality_visit, the_path_line_stops_when_the_window_is_spent)
{ Enumeration enumeration;
  while (enumeration.budget().allows()) {}
  const std::uint32_t spent = enumeration.fixture().platform().logged();
  CHECK_EQ(enumeration.run(), 0u);
  CHECK_EQ(enumeration.fixture().platform().logged(), spent); }

TEST(cofunc_modality_visit, a_path_to_another_source_or_target_is_invalid)
{ const std::uint32_t ends[][2] = {{1u, 0u}, {0u, 1u}};
  for (const auto &end : ends) {
      VidPnFixture fixture;
      av::LogBudget budget(fixture.platform());
      DisplayModeList modes;
      const av::DisplaySources sources(fixture.platform());
      fixture.vidpn().add_path(end[0], end[1]);
      DxgkargEnumVidPnCofuncModalityRecord request = {fixture.handle(), no_pivot, 0u, 0u, 0u};
      CHECK_EQ(static_cast<std::uint32_t>(CofuncModality(fixture.platform(), fixture.dxgkrnl(), budget, modes,
                                                        sources, request).enumerate().value()),
               invalid_parameter);
  } }

TEST(cofunc_modality_enumerate, every_refusal_fails_without_a_leak)
{ const VidPnCall calls[] = {VidPnCall::get_topology,          VidPnCall::acquire_source_mode_set,
                             VidPnCall::acquire_pinned_mode,   VidPnCall::create_source_mode_set,
                             VidPnCall::add_mode,              VidPnCall::assign_source_mode_set,
                             VidPnCall::acquire_target_mode_set, VidPnCall::create_target_mode_set,
                             VidPnCall::assign_target_mode_set, VidPnCall::update_path_support};
  for (VidPnCall call : calls) {
      Enumeration enumeration;
      enumeration.vidpn().refuse(call, no_memory);
      CHECK_EQ(enumeration.run(), static_cast<std::uint32_t>(no_memory));
  }
  Enumeration pinned;
  pinned.vidpn().source_modes(0u).seed(source_mode(1024u, 768u), true);
  pinned.vidpn().refuse(VidPnCall::add_mode, no_memory);
  CHECK_EQ(pinned.run(), static_cast<std::uint32_t>(no_memory)); }

TEST(cofunc_modality, is_passive_only)
{ Enumeration enumeration;
  DisplayModeList modes;
  const av::DisplaySources sources(enumeration.fixture().platform());
  CofuncModality cofunc(enumeration.fixture().platform(), enumeration.fixture().dxgkrnl(), enumeration.budget(),
                        modes, sources, enumeration.request());
  enumeration.fixture().platform().set_irql(Irql::dispatch);
  static_cast<void>(cofunc.visit(enumeration.vidpn().path(0u)));
  enumeration.fixture().platform().set_irql(Irql::passive);
  CHECK(enumeration.fixture().platform().claim(Violation::irql_ceiling));
  while (enumeration.fixture().platform().claim(Violation::irql_ceiling)) {}
  enumeration.fixture().platform().set_irql(Irql::dispatch);
  static_cast<void>(cofunc.enumerate());
  enumeration.fixture().platform().set_irql(Irql::passive);
  CHECK(enumeration.fixture().platform().claim(Violation::irql_ceiling));
  while (enumeration.fixture().platform().claim(Violation::irql_ceiling)) {} }

namespace {

av::DisplayModeSet wide_host_modes()
{
    av::DisplayModeSet host;
    CHECK_STATUS(host.add(5120u, 1440u), av::Status::ok);
    CHECK_STATUS(host.add(7680u, 2160u), av::Status::ok);
    return host;
}

template <typename Modes, typename Width, typename Height>
std::uint32_t sized(const Modes &modes, Width width, Height height, std::uint32_t wanted_width, std::uint32_t wanted_height)
{
    std::uint32_t found = 0u;
    for (std::uint32_t index = 0u; index < modes.modes(); index++)
        found += width(modes.mode(index)) == wanted_width && height(modes.mode(index)) == wanted_height ? 1u : 0u;
    return found;
}

std::uint32_t source_modes_of(av::FakeVidPn &vidpn, std::uint32_t width, std::uint32_t height)
{
    return sized(vidpn.source_modes(0u), [](const D3dkmdtVidPnSourceModeRecord &mode) { return mode.PrimSurfSizeCx; },
                 [](const D3dkmdtVidPnSourceModeRecord &mode) { return mode.PrimSurfSizeCy; }, width, height);
}

std::uint32_t target_modes_of(av::FakeVidPn &vidpn, std::uint32_t width, std::uint32_t height)
{
    return sized(vidpn.target_modes(0u),
                 [](const D3dkmdtVidPnTargetModeRecord &mode) { return mode.VideoSignalInfo.ActiveSizeCx; },
                 [](const D3dkmdtVidPnTargetModeRecord &mode) { return mode.VideoSignalInfo.ActiveSizeCy; }, width, height);
}

}

TEST(cofunc_modality_enumerate, modes_wider_than_an_edid_timing_are_offered_on_both_ends)
{ Enumeration enumeration;
  CHECK_STATUS(enumeration.modes().adopt(wide_host_modes()), av::Status::ok);
  CHECK_EQ(enumeration.run(), 0u);
  CHECK_EQ(source_modes_of(enumeration.vidpn(), 5120u, 1440u), 2u);
  CHECK_EQ(source_modes_of(enumeration.vidpn(), 7680u, 2160u), 2u);
  CHECK(target_modes_of(enumeration.vidpn(), 5120u, 1440u) >= 1u);
  CHECK(target_modes_of(enumeration.vidpn(), 7680u, 2160u) >= 1u);
  const auto &sources = enumeration.vidpn().source_modes(0u);
  for (std::uint32_t index = 0u; index < sources.modes(); index++) {
      const D3dkmdtVidPnSourceModeRecord &mode = sources.mode(index);
      CHECK_EQ(mode.VisibleRegionSizeCx, mode.PrimSurfSizeCx);
      CHECK_EQ(mode.Stride, mode.PrimSurfSizeCx * 4u);
  }
  const auto &targets = enumeration.vidpn().target_modes(0u);
  std::uint32_t preferred_modes = 0u;
  for (std::uint32_t index = 0u; index < targets.modes(); index++) {
      const D3dkmdtVidPnTargetModeRecord &mode = targets.mode(index);
      if (mode.WireFormatAndPreference != (preferred | rgb_8_bits)) continue;
      preferred_modes++;
      CHECK_EQ(mode.VideoSignalInfo.ActiveSizeCx, 5120u);
      CHECK_EQ(mode.VideoSignalInfo.ActiveSizeCy, 1440u);
      CHECK_EQ(mode.VideoSignalInfo.VSyncFreqNumerator, 60u);
  }
  CHECK_EQ(preferred_modes, 1u); }

TEST(cofunc_modality_visit, a_pinned_source_mode_wider_than_an_edid_timing_constrains_the_target_to_its_size)
{ struct Case { std::uint32_t width, height; };
  for (const Case &pinned : {Case{5120u, 1440u}, Case{7680u, 2160u}}) {
      Enumeration enumeration;
      CHECK_STATUS(enumeration.modes().adopt(wide_host_modes()), av::Status::ok);
      enumeration.vidpn().source_modes(0u).seed(source_mode(pinned.width, pinned.height), true);
      CHECK_EQ(enumeration.run(), 0u);
      CHECK_EQ(enumeration.vidpn().source_modes(0u).modes(), 1u);
      const auto &targets = enumeration.vidpn().target_modes(0u);
      CHECK_EQ(targets.modes(), target_modes_of(enumeration.vidpn(), pinned.width, pinned.height));
      CHECK(target_modes_of(enumeration.vidpn(), pinned.width, pinned.height) >= 1u);
      CHECK_EQ(enumeration.vidpn().path_support_updates(), 1u);
  } }
