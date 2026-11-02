#include "executor/d3d/d3d12_footprint.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/d3d/dxgi_format_layout.hpp"
#include "proto/resources/pitched_extent.hpp"

namespace av {

namespace {

constexpr uint32_t most_edge = 16384u;
constexpr uint32_t most_depth = 2048u;

}

D3d12Footprint::D3d12Footprint(uint32_t format, uint32_t width, uint32_t height, uint32_t depth,
                               uint32_t row_pitch, uint64_t offset)
    : layout_(format), width_(width), height_(height), depth_(depth), row_pitch_(row_pitch), offset_(offset)
{
    AV_SCOPE("D3d12Footprint::D3d12Footprint");
}

d3d12proto::Box D3d12Footprint::whole() const
{
    AV_SCOPE("D3d12Footprint::whole");
    return d3d12proto::Box{0u, 0u, 0u, width_, height_, depth_};
}

bool D3d12Footprint::holds(const d3d12proto::Box &region, uint64_t buffer_bytes) const
{
    AV_SCOPE("D3d12Footprint::holds");
    if (width_ > most_edge || height_ > most_edge || depth_ > most_depth || offset_ > buffer_bytes) return false;
    if (region.left > region.right || region.right > width_ || region.top > region.bottom ||
        region.bottom > height_ || region.front > region.back || region.back > depth_)
        return false;
    if (region.left == region.right || region.top == region.bottom || region.front == region.back) return true;
    const uint64_t slice = static_cast<uint64_t>(row_pitch_) * layout_.rows(height_);
    return PitchedExtent(layout_.copied_row_bytes(region.right), layout_.rows(region.bottom), region.back, row_pitch_, slice)
        .within(buffer_bytes - offset_);
}

}
