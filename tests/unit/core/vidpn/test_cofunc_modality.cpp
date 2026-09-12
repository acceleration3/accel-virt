#include "core/ddi/dxgkarg_enum_vidpn_cofunc_modality_record.hpp"
#include "core/display/display_mode_list.hpp"
#include "core/platform/log_budget.hpp"
#include "core/support/nt_status.hpp"
#include "core/vidpn/cofunc_modality.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/violation.hpp"
#include "harness/vidpn/vidpn_fixture.hpp"

#include <cstdint>
#include <cstring>

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
constexpr std::int32_t pivot_source = 1;    /* D3DKMDT_EPT_VIDPNSOURCE */
constexpr std::int32_t pivot_target = 2;
constexpr std::int32_t pivot_scaling = 3;
constexpr std::int32_t pivot_rotation = 4;
constexpr std::int32_t no_pivot = 5;
constexpr std::uint32_t preferred = 1u;         /* D3DKMDT_MP_PREFERRED */
constexpr std::uint32_t not_preferred = 2u;
constexpr std::uint32_t rgb_8_bits = 0x02u << 2u;
constexpr std::int32_t identity = 1;            /* D3DKMDT_VPPS_IDENTITY / D3DKMDT_VPPR_IDENTITY */
constexpr std::uint32_t identity_supported = 1u;
/* Identity | Offset0: without Offset0 dxgkrnl bugchecks 0x113 subcode 0x1C. */
constexpr std::uint32_t rotation_supported = 0x11u;

D3dkmdtVidPnSourceModeRecord source_mode(std::uint32_t width, std::uint32_t height)
{
    D3dkmdtVidPnSourceModeRecord mode = {};
    DisplayMode(width, height).describe(mode);
    return mode;
}

class Enumeration {
public:
    Enumeration() : fixture_(), budget_(fixture_.platform()), modes_(), request_{},
                    path_(fixture_.vidpn().add_path(0u, 0u))
    {
        request_.hConstrainingVidPn = fixture_.handle();
        request_.EnumPivotType = no_pivot;
    }
    VidPnFixture                          &fixture() { return fixture_; }
    av::LogBudget                         &budget() { return budget_; }
    av::D3dkmdtVidPnPresentPathRecord     &path() { return path_; }
    av::FakeVidPn                         &vidpn() { return fixture_.vidpn(); }
    DxgkargEnumVidPnCofuncModalityRecord &request() { return request_; }
    std::uint32_t run()
    {
        return static_cast<std::uint32_t>(
            CofuncModality(fixture_.platform(), fixture_.dxgkrnl(), budget_, modes_, request_).enumerate().value());
    }

private:
    VidPnFixture                          fixture_;
    av::LogBudget                         budget_;
    DisplayModeList                       modes_;
    DxgkargEnumVidPnCofuncModalityRecord  request_;
    av::D3dkmdtVidPnPresentPathRecord     &path_;
};

}  // namespace

TEST(cofunc_modality_enumerate, nothing_pinned_offers_every_mode_on_both_ends)
{ Enumeration enumeration;
  CHECK_EQ(enumeration.run(), 0u);
  const auto &sources = enumeration.vidpn().source_modes(0u);
  const auto &targets = enumeration.vidpn().target_modes(0u);
  CHECK_EQ(sources.modes(), DisplayModeList::standard_count);
  CHECK_EQ(targets.modes(), DisplayModeList::standard_count);
  CHECK_EQ(sources.mode(0u).PrimSurfSizeCx, 640u);
  CHECK_EQ(sources.mode(0u).PixelFormat, 22);
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

/* Assigning the empty set that would otherwise be built is what SetCofuncModeSet
 * refuses with STATUS_GRAPHICS_EMPTY_ADAPTER_MONITOR_MODE_SUPPORT_INTERSECTION. */
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
  CHECK_EQ(enumeration.vidpn().source_modes(0u).modes(), DisplayModeList::standard_count); }

TEST(cofunc_modality_visit, the_pivot_mode_set_is_left_alone)
{ Enumeration source_pivot;
  source_pivot.request().EnumPivotType = pivot_source;
  CHECK_EQ(source_pivot.run(), 0u);
  CHECK_EQ(source_pivot.vidpn().source_modes(0u).modes(), 0u);
  CHECK_EQ(source_pivot.vidpn().target_modes(0u).modes(), DisplayModeList::standard_count);
  Enumeration target_pivot;
  target_pivot.request().EnumPivotType = pivot_target;
  CHECK_EQ(target_pivot.run(), 0u);
  CHECK_EQ(target_pivot.vidpn().source_modes(0u).modes(), DisplayModeList::standard_count);
  CHECK_EQ(target_pivot.vidpn().target_modes(0u).modes(), 0u);
  Enumeration other_ids;
  other_ids.request().EnumPivotType = pivot_source;
  other_ids.request().EnumPivotVidPnSourceId = 1u;
  CHECK_EQ(other_ids.run(), 0u);
  CHECK_EQ(other_ids.vidpn().source_modes(0u).modes(), DisplayModeList::standard_count);
  other_ids.request().EnumPivotType = pivot_target;
  other_ids.request().EnumPivotVidPnTargetId = 1u;
  CHECK_EQ(other_ids.run(), 0u);
  CHECK_EQ(other_ids.vidpn().target_modes(0u).modes(), DisplayModeList::standard_count); }

/* dxgkrnl closes an enumeration only when it has been told about every path -
 * UpdatePathSupportInfo's countdown - so a pivot does not buy the path a skip.
 * A pivot changes what is pinned, never what the adapter supports. */
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

/* What the path already carries does not change the answer: identity is all the
 * adapter does, whether the transformation is unpinned (254, what dxgkrnl
 * presents), uninitialized (0, what a caller-built record carries) or already
 * pinned to identity. */
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

/* The line is the only way to see, from a guest that will not settle a topology,
 * what dxgkrnl asked about each path and what it already had pinned. */
TEST(cofunc_modality_visit, each_path_says_what_it_was_asked_and_what_was_pinned)
{ Enumeration unpinned;
  CHECK_EQ(unpinned.run(), 0u);
  CHECK(std::strcmp(unpinned.fixture().platform().last_log(),
                    "rvkgpu: cofunc path 0->0 pivot 5 scaling 254 rotation 254 source unpinned target unpinned") == 0);
  Enumeration pinned;
  pinned.vidpn().source_modes(0u).seed(source_mode(1024u, 768u), true);
  D3dkmdtVidPnTargetModeRecord target = {};
  DisplayMode(1024u, 768u).describe(target.VideoSignalInfo);
  pinned.vidpn().target_modes(0u).seed(target, true);
  CHECK_EQ(pinned.run(), 0u);
  CHECK(std::strcmp(pinned.fixture().platform().last_log(),
                    "rvkgpu: cofunc path 0->0 pivot 5 scaling 254 rotation 254 source 1024x768 target pinned") == 0); }

/* dxgkrnl repeats the VidPn DDIs while it settles a topology, so the line comes
 * out of the budget like every other repeated one. */
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
      fixture.vidpn().add_path(end[0], end[1]);
      DxgkargEnumVidPnCofuncModalityRecord request = {fixture.handle(), no_pivot, 0u, 0u, 0u};
      CHECK_EQ(static_cast<std::uint32_t>(
                   CofuncModality(fixture.platform(), fixture.dxgkrnl(), budget, modes, request).enumerate().value()),
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
  CofuncModality cofunc(enumeration.fixture().platform(), enumeration.fixture().dxgkrnl(), enumeration.budget(),
                        modes, enumeration.request());
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
