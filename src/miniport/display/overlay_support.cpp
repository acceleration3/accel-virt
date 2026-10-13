#include "miniport/display/overlay_support.hpp"

#include "miniport/ddi/dxgkarg_check_multiplane_overlay_support3_record.hpp"
#include "miniport/ddi/dxgkarg_multiplane_overlay_caps_records.hpp"
#include "miniport/display/display_sources.hpp"
#include "miniport/display/overlay_plane.hpp"
#include "miniport/memory/allocation.hpp"
#include "miniport/memory/pixel_format.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

namespace {

/* 8.0f, 0.125f and 1.0f as IEEE single bits: the factors DXGI compares a
 * plane's scale with, destination over source (COverlayContext::
 * AreSupportedStretchFactors, dwmcore.dll), so the shrink is a fraction. */
constexpr uint32_t factor_max_stretch = 0x41000000u;
constexpr uint32_t factor_max_shrink = 0x3E000000u;
constexpr uint32_t factor_one = 0x3F800000u;
/* D3DDDI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, the desktop's. */
constexpr uint32_t colour_space_srgb = 0u;
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

OverlaySupport::OverlaySupport(Platform &platform, DisplaySources &sources) : platform_(platform), sources_(sources)
{
    AV_SCOPE("OverlaySupport::OverlaySupport");
}

Status OverlaySupport::caps(DxgkargGetMultiPlaneOverlayCapsRecord &caps) const
{
    AV_SCOPE("OverlaySupport::caps");
    platform_.require_irql_at_most(Irql::passive, "OverlaySupport::caps");
    if (caps.VidPnSourceId >= sources_.count()) return Status::invalid_parameter;
    caps.MaxPlanes = OverlayPlane::max_planes;
    caps.MaxRGBPlanes = OverlayPlane::max_planes;
    caps.MaxYUVPlanes = 0u;
    caps.OverlayCaps = overlay_caps_vertical_flip | overlay_caps_horizontal_flip | overlay_caps_stretch_rgb |
                       overlay_caps_bilinear_filter;
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
        bool fits = plane != nullptr && plane->VidPnSourceId < sources_.count() &&
                    plane->LayerIndex < OverlayPlane::max_planes;
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
    if (!layout.is_surface() || PixelFormat(layout.format()).bytes_per_pixel() != 4u || allocation.host_key() == 0u)
        return false;
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
    if (attributes.Rotation != rotation_unset && attributes.Rotation != rotation_identity) return false;
    if (attributes.ColorSpaceType != colour_space_srgb) return false;
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
