#include "miniport/ddi/d3dkmdt_vidpn_present_path_record.hpp"
#include "miniport/vidpn/path_transformation.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3dkmdtVidPnPresentPathRecord;
using av::PathTransformation;

namespace {

constexpr std::int32_t transformation_not_supported = static_cast<std::int32_t>(0xC01E0346u);
constexpr std::int32_t gamma_ramp_not_supported = static_cast<std::int32_t>(0xC01E0348u);

D3dkmdtVidPnPresentPathRecord plain_path()
{
    D3dkmdtVidPnPresentPathRecord path = {};
    path.ContentTransformationScaling = 1;
    path.ContentTransformationRotation = 1;
    path.GammaRampType = 1;
    return path;
}

}

TEST(path_transformation_accepted, identity_and_the_default_ramp_only)
{ D3dkmdtVidPnPresentPathRecord path = plain_path();
  CHECK_EQ(PathTransformation(path).accepted().value(), 0);
  path.GammaRampType = 0;
  CHECK_EQ(PathTransformation(path).accepted().value(), 0);
  const std::int32_t scalings[] = {0, 2, 3, 4, 5, 254, 255};
  for (const std::int32_t scaling : scalings) {
      path = plain_path();
      path.ContentTransformationScaling = scaling;
      CHECK_EQ(PathTransformation(path).accepted().value(), transformation_not_supported);
  }
  const std::int32_t rotations[] = {0, 2, 3, 4, 5, 9, 13, 16, 254, 255};
  for (const std::int32_t rotation : rotations) {
      path = plain_path();
      path.ContentTransformationRotation = rotation;
      CHECK_EQ(PathTransformation(path).accepted().value(), transformation_not_supported);
  } }

TEST(path_transformation_accepted, every_real_gamma_ramp_is_refused)
{ for (std::int32_t ramp = 2; ramp <= 5; ramp++) {
      D3dkmdtVidPnPresentPathRecord path = plain_path();
      path.GammaRampType = ramp;
      std::uint16_t levels[3][256] = {};
      path.GammaRampData = levels;
      path.GammaRampDataSize = sizeof levels;
      CHECK_EQ(PathTransformation(path).accepted().value(), gamma_ramp_not_supported);
  } }

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
{ const std::int32_t scalings[] = {2, 3, 4, 5, 253};
  for (const std::int32_t scaling : scalings) {
      D3dkmdtVidPnPresentPathRecord path = plain_path();
      path.ContentTransformationScaling = scaling;
      CHECK(!PathTransformation(path).supportable());
  }
  const std::int32_t rotations[] = {2, 3, 4, 5, 6, 9, 13, 16};
  for (const std::int32_t rotation : rotations) {
      D3dkmdtVidPnPresentPathRecord path = plain_path();
      path.ContentTransformationRotation = rotation;
      CHECK(!PathTransformation(path).supportable());
  }
  D3dkmdtVidPnPresentPathRecord path = plain_path();
  path.GammaRampType = 2;
  CHECK(!PathTransformation(path).supportable()); }
