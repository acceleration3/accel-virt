#include "core/ddi/dxgkarg_is_supported_vidpn_record.hpp"
#include "core/support/nt_status.hpp"
#include "core/display/display_sources.hpp"
#include "core/vidpn/vidpn_support_check.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/violation.hpp"
#include "harness/vidpn/vidpn_fixture.hpp"

#include <cstdint>

using av::DxgkargIsSupportedVidPnRecord;
using av::Irql;
using av::VidPnCall;
using av::VidPnFixture;
using av::VidPnSupportCheck;
using av::Violation;

namespace {

constexpr std::int32_t no_memory = static_cast<std::int32_t>(0xC0000017u);

/* Decides for the fixture's VidPn; answers the NTSTATUS, and the verdict in `supported`. */
std::int32_t decide(VidPnFixture &fixture, void *vidpn, std::uint8_t &supported)
{
    DxgkargIsSupportedVidPnRecord request = {};
    request.hDesiredVidPn = vidpn;
    request.IsVidPnSupported = 7u;
    const av::DisplaySources sources(fixture.platform());
    const std::int32_t status =
        VidPnSupportCheck(fixture.platform(), fixture.dxgkrnl(), sources, request).decide().value();
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
  const av::DisplaySources sources(fixture.platform());
  VidPnSupportCheck check(fixture.platform(), fixture.dxgkrnl(), sources, request);
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(check.decide());
  static_cast<void>(check.visit(fixture.vidpn().path(0u)));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  CHECK(fixture.platform().claim(Violation::irql_ceiling)); }
