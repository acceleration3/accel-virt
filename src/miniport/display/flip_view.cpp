#include "miniport/display/flip_view.hpp"

#include "miniport/ddi/dxgk_multiplane_overlay_vsync_info_record.hpp"
#include "miniport/ddi/dxgk_multiplane_overlay_vsync_info2_record.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

FlipView::FlipView() : kind_(Kind::address), address_(0u), present_id_(0u), attributes_{}
{
    AV_SCOPE("FlipView::FlipView");
}

/* Attributes only a base plane has; a null pointer is none. */
FlipView::FlipView(Kind kind, uint64_t address, const uint32_t *attributes)
    : kind_(kind), address_(address), present_id_(0u), attributes_{}
{
    AV_SCOPE("FlipView::FlipView");
    if (attributes == nullptr) return;
    for (uint32_t word = 0u; word < multiplane_overlay_attribute_words; word++) attributes_[word] = attributes[word];
}

FlipView::FlipView(uint64_t address, uint64_t present_id)
    : kind_(Kind::presented_plane), address_(address), present_id_(present_id), attributes_{}
{
    AV_SCOPE("FlipView::FlipView");
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

void FlipView::layer(DxgkMultiplaneOverlayVsyncInfo2Record &layer) const
{
    AV_SCOPE("FlipView::layer");
    layer = DxgkMultiplaneOverlayVsyncInfo2Record{};
    layer.PresentId = present_id_;
}

}  // namespace av
