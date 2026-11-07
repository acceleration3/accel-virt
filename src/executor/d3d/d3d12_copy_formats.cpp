#include "executor/d3d/d3d12_copy_formats.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint32_t eight_bytes = 8u;
constexpr uint32_t sixteen_bytes = 16u;
constexpr uint32_t block_side = 4u;
constexpr uint32_t family_r32g32b32a32 = 1u;
constexpr uint32_t family_r16g16b16a16 = 9u;
constexpr uint32_t family_r32g32 = 15u;

struct PlaneRow {
    uint32_t texture_;
    uint32_t plane_;
    uint32_t footprint_;
};

constexpr PlaneRow plane_rows[] = {
    {19u, 0u, 39u},  {19u, 0u, 41u},  {19u, 0u, 21u},  {19u, 1u, 60u},  {19u, 1u, 62u},  {19u, 1u, 61u},
    {19u, 1u, 22u},  {20u, 0u, 39u},  {20u, 0u, 40u},  {20u, 0u, 41u},  {20u, 0u, 21u},  {20u, 1u, 60u},
    {20u, 1u, 62u},  {20u, 1u, 61u},  {20u, 1u, 22u},  {44u, 0u, 39u},  {44u, 0u, 46u},  {44u, 1u, 60u},
    {44u, 1u, 62u},  {44u, 1u, 61u},  {44u, 1u, 47u},  {45u, 0u, 39u},  {45u, 0u, 46u},  {45u, 1u, 60u},
    {45u, 1u, 62u},  {45u, 1u, 61u},  {45u, 1u, 47u},  {103u, 0u, 60u}, {103u, 0u, 61u}, {103u, 0u, 62u},
    {103u, 1u, 48u}, {103u, 1u, 49u}, {103u, 1u, 50u}, {104u, 0u, 53u}, {104u, 0u, 56u}, {104u, 0u, 57u},
    {104u, 1u, 33u}, {104u, 1u, 35u}, {104u, 1u, 36u}, {105u, 0u, 53u}, {105u, 0u, 56u}, {105u, 0u, 57u},
    {105u, 1u, 33u}, {105u, 1u, 35u}, {105u, 1u, 36u},
};

bool planar(uint32_t format)
{
    AV_SCOPE("planar");
    for (const PlaneRow &row : plane_rows)
        if (row.texture_ == format) return true;
    return false;
}

}

D3d12CopyFormats::D3d12CopyFormats(uint32_t destination, uint32_t source)
    : destination_format_(destination), source_format_(source), destination_(destination), source_(source)
{
    AV_SCOPE("D3d12CopyFormats::D3d12CopyFormats");
}

bool D3d12CopyFormats::block_pair() const
{
    AV_SCOPE("D3d12CopyFormats::block_pair");
    if (!destination_.known() || !source_.known() || destination_.depth() || source_.depth()) return false;
    const uint32_t bytes = destination_.element_bytes();
    if (bytes != source_.element_bytes() || (bytes != eight_bytes && bytes != sixteen_bytes)) return false;
    const bool destination_block = destination_.element_width() == block_side && destination_.element_height() == block_side;
    const bool source_block = source_.element_width() == block_side && source_.element_height() == block_side;
    const bool destination_plain = destination_.element_width() == 1u && destination_.element_height() == 1u;
    const bool source_plain = source_.element_width() == 1u && source_.element_height() == 1u;
    if (!(destination_block && source_plain) && !(destination_plain && source_block)) return false;
    const DxgiFormat &plain = destination_plain ? destination_ : source_;
    if (bytes == sixteen_bytes) return plain.family() == family_r32g32b32a32;
    return plain.family() == family_r16g16b16a16 || plain.family() == family_r32g32;
}

bool D3d12CopyFormats::same_layout() const
{
    AV_SCOPE("D3d12CopyFormats::same_layout");
    if (!destination_.known() || !source_.known() || destination_.depth() || source_.depth()) return false;
    if (planar(destination_format_) || planar(source_format_)) return false;
    return destination_.element_bytes() != 0u && destination_.element_bytes() == source_.element_bytes() &&
           destination_.element_width() == source_.element_width() &&
           destination_.element_height() == source_.element_height();
}

bool D3d12CopyFormats::same_texels() const
{
    AV_SCOPE("D3d12CopyFormats::same_texels");
    if (!destination_.known() || !source_.known()) return false;
    if (destination_format_ == source_format_) return true;
    if (destination_.element_bytes() == 0u || source_.element_bytes() == 0u) return false;
    if (destination_.family() != 0u && destination_.family() == source_.family()) return true;
    return block_pair();
}

bool D3d12CopyFormats::scaled() const
{
    AV_SCOPE("D3d12CopyFormats::scaled");
    return block_pair();
}

uint32_t D3d12CopyFormats::destination_extent(uint32_t source_texels, bool horizontal) const
{
    AV_SCOPE("D3d12CopyFormats::destination_extent");
    if (!block_pair()) return source_texels;
    const uint32_t from = horizontal ? source_.element_width() : source_.element_height();
    const uint32_t into = horizontal ? destination_.element_width() : destination_.element_height();
    const uint64_t blocks = (uint64_t{source_texels} + from - 1u) / from;
    const uint64_t texels = blocks * into;
    return texels > 0xFFFFFFFFu ? 0xFFFFFFFFu : static_cast<uint32_t>(texels);
}

d3d12proto::Box D3d12CopyFormats::rescaled(const d3d12proto::Box &written) const
{
    AV_SCOPE("D3d12CopyFormats::rescaled");
    if (!block_pair() || written.right < written.left || written.bottom < written.top) return written;
    d3d12proto::Box made = written;
    const uint64_t  right = uint64_t{written.left} + destination_extent(written.right - written.left, true);
    const uint64_t  bottom = uint64_t{written.top} + destination_extent(written.bottom - written.top, false);
    made.right = right > 0xFFFFFFFFu ? 0xFFFFFFFFu : static_cast<uint32_t>(right);
    made.bottom = bottom > 0xFFFFFFFFu ? 0xFFFFFFFFu : static_cast<uint32_t>(bottom);
    return made;
}

bool D3d12CopyFormats::footprint_of_plane(uint32_t plane) const
{
    AV_SCOPE("D3d12CopyFormats::footprint_of_plane");
    if (!planar(destination_format_)) return plane == 0u && (same_texels() || same_layout());
    for (const PlaneRow &row : plane_rows)
        if (row.texture_ == destination_format_ && row.plane_ == plane && row.footprint_ == source_format_) return true;
    return false;
}

}
