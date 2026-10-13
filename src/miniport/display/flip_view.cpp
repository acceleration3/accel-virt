#include "miniport/display/flip_view.hpp"

#include "miniport/ddi/dxgk_multiplane_overlay_vsync_info_record.hpp"
#include "miniport/ddi/dxgk_multiplane_overlay_vsync_info2_record.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

FlipView::FlipView() : kind_(Kind::address), address_(0u), planes_(), attributes_{}
{
    AV_SCOPE("FlipView::FlipView");
}

/* Attributes only a base plane has; a null pointer is none. */
FlipView::FlipView(Kind kind, uint64_t address, const uint32_t *attributes)
    : kind_(kind), address_(address), planes_(), attributes_{}
{
    AV_SCOPE("FlipView::FlipView");
    if (attributes == nullptr) return;
    for (uint32_t word = 0u; word < multiplane_overlay_attribute_words; word++) attributes_[word] = attributes[word];
}

FlipView::FlipView(const OverlayPlane *planes) : kind_(Kind::presented_plane), address_(0u), planes_(), attributes_{}
{
    AV_SCOPE("FlipView::FlipView");
    for (uint32_t layer = 0u; layer < OverlayPlane::max_planes; layer++) planes_[layer] = planes[layer];
    address_ = static_cast<uint64_t>(planes_[0].address());
}

FlipView::Kind FlipView::kind() const
{
    AV_SCOPE("FlipView::kind");
    return kind_;
}

uint64_t FlipView::address() const
{
    AV_SCOPE("FlipView::address");
    return address_;
}

void FlipView::layer(DxgkMultiplaneOverlayVsyncInfoRecord &layer) const
{
    AV_SCOPE("FlipView::layer");
    layer = DxgkMultiplaneOverlayVsyncInfoRecord{};
    layer.Enabled = 1u;
    layer.PhysicalAddress = static_cast<int64_t>(address_);
    for (uint32_t word = 0u; word < multiplane_overlay_attribute_words; word++) layer.PlaneAttributes[word] = attributes_[word];
}

uint32_t FlipView::layers(DxgkMultiplaneOverlayVsyncInfo2Record *layers) const
{
    AV_SCOPE("FlipView::layers");
    uint32_t count = 0u;
    for (const OverlayPlane &plane : planes_) {
        if (!plane.flipped()) continue;
        layers[count] = DxgkMultiplaneOverlayVsyncInfo2Record{};
        layers[count].LayerIndex = plane.layer();
        layers[count].PresentId = plane.present_id();
        count++;
    }
    return count;
}

const OverlayPlane &FlipView::plane(uint32_t layer) const
{
    AV_SCOPE("FlipView::plane");
    return planes_[layer < OverlayPlane::max_planes ? layer : 0u];
}

}  // namespace av
