#include "core/ddi/dxgkarg_enum_vidpn_cofunc_modality_record.hpp"
#include "core/display/display_mode_list.hpp"
#include "core/support/nt_status.hpp"
#include "core/vidpn/cofunc_modality.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/violation.hpp"
#include "harness/vidpn/vidpn_fixture.hpp"

#include <cstdint>

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

D3dkmdtVidPnSourceModeRecord source_mode(std::uint32_t width, std::uint32_t height)
{
    D3dkmdtVidPnSourceModeRecord mode = {};
    DisplayMode(width, height).describe(mode);
    return mode;
}

class Enumeration {
public:
    Enumeration() : fixture_(), modes_(), request_{}
    {
        request_.hConstrainingVidPn = fixture_.handle();
        request_.EnumPivotType = no_pivot;
        fixture_.vidpn().add_path(0u, 0u);
    }
    VidPnFixture                          &fixture() { return fixture_; }
    av::FakeVidPn                         &vidpn() { return fixture_.vidpn(); }
    DxgkargEnumVidPnCofuncModalityRecord &request() { return request_; }
    std::uint32_t run()
    {
        return static_cast<std::uint32_t>(
            CofuncModality(fixture_.platform(), fixture_.dxgkrnl(), modes_, request_).enumerate().value());
    }

private:
    VidPnFixture                         fixture_;
    DisplayModeList                      modes_;
    DxgkargEnumVidPnCofuncModalityRecord request_;
};

}  // namespace

TEST(cofunc_modality_enumerate, nothing_pinned_offers_every_mode_on_both_ends)
{ Enumeration enumeration;
  CHECK_EQ(enumeration.run(), 0u);
  const auto &sources = enumeration.vidpn().source_modes(0u);
  const auto &targets = enumeration.vidpn().target_modes(0u);
  CHECK_EQ(sources.modes(), DisplayModeList::mode_count);
  CHECK_EQ(targets.modes(), DisplayModeList::mode_count);
  CHECK_EQ(sources.mode(0u).PrimSurfSizeCx, 640u);
  CHECK_EQ(sources.mode(0u).PixelFormat, 22);
  for (std::uint32_t index = 0u; index < targets.modes(); index++) {
      const D3dkmdtVidPnTargetModeRecord &mode = targets.mode(index);
      const bool is_preferred = mode.VideoSignalInfo.ActiveSizeCx == 1280u && mode.VideoSignalInfo.ActiveSizeCy == 800u;
      CHECK_EQ(mode.WireFormatAndPreference, (is_preferred ? preferred : not_preferred) | rgb_8_bits);
      CHECK_EQ(mode.VideoSignalInfo.VSyncFreqNumerator, 60u);
  }
  CHECK_EQ(enumeration.vidpn().path(0u).ContentTransformationScalingSupport, 1u);
  CHECK_EQ(enumeration.vidpn().path(0u).ContentTransformationRotationSupport, 1u);
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

TEST(cofunc_modality_visit, a_pinned_source_mode_the_adapter_lacks_leaves_no_target_mode)
{ Enumeration enumeration;
  enumeration.vidpn().source_modes(0u).seed(source_mode(1000u, 700u), true);
  enumeration.vidpn().target_modes(0u).seed(D3dkmdtVidPnTargetModeRecord{}, false);
  CHECK_EQ(enumeration.run(), 0u);
  CHECK_EQ(enumeration.vidpn().target_modes(0u).modes(), 0u); }

TEST(cofunc_modality_visit, a_pinned_target_mode_leaves_its_set)
{ Enumeration enumeration;
  D3dkmdtVidPnTargetModeRecord pinned = {};
  DisplayMode(800u, 600u).describe(pinned.VideoSignalInfo);
  enumeration.vidpn().target_modes(0u).seed(pinned, true);
  CHECK_EQ(enumeration.run(), 0u);
  CHECK_EQ(enumeration.vidpn().target_modes(0u).modes(), 1u);
  CHECK_EQ(enumeration.vidpn().source_modes(0u).modes(), DisplayModeList::mode_count); }

TEST(cofunc_modality_visit, the_pivot_mode_set_is_left_alone)
{ Enumeration source_pivot;
  source_pivot.request().EnumPivotType = pivot_source;
  CHECK_EQ(source_pivot.run(), 0u);
  CHECK_EQ(source_pivot.vidpn().source_modes(0u).modes(), 0u);
  CHECK_EQ(source_pivot.vidpn().target_modes(0u).modes(), DisplayModeList::mode_count);
  Enumeration target_pivot;
  target_pivot.request().EnumPivotType = pivot_target;
  CHECK_EQ(target_pivot.run(), 0u);
  CHECK_EQ(target_pivot.vidpn().source_modes(0u).modes(), DisplayModeList::mode_count);
  CHECK_EQ(target_pivot.vidpn().target_modes(0u).modes(), 0u);
  Enumeration other_ids;
  other_ids.request().EnumPivotType = pivot_source;
  other_ids.request().EnumPivotVidPnSourceId = 1u;
  CHECK_EQ(other_ids.run(), 0u);
  CHECK_EQ(other_ids.vidpn().source_modes(0u).modes(), DisplayModeList::mode_count);
  other_ids.request().EnumPivotType = pivot_target;
  other_ids.request().EnumPivotVidPnTargetId = 1u;
  CHECK_EQ(other_ids.run(), 0u);
  CHECK_EQ(other_ids.vidpn().target_modes(0u).modes(), DisplayModeList::mode_count); }

TEST(cofunc_modality_visit, a_pivoting_transformation_keeps_its_support)
{ Enumeration scaling;
  scaling.request().EnumPivotType = pivot_scaling;
  CHECK_EQ(scaling.run(), 0u);
  CHECK_EQ(scaling.vidpn().path(0u).ContentTransformationScalingSupport, 0u);
  CHECK_EQ(scaling.vidpn().path(0u).ContentTransformationRotationSupport, 1u);
  Enumeration rotation;
  rotation.request().EnumPivotType = pivot_rotation;
  CHECK_EQ(rotation.run(), 0u);
  CHECK_EQ(rotation.vidpn().path(0u).ContentTransformationScalingSupport, 1u);
  CHECK_EQ(rotation.vidpn().path(0u).ContentTransformationRotationSupport, 0u); }

TEST(cofunc_modality_visit, a_path_with_both_transformations_chosen_is_not_updated)
{ VidPnFixture fixture;
  DisplayModeList modes;
  auto &path = fixture.vidpn().add_path(0u, 0u);
  path.ContentTransformationScaling = 1;
  path.ContentTransformationRotation = 1;
  DxgkargEnumVidPnCofuncModalityRecord request = {fixture.handle(), no_pivot, 0u, 0u, 0u};
  CHECK_EQ(CofuncModality(fixture.platform(), fixture.dxgkrnl(), modes, request).enumerate().value(), 0);
  CHECK_EQ(fixture.vidpn().path_support_updates(), 0u); }

TEST(cofunc_modality_visit, a_path_to_another_source_or_target_is_invalid)
{ const std::uint32_t ends[][2] = {{1u, 0u}, {0u, 1u}};
  for (const auto &end : ends) {
      VidPnFixture fixture;
      DisplayModeList modes;
      fixture.vidpn().add_path(end[0], end[1]);
      DxgkargEnumVidPnCofuncModalityRecord request = {fixture.handle(), no_pivot, 0u, 0u, 0u};
      CHECK_EQ(static_cast<std::uint32_t>(
                   CofuncModality(fixture.platform(), fixture.dxgkrnl(), modes, request).enumerate().value()),
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
  CofuncModality cofunc(enumeration.fixture().platform(), enumeration.fixture().dxgkrnl(), modes, enumeration.request());
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
