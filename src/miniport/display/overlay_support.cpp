#include "miniport/display/overlay_support.hpp"

#include "miniport/ddi/dxgkarg_check_multiplane_overlay_support3_record.hpp"
#include "miniport/ddi/dxgkarg_multiplane_overlay_caps_records.hpp"
#include "miniport/display/display_sources.hpp"
#include "miniport/display/overlay_plane.hpp"
#include "miniport/memory/allocation.hpp"
#include "miniport/memory/pixel_format.hpp"
#include "proto/context/capset_info.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

namespace {

/* 8.0f, 0.125f and 1.0f as IEEE single bits: the factors DXGI compares a
 * plane's scale with, destination over source (COverlayContext::
 * AreSupportedStretchFactors, dwmcore.dll), so the shrink is a fraction. */
constexpr uint32_t factor_max_stretch = 0x41000000u;
constexpr uint32_t factor_max_shrink = 0x3E000000u;
constexpr uint32_t factor_one = 0x3F800000u;
/* D3DDDI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, the desktop's, and the YCbCr
 * spaces of BT.601 and BT.709 video, studio and full range, which the host
 * converts (PlaneScanout). */
constexpr uint32_t colour_space_srgb = 0u;
constexpr uint32_t first_ycbcr_space = 5u, last_ycbcr_space = 9u;
/* D3DDDIFMT_NV12 and D3DDDIFMT_YUY2, the video surfaces a plane shows. */
constexpr uint32_t format_nv12 = 0x3231564Eu, format_yuy2 = 0x32595559u;
/* D3DDDI_ROTATION_IDENTITY, or 0 for one never set. */
constexpr uint32_t rotation_unset = 0u;

int64_t extent(int32_t from, int32_t to)
{
    AV_SCOPE("extent");
    return static_cast<int64_t>(to) - static_cast<int64_t>(from);
}

/* Destination over source no further than max_scale either way. */
bool scaled_within(int64_t source, int64_t destination)
{
    AV_SCOPE("scaled_within");
    return destination <= source * OverlaySupport::max_scale && source <= destination * OverlaySupport::max_scale;
}

}  // namespace

OverlaySupport::OverlaySupport(Platform &platform, DisplaySources &sources)
    : platform_(platform), sources_(sources), composed_(false)
{
    AV_SCOPE("OverlaySupport::OverlaySupport");
}

void OverlaySupport::offer(const CapsetInfo &capset)
{
    AV_SCOPE("OverlaySupport::offer");
    platform_.require_irql_at_most(Irql::passive, "OverlaySupport::offer");
    composed_ = capset.has_feature(CapsetInfo::feature_scanout_planes);
}

uint32_t OverlaySupport::planes() const
{
    AV_SCOPE("OverlaySupport::planes");
    return composed_ ? OverlayPlane::max_planes : 1u;
}

Status OverlaySupport::caps(DxgkargGetMultiPlaneOverlayCapsRecord &caps) const
{
    AV_SCOPE("OverlaySupport::caps");
    platform_.require_irql_at_most(Irql::passive, "OverlaySupport::caps");
    if (caps.VidPnSourceId >= sources_.count()) return Status::invalid_parameter;
    caps.MaxPlanes = planes();
    caps.MaxRGBPlanes = planes();
    /* Video goes over the desktop, never under it: every plane but the base. */
    caps.MaxYUVPlanes = planes() - 1u;
    caps.OverlayCaps = overlay_caps_stretch_rgb | overlay_caps_bilinear_filter |
                       (caps.MaxYUVPlanes != 0u ? overlay_caps_stretch_yuv : 0u);
    caps.MaxStretchFactor = factor_max_stretch;
    caps.MaxShrinkFactor = factor_max_shrink;
    return Status::ok;
}

Status OverlaySupport::post_composition_caps(DxgkargGetPostCompositionCapsRecord &caps) const
{
    AV_SCOPE("OverlaySupport::post_composition_caps");
    platform_.require_irql_at_most(Irql::passive, "OverlaySupport::post_composition_caps");
    if (caps.VidPnSourceId >= sources_.count()) return Status::invalid_parameter;
    caps.MaxStretchFactor = factor_one;
    caps.MaxShrinkFactor = factor_one;
    return Status::ok;
}

/* dxgkrnl hands every source's planes at once, the query merged into what is
 * on screen (CheckMultiPlaneOverlayInternal3, dxgkrnl.sys), so the limit on
 * planes is per source and each layer may appear once in it. */
Status OverlaySupport::check(DxgkargCheckMultiPlaneOverlaySupport3Record &check) const
{
    AV_SCOPE("OverlaySupport::check");
    platform_.require_irql_at_most(Irql::passive, "OverlaySupport::check");
    if ((check.PlaneCount != 0u && check.ppPlanes == nullptr) ||
        (check.PostCompositionCount != 0u && check.ppPostComposition == nullptr))
        return Status::invalid_parameter;
    check.Supported = 1;
    check.ReturnInfo = 0u;
    uint32_t layers[DisplaySources::max_sources] = {};
    for (uint32_t index = 0u; index < check.PlaneCount; index++) {
        const DxgkMultiplaneOverlayPlaneWithSource2Record *const plane = check.ppPlanes[index];
        bool fits = plane != nullptr && plane->VidPnSourceId < sources_.count() && plane->LayerIndex < planes();
        if (fits) {
            const uint32_t layer = 1u << plane->LayerIndex;
            fits = (layers[plane->VidPnSourceId] & layer) == 0u && shows(*plane);
            layers[plane->VidPnSourceId] |= layer;
        }
        if (fits) continue;
        check.Supported = 0;
        check.ReturnInfo = index & check_failing_plane_mask;
        return Status::ok;
    }
    for (uint32_t index = 0u; index < check.PostCompositionCount; index++) {
        const DxgkMultiplaneOverlayPostCompositionWithSourceRecord *const composition = check.ppPostComposition[index];
        if (composition == nullptr || !shows(*composition)) check.Supported = 0;
    }
    return Status::ok;
}

bool OverlaySupport::shows(const DxgkMultiplaneOverlayPlaneWithSource2Record &plane) const
{
    AV_SCOPE("OverlaySupport::shows");
    if (plane.hAllocation == nullptr) return false;
    const auto &allocation = *static_cast<const Allocation *>(plane.hAllocation);
    const SurfaceLayout &layout = allocation.descriptor().layout();
    const bool rgb = PixelFormat(layout.format()).bytes_per_pixel() == 4u;
    const bool video = layout.format() == format_nv12 || layout.format() == format_yuy2;
    if (!layout.is_surface() || (!rgb && !video) || allocation.host_key() == 0u) return false;
    const DxgkMultiplaneOverlayAttributes3Record &attributes = plane.PlaneAttributes;
    const RectRecord &source = attributes.SrcRect;
    const RectRecord &destination = attributes.DstRect;
    const int64_t source_width = extent(source.left, source.right);
    const int64_t source_height = extent(source.top, source.bottom);
    const int64_t destination_width = extent(destination.left, destination.right);
    const int64_t destination_height = extent(destination.top, destination.bottom);
    if (source.left < 0 || source.top < 0 || source_width <= 0 || source_height <= 0 ||
        source.right > static_cast<int64_t>(layout.width()) || source.bottom > static_cast<int64_t>(layout.height()))
        return false;
    if (destination_width <= 0 || destination_height <= 0) return false;
    /* The host's composer neither rotates nor mirrors a plane. */
    if (attributes.Rotation != rotation_unset && attributes.Rotation != rotation_identity) return false;
    if ((attributes.Flags & (overlay_vertical_flip | overlay_horizontal_flip)) != 0u) return false;
    const bool ycbcr = attributes.ColorSpaceType >= first_ycbcr_space && attributes.ColorSpaceType <= last_ycbcr_space;
    if (rgb ? attributes.ColorSpaceType != colour_space_srgb : !ycbcr || plane.LayerIndex == 0u) return false;
    return scaled_within(source_width, destination_width) && scaled_within(source_height, destination_height);
}

bool OverlaySupport::shows(const DxgkMultiplaneOverlayPostCompositionWithSourceRecord &composition) const
{
    AV_SCOPE("OverlaySupport::shows");
    const RectRecord &source = composition.SrcRect;
    const RectRecord &destination = composition.DstRect;
    return composition.VidPnSourceId < sources_.count() && composition.Flags == 0u &&
           (composition.Rotation == rotation_unset || composition.Rotation == rotation_identity) &&
           extent(source.left, source.right) == extent(destination.left, destination.right) &&
           extent(source.top, source.bottom) == extent(destination.top, destination.bottom);
}

}  // namespace av
