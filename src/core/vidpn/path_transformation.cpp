#include "core/vidpn/path_transformation.hpp"

#include "core/ddi/d3dkmdt_vidpn_present_path_record.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

PathTransformation::PathTransformation(const D3dkmdtVidPnPresentPathRecord &path)
    : scaling_(path.ContentTransformationScaling), rotation_(path.ContentTransformationRotation),
      gamma_(path.GammaRampType)
      {
          AV_SCOPE("PathTransformation::PathTransformation");
      }

NtStatus PathTransformation::accepted() const
{
    AV_SCOPE("PathTransformation::accepted");
    if (scaling_ != identity || rotation_ != identity)
        return NtStatus(NtStatus::path_content_geometry_transformation_not_supported);
    if (!default_gamma()) return NtStatus(NtStatus::gamma_ramp_not_supported);
    return NtStatus(Status::ok);
}

bool PathTransformation::supportable() const
{
    AV_SCOPE("PathTransformation::supportable");
    const bool scaled = scaling_ != uninitialized && scaling_ != identity && scaling_ != unpinned &&
                        scaling_ != not_specified;
    const bool rotated = rotation_ != uninitialized && rotation_ != identity && rotation_ != unpinned &&
                         rotation_ != not_specified;
    return !scaled && !rotated && default_gamma();
}

bool PathTransformation::default_gamma() const
{
    AV_SCOPE("PathTransformation::default_gamma");
    return gamma_ == gamma_uninitialized || gamma_ == gamma_default;
}

}  // namespace av
