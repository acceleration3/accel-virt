#include "miniport/display/overlay_plane.hpp"

#include "miniport/ddi/dxgk_multiplane_overlay_attributes3_record.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

namespace {

/* DXGK_MULTIPLANE_OVERLAY_STRETCH_QUALITY_BILINEAR and _HIGH: both filter. */
constexpr uint32_t stretch_quality_bilinear = 1u;

}  // namespace

OverlayPlane::OverlayPlane()
    : layer_(0u), present_id_(0u), allocation_(nullptr), address_(0), source_{}, destination_{}, clip_{}, flips_(0u),
      blended_(false), filtered_(false)
{
    AV_SCOPE("OverlayPlane::OverlayPlane");
}

OverlayPlane::OverlayPlane(uint32_t layer, uint64_t present_id)
    : layer_(layer), present_id_(present_id), allocation_(nullptr), address_(0), source_{}, destination_{}, clip_{},
      flips_(0u), blended_(false), filtered_(false)
{
    AV_SCOPE("OverlayPlane::OverlayPlane");
}

OverlayPlane::OverlayPlane(uint32_t layer, uint64_t present_id, void *allocation, int64_t address,
                           const DxgkMultiplaneOverlayAttributes3Record &attributes)
    : layer_(layer), present_id_(present_id), allocation_(allocation), address_(address), source_(attributes.SrcRect),
      destination_(attributes.DstRect), clip_(attributes.ClipRect),
      flips_(attributes.Flags & (overlay_vertical_flip | overlay_horizontal_flip)),
      blended_((attributes.Blend & overlay_blend_alpha) != 0u), filtered_(attributes.StretchQuality >= stretch_quality_bilinear)
{
    AV_SCOPE("OverlayPlane::OverlayPlane");
}

uint32_t OverlayPlane::layer() const
{
    AV_SCOPE("OverlayPlane::layer");
    return layer_;
}

uint64_t OverlayPlane::present_id() const
{
    AV_SCOPE("OverlayPlane::present_id");
    return present_id_;
}

bool OverlayPlane::flipped() const
{
    AV_SCOPE("OverlayPlane::flipped");
    return present_id_ != 0u || allocation_ != nullptr;
}

bool OverlayPlane::shown() const
{
    AV_SCOPE("OverlayPlane::shown");
    return allocation_ != nullptr;
}

void *OverlayPlane::allocation() const
{
    AV_SCOPE("OverlayPlane::allocation");
    return allocation_;
}

int64_t OverlayPlane::address() const
{
    AV_SCOPE("OverlayPlane::address");
    return address_;
}

const RectRecord &OverlayPlane::source() const
{
    AV_SCOPE("OverlayPlane::source");
    return source_;
}

const RectRecord &OverlayPlane::destination() const
{
    AV_SCOPE("OverlayPlane::destination");
    return destination_;
}

const RectRecord &OverlayPlane::clip() const
{
    AV_SCOPE("OverlayPlane::clip");
    return clip_;
}

uint32_t OverlayPlane::flips() const
{
    AV_SCOPE("OverlayPlane::flips");
    return flips_;
}

bool OverlayPlane::blended() const
{
    AV_SCOPE("OverlayPlane::blended");
    return blended_;
}

bool OverlayPlane::filtered() const
{
    AV_SCOPE("OverlayPlane::filtered");
    return filtered_;
}

}  // namespace av
