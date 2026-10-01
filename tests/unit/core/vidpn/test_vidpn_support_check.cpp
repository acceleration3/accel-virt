#include "core/ddi/d3dkmdt_vidpn_source_mode_record.hpp"
#include "core/ddi/dxgkarg_is_supported_vidpn_record.hpp"
#include "core/support/nt_status.hpp"
#include "core/display/display_mode.hpp"
#include "core/display/display_mode_list.hpp"
#include "core/display/display_sources.hpp"
#include "core/vidpn/vidpn_support_check.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/violation.hpp"
#include "harness/vidpn/vidpn_fixture.hpp"

#include <cstdint>

using av::D3dkmdtVidPnSourceModeRecord;
using av::DisplayMode;
using av::DisplayModeList;
using av::DxgkargIsSupportedVidPnRecord;
using av::Irql;
using av::VidPnCall;
using av::VidPnFixture;
using av::VidPnSupportCheck;
using av::Violation;

namespace {

constexpr std::int32_t no_memory = static_cast<std::int32_t>(0xC0000017u);
constexpr std::int32_t x8r8g8b8 = 22;   /* D3DDDIFMT_X8R8G8B8 */

/* Decides for the fixture's VidPn; answers the NTSTATUS, and the verdict in `supported`. */
std::int32_t decide(VidPnFixture &fixture, void *vidpn, std::uint8_t &supported)
{
    DxgkargIsSupportedVidPnRecord request = {};
    request.hDesiredVidPn = vidpn;
    request.IsVidPnSupported = 7u;
    const DisplayModeList    modes;
    const av::DisplaySources sources(fixture.platform());
    const std::int32_t status =
        VidPnSupportCheck(fixture.platform(), fixture.dxgkrnl(), modes, sources, request).decide().value();
    supported = request.IsVidPnSupported;
    return status;
}

}  // namespace

TEST(vidpn_support_check_decide, no_vidpn_is_supported)
{ VidPnFixture fixture;
  std::uint8_t supported = 0u;
  CHECK_EQ(decide(fixture, nullptr, supported), 0);
  CHECK_EQ(supported, 1u); }

TEST(vidpn_support_check_decide, the_one_path_or_none_is_supported)
{ VidPnFixture fixture;
  std::uint8_t supported = 0u;
  CHECK_EQ(decide(fixture, fixture.handle(), supported), 0);
  CHECK_EQ(supported, 1u);
  fixture.vidpn().add_path(0u, 0u);
  CHECK_EQ(decide(fixture, fixture.handle(), supported), 0);
  CHECK_EQ(supported, 1u); }

TEST(vidpn_support_check_visit, a_second_path_or_another_source_or_target_is_not_supported)
{ const std::uint32_t ends[][2] = {{1u, 0u}, {0u, 1u}};
  for (const auto &end : ends) {
      VidPnFixture fixture;
      fixture.vidpn().add_path(end[0], end[1]);
      std::uint8_t supported = 1u;
      CHECK_EQ(decide(fixture, fixture.handle(), supported), 0);
      CHECK_EQ(supported, 0u);
  }
  VidPnFixture fixture;
  fixture.vidpn().add_path(0u, 0u);
  fixture.vidpn().add_path(0u, 0u);
  std::uint8_t supported = 1u;
  CHECK_EQ(decide(fixture, fixture.handle(), supported), 0);
  CHECK_EQ(supported, 0u); }

/* FR-006: the answer is "not supported", so DMM never commits a VidPn the
 * adapter would have to refuse halfway through UpdateActiveVidPnPresentPath. */
TEST(vidpn_support_check_visit, a_path_pinning_scaling_rotation_or_a_gamma_ramp_is_not_supported)
{ const std::int32_t scalings[] = {2, 3, 4, 5, 253};   /* Centered .. Custom, and Reserved1 */
  for (const std::int32_t scaling : scalings) {
      VidPnFixture fixture;
      fixture.vidpn().add_path(0u, 0u).ContentTransformationScaling = scaling;
      std::uint8_t supported = 1u;
      CHECK_EQ(decide(fixture, fixture.handle(), supported), 0);
      CHECK_EQ(supported, 0u);
  }
  const std::int32_t rotations[] = {2, 3, 4, 5, 16};   /* Rotate90 .. Rotate270, and the Offset rotations */
  for (const std::int32_t rotation : rotations) {
      VidPnFixture fixture;
      fixture.vidpn().add_path(0u, 0u).ContentTransformationRotation = rotation;
      std::uint8_t supported = 1u;
      CHECK_EQ(decide(fixture, fixture.handle(), supported), 0);
      CHECK_EQ(supported, 0u);
  }
  VidPnFixture fixture;
  fixture.vidpn().add_path(0u, 0u).GammaRampType = 2;   /* D3DDDI_GAMMARAMP_RGB256x3x16 */
  std::uint8_t supported = 1u;
  CHECK_EQ(decide(fixture, fixture.handle(), supported), 0);
  CHECK_EQ(supported, 0u); }

/* Unpinned and NotSpecified are what a VidPn under construction carries, and
 * they are not a claim to refuse. */
TEST(vidpn_support_check_visit, an_undecided_transformation_keeps_the_vidpn_supported)
{ const std::int32_t undecided[][2] = {{254, 254}, {255, 255}, {1, 1}, {0, 0}};
  for (const auto &pair : undecided) {
      VidPnFixture fixture;
      av::D3dkmdtVidPnPresentPathRecord &path = fixture.vidpn().add_path(0u, 0u);
      path.ContentTransformationScaling = pair[0];
      path.ContentTransformationRotation = pair[1];
      std::uint8_t supported = 0u;
      CHECK_EQ(decide(fixture, fixture.handle(), supported), 0);
      CHECK_EQ(supported, 1u);
  } }

/* The standard list has 1024x768; the source mode dxgkrnl pins is one it was
 * given, described the same way. */
TEST(vidpn_support_check_visit, a_pinned_source_mode_the_adapter_offers_is_supported)
{ VidPnFixture fixture;
  fixture.vidpn().add_path(0u, 0u);
  D3dkmdtVidPnSourceModeRecord mode = {};
  DisplayMode(1024u, 768u).describe(mode, DisplayMode::source_formats[0]);
  fixture.vidpn().source_modes(0u).seed(mode, true);
  std::uint8_t supported = 0u;
  CHECK_EQ(decide(fixture, fixture.handle(), supported), 0);
  CHECK_EQ(supported, 1u); }

/* What dxgkrnl restores at boot: a size the host stopped offering, or the
 * X8R8G8B8 an older driver described. Refused, so it is never committed. */
TEST(vidpn_support_check_visit, a_pinned_source_mode_the_adapter_does_not_offer_is_not_supported)
{ D3dkmdtVidPnSourceModeRecord unoffered_size = {};
  DisplayMode(1000u, 700u).describe(unoffered_size, DisplayMode::source_formats[0]);
  D3dkmdtVidPnSourceModeRecord old_format = {};
  DisplayMode(1024u, 768u).describe(old_format, DisplayMode::source_formats[0]);
  old_format.PixelFormat = x8r8g8b8;
  const D3dkmdtVidPnSourceModeRecord pinned[] = {unoffered_size, old_format};
  for (const D3dkmdtVidPnSourceModeRecord &mode : pinned) {
      VidPnFixture fixture;
      fixture.vidpn().add_path(0u, 0u);
      fixture.vidpn().source_modes(0u).seed(mode, true);
      std::uint8_t supported = 1u;
      CHECK_EQ(decide(fixture, fixture.handle(), supported), 0);
      CHECK_EQ(supported, 0u);
  } }

TEST(vidpn_support_check_visit, a_source_mode_set_it_cannot_read_fails)
{ const VidPnCall calls[] = {VidPnCall::acquire_source_mode_set, VidPnCall::acquire_pinned_mode};
  for (const VidPnCall call : calls) {
      VidPnFixture fixture;
      fixture.vidpn().add_path(0u, 0u);
      fixture.vidpn().refuse(call, no_memory);
      std::uint8_t supported = 1u;
      CHECK_EQ(decide(fixture, fixture.handle(), supported), no_memory);
      CHECK_EQ(supported, 0u);
  } }

TEST(vidpn_support_check_decide, a_vidpn_it_cannot_read_fails)
{ VidPnFixture fixture;
  fixture.vidpn().add_path(0u, 0u);
  fixture.vidpn().refuse(VidPnCall::get_topology, no_memory);
  std::uint8_t supported = 1u;
  CHECK_EQ(decide(fixture, fixture.handle(), supported), no_memory);
  CHECK_EQ(supported, 0u); }

TEST(vidpn_support_check, is_passive_only)
{ VidPnFixture fixture;
  DxgkargIsSupportedVidPnRecord request = {};
  const DisplayModeList    modes;
  const av::DisplaySources sources(fixture.platform());
  VidPnSupportCheck check(fixture.platform(), fixture.dxgkrnl(), modes, sources, request);
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(check.decide());
  CHECK_EQ(fixture.platform().unclaimed_violations(), 1u);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  /* A path it would refuse anyway, so the walk stops at visit's own ceiling
   * before it reads the source mode set. */
  av::D3dkmdtVidPnPresentPathRecord path = fixture.vidpn().path(0u);
  path.VidPnSourceId = 1u;
  static_cast<void>(check.visit(path));
  fixture.platform().set_irql(Irql::passive);
  CHECK_EQ(fixture.platform().unclaimed_violations(), 1u);
  CHECK(fixture.platform().claim(Violation::irql_ceiling)); }
