#include "icd/opengl/pixel_format_table.hpp"

#include "core/trace/function_scope.hpp"

namespace av::glicd {

namespace {

constexpr uint16_t descriptor_version = 1u;
constexpr uint8_t  type_rgba = 0u;
constexpr uint8_t  color_bits = 32u;
constexpr uint8_t  channel_bits = 8u;
/* BGRA in memory: blue is the lowest byte. */
constexpr uint8_t  red_shift = 16u;
constexpr uint8_t  green_shift = 8u;
constexpr uint8_t  blue_shift = 0u;
constexpr uint8_t  alpha_shift = 24u;
constexpr uint8_t  multisampled = 4u;

constexpr uint32_t at_size = 0u;
constexpr uint32_t at_version = 2u;
constexpr uint32_t at_flags = 4u;
constexpr uint32_t at_pixel_type = 8u;
constexpr uint32_t at_color_bits = 9u;
constexpr uint32_t at_red_bits = 10u;
constexpr uint32_t at_depth_bits = 23u;
constexpr uint32_t at_stencil_bits = 24u;

void put16(uint8_t *bytes, uint32_t at, uint16_t value)
{
    AV_SCOPE("put16");
    bytes[at] = static_cast<uint8_t>(value);
    bytes[at + 1u] = static_cast<uint8_t>(value >> 8u);
}

void put32(uint8_t *bytes, uint32_t at, uint32_t value)
{
    AV_SCOPE("put32");
    for (uint32_t shift = 0u; shift < 32u; shift += 8u) bytes[at + shift / 8u] = static_cast<uint8_t>(value >> shift);
}

}  // namespace

/* Double-buffered D24S8 first: what an application asking for nothing
 * special matches first. */
PixelFormatTable::PixelFormatTable()
    : rows_{PixelFormat(24u, 8u, true, false, 0u),  PixelFormat(24u, 8u, false, false, 0u),
            PixelFormat(32u, 0u, true, false, 0u),  PixelFormat(32u, 0u, false, false, 0u),
            PixelFormat(0u, 0u, true, false, 0u),   PixelFormat(0u, 0u, false, false, 0u),
            PixelFormat(24u, 8u, true, true, 0u),   PixelFormat(24u, 8u, false, true, 0u),
            PixelFormat(32u, 0u, true, true, 0u),   PixelFormat(32u, 0u, false, true, 0u),
            PixelFormat(0u, 0u, true, true, 0u),    PixelFormat(0u, 0u, false, true, 0u),
            PixelFormat(24u, 8u, true, false, multisampled),  PixelFormat(24u, 8u, false, false, multisampled),
            PixelFormat(32u, 0u, true, false, multisampled),  PixelFormat(32u, 0u, false, false, multisampled),
            PixelFormat(0u, 0u, true, false, multisampled),   PixelFormat(0u, 0u, false, false, multisampled),
            PixelFormat(24u, 8u, true, true, multisampled),   PixelFormat(24u, 8u, false, true, multisampled),
            PixelFormat(32u, 0u, true, true, multisampled),   PixelFormat(32u, 0u, false, true, multisampled),
            PixelFormat(0u, 0u, true, true, multisampled),    PixelFormat(0u, 0u, false, true, multisampled)}
            {
                AV_SCOPE("PixelFormatTable::PixelFormatTable");
            }

uint32_t PixelFormatTable::count() const
{
    AV_SCOPE("PixelFormatTable::count");
    return row_count;
}

bool PixelFormatTable::contains(int32_t index) const
{
    AV_SCOPE("PixelFormatTable::contains");
    return index >= 1 && static_cast<uint32_t>(index) <= row_count;
}

const PixelFormat &PixelFormatTable::format(int32_t index) const
{
    AV_SCOPE("PixelFormatTable::format");
    return rows_[static_cast<uint32_t>(index) - 1u];
}

int32_t PixelFormatTable::describe(int32_t index, uint32_t bytes, uint8_t *descriptor) const
{
    AV_SCOPE("PixelFormatTable::describe");
    if (contains(index) && descriptor != nullptr && bytes >= descriptor_bytes) {
        const PixelFormat &row = format(index);
        for (uint32_t at = 0u; at < descriptor_bytes; at++) descriptor[at] = 0u;
        put16(descriptor, at_size, descriptor_bytes);
        put16(descriptor, at_version, descriptor_version);
        put32(descriptor, at_flags, row.flags());
        descriptor[at_pixel_type] = type_rgba;
        descriptor[at_color_bits] = color_bits;
        const uint8_t channels[] = {channel_bits, red_shift,  channel_bits, green_shift,
                                    channel_bits, blue_shift, channel_bits, alpha_shift};
        for (uint32_t at = 0u; at < sizeof channels; at++) descriptor[at_red_bits + at] = channels[at];
        descriptor[at_depth_bits] = row.depth_bits();
        descriptor[at_stencil_bits] = row.stencil_bits();
    }
    return static_cast<int32_t>(row_count);
}

bool PixelFormatTable::invariants_hold() const
{
    AV_SCOPE("PixelFormatTable::invariants_hold");
    for (uint32_t index = 0u; index < row_count; index++) {
        const PixelFormat &row = rows_[index];
        const bool    no_depth = row.depth_bits() == 0u && row.stencil_bits() == 0u;
        const bool    d24s8 = row.depth_bits() == 24u && row.stencil_bits() == 8u;
        const bool    d32 = row.depth_bits() == 32u && row.stencil_bits() == 0u;
        if (!no_depth && !d24s8 && !d32) return false;
        if (row.samples() != 0u && row.samples() != multisampled) return false;
        /* Each depth has one stencil size, checked above, so the depth tells rows apart. */
        for (uint32_t earlier = 0u; earlier < index; earlier++) {
            const PixelFormat &other = rows_[earlier];
            if (other.depth_bits() == row.depth_bits() &&
                other.double_buffer() == row.double_buffer() && other.srgb() == row.srgb() &&
                other.samples() == row.samples())
                return false;
        }
    }
    return true;
}

}  // namespace av::glicd
