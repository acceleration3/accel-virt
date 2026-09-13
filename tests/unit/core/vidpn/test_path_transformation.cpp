#include "core/ddi/d3dkmdt_vidpn_present_path_record.hpp"
#include "core/vidpn/path_transformation.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3dkmdtVidPnPresentPathRecord;
using av::PathTransformation;

namespace {

constexpr std::int32_t transformation_not_supported = static_cast<std::int32_t>(0xC01E0346u);
constexpr std::int32_t gamma_ramp_not_supported = static_cast<std::int32_t>(0xC01E0348u);

/* D3DKMDT_VIDPN_PRESENT_PATH with identity scaling, identity rotation and the
 * default gamma ramp - everything the adapter can actually do. */
D3dkmdtVidPnPresentPathRecord plain_path()
{
    D3dkmdtVidPnPresentPathRecord path = {};
    path.ContentTransformationScaling = 1;   /* D3DKMDT_VPPS_IDENTITY */
    path.ContentTransformationRotation = 1;  /* D3DKMDT_VPPR_IDENTITY */
    path.GammaRampType = 1;                  /* D3DDDI_GAMMARAMP_DEFAULT */
    return path;
}

}  // namespace

TEST(path_transformation_accepted, identity_and_the_default_ramp_only)
{ D3dkmdtVidPnPresentPathRecord path = plain_path();
  CHECK_EQ(PathTransformation(path).accepted().value(), 0);
  path.GammaRampType = 0;   /* D3DDDI_GAMMARAMP_UNINITIALIZED: no ramp asked for */
  CHECK_EQ(PathTransformation(path).accepted().value(), 0);
  /* Uninitialized, Centered .. Custom, Unpinned and NotSpecified. */
  const std::int32_t scalings[] = {0, 2, 3, 4, 5, 254, 255};
  for (const std::int32_t scaling : scalings) {
      path = plain_path();
      path.ContentTransformationScaling = scaling;
      CHECK_EQ(PathTransformation(path).accepted().value(), transformation_not_supported);
  }
  /* Uninitialized, Rotate90 .. Rotate270_Offset270, Unpinned and NotSpecified. */
  const std::int32_t rotations[] = {0, 2, 3, 4, 5, 9, 13, 16, 254, 255};
  for (const std::int32_t rotation : rotations) {
      path = plain_path();
      path.ContentTransformationRotation = rotation;
      CHECK_EQ(PathTransformation(path).accepted().value(), transformation_not_supported);
  } }

/* GammaRampCaps is zero, so a ramp of any kind is refused with its own status
 * rather than accepted and then never programmed. */
TEST(path_transformation_accepted, every_real_gamma_ramp_is_refused)
{ for (std::int32_t ramp = 2; ramp <= 5; ramp++) {
      D3dkmdtVidPnPresentPathRecord path = plain_path();
      path.GammaRampType = ramp;   /* RGB256x3x16, DXGI_1, MATRIX_3x4, MATRIX_V2 */
      std::uint16_t levels[3][256] = {};
      path.GammaRampData = levels;
      path.GammaRampDataSize = sizeof levels;
      CHECK_EQ(PathTransformation(path).accepted().value(), gamma_ramp_not_supported);
  } }

/* A VidPn dxgkrnl is still deciding leaves scaling and rotation Unpinned or
 * NotSpecified, and that is not a claim the adapter has to refuse. */
TEST(path_transformation_supportable, an_undecided_transformation_is_not_a_refusal)
{ const std::int32_t undecided_scaling[] = {0, 1, 254, 255};
  for (const std::int32_t scaling : undecided_scaling) {
      D3dkmdtVidPnPresentPathRecord path = plain_path();
      path.ContentTransformationScaling = scaling;
      CHECK(PathTransformation(path).supportable());
  }
  const std::int32_t undecided_rotation[] = {0, 1, 254, 255};
  for (const std::int32_t rotation : undecided_rotation) {
      D3dkmdtVidPnPresentPathRecord path = plain_path();
      path.ContentTransformationRotation = rotation;
      CHECK(PathTransformation(path).supportable());
  } }

TEST(path_transformation_supportable, a_pinned_scaling_rotation_or_ramp_is_not)
{ const std::int32_t scalings[] = {2, 3, 4, 5, 253};   /* Centered .. Custom, and Reserved1 */
  for (const std::int32_t scaling : scalings) {
      D3dkmdtVidPnPresentPathRecord path = plain_path();
      path.ContentTransformationScaling = scaling;
      CHECK(!PathTransformation(path).supportable());
  }
  /* Rotate90 .. Rotate270, and the Offset rotations: Identity_Offset90 still
   * asks the driver to turn the content 90 degrees itself. */
  const std::int32_t rotations[] = {2, 3, 4, 5, 6, 9, 13, 16};
  for (const std::int32_t rotation : rotations) {
      D3dkmdtVidPnPresentPathRecord path = plain_path();
      path.ContentTransformationRotation = rotation;
      CHECK(!PathTransformation(path).supportable());
  }
  D3dkmdtVidPnPresentPathRecord path = plain_path();
  path.GammaRampType = 2;
  CHECK(!PathTransformation(path).supportable()); }
