#include "core/display/cursor_image.hpp"

#include "core/ddi/dxgkarg_set_pointer_shape_record.hpp"

namespace av {

namespace {

constexpr uint8_t opaque = 0xFFu;
constexpr uint32_t bits_per_byte = 8u;

}  // namespace

CursorImage::CursorImage(Platform &platform, const DxgkargSetPointerShapeRecord &shape)
    : platform_(platform), shape_(shape) {}

Status CursorImage::validate() const
{
    const DxgkargSetPointerShapeRecord &s = shape_;
    const bool monochrome = s.Flags == flag_monochrome;
    if (!monochrome && s.Flags != flag_color && s.Flags != flag_masked_color) return Status::invalid_parameter;
    if (s.Width == 0u || s.Height == 0u || s.Width > side || s.Height > side || s.pPixels == nullptr)
        return Status::invalid_parameter;
    const uint32_t row_bytes = monochrome ? (s.Width + bits_per_byte - 1u) / bits_per_byte : s.Width * 4u;
    if (s.Pitch < row_bytes || s.XHot >= s.Width || s.YHot >= s.Height) return Status::invalid_parameter;
    return Status::ok;
}

Status CursorImage::draw(uint8_t *pixels) const
{
    platform_.require_irql_at_most(Irql::passive, "CursorImage::draw");
    const Status status = validate();
    if (status != Status::ok) return status;
    platform_.zero(pixels, pixel_bytes);
    if (shape_.Flags == flag_monochrome) draw_monochrome(pixels);
    else draw_color(pixels);
    return Status::ok;
}

/* The AND mask's rows, then the XOR mask's; a set AND bit keeps the screen,
 * which with a set XOR bit inverts it. */
void CursorImage::draw_monochrome(uint8_t *pixels) const
{
    const auto *const masks = static_cast<const uint8_t *>(shape_.pPixels);
    const uint64_t xor_start = uint64_t{shape_.Pitch} * shape_.Height;
    for (uint32_t y = 0u; y < shape_.Height; y++) {
        for (uint32_t x = 0u; x < shape_.Width; x++) {
            const uint64_t byte = uint64_t{shape_.Pitch} * y + x / bits_per_byte;
            const uint8_t bit = static_cast<uint8_t>(0x80u >> (x % bits_per_byte));
            const bool keep = (masks[byte] & bit) != 0u;
            const bool flip = (masks[xor_start + byte] & bit) != 0u;
            const uint8_t shade = !keep && flip ? opaque : 0u;
            uint8_t *const out = &pixels[(y * side + x) * 4u];
            out[0] = shade;
            out[1] = shade;
            out[2] = shade;
            out[3] = keep && !flip ? 0u : opaque;
        }
    }
}

/* Masked colour: a non-zero mask over black keeps the screen; over a colour
 * it inverts, shown as the colour. */
void CursorImage::draw_color(uint8_t *pixels) const
{
    const auto *const source = static_cast<const uint8_t *>(shape_.pPixels);
    const bool masked = shape_.Flags == flag_masked_color;
    for (uint32_t y = 0u; y < shape_.Height; y++) {
        for (uint32_t x = 0u; x < shape_.Width; x++) {
            const uint8_t *const in = &source[uint64_t{shape_.Pitch} * y + x * 4u];
            uint8_t *const out = &pixels[(y * side + x) * 4u];
            const bool black = (in[0] | in[1] | in[2]) == 0u;
            out[0] = in[0];
            out[1] = in[1];
            out[2] = in[2];
            out[3] = !masked ? in[3] : (in[3] != 0u && black ? 0u : opaque);
        }
    }
}

}  // namespace av
