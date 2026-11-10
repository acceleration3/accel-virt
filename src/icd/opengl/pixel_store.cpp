#include "icd/opengl/pixel_store.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/resources/pitched_extent.hpp"

namespace av::glicd {

namespace {

enum : uint32_t {
    gl_color_index = 0x1900u, gl_luminance = 0x1909u, gl_luminance_alpha = 0x190Au, gl_alpha_integer = 0x8D97u,
    gl_stencil_index = 0x1901u, gl_depth_component = 0x1902u, gl_red = 0x1903u, gl_green = 0x1904u,
    gl_blue = 0x1905u, gl_alpha = 0x1906u, gl_rgb = 0x1907u, gl_rgba = 0x1908u, gl_bgr = 0x80E0u,
    gl_bgra = 0x80E1u, gl_rg = 0x8227u, gl_rg_integer = 0x8228u, gl_depth_stencil = 0x84F9u,
    gl_red_integer = 0x8D94u, gl_green_integer = 0x8D95u, gl_blue_integer = 0x8D96u, gl_rgb_integer = 0x8D98u,
    gl_rgba_integer = 0x8D99u, gl_bgr_integer = 0x8D9Au, gl_bgra_integer = 0x8D9Bu,
};

enum : uint32_t {
    gl_byte = 0x1400u, gl_unsigned_byte = 0x1401u, gl_short = 0x1402u, gl_unsigned_short = 0x1403u,
    gl_int = 0x1404u, gl_unsigned_int = 0x1405u, gl_float = 0x1406u, gl_half_float = 0x140Bu,
    gl_unsigned_byte_3_3_2 = 0x8032u, gl_unsigned_byte_2_3_3_rev = 0x8362u,
    gl_unsigned_short_4_4_4_4 = 0x8033u, gl_unsigned_short_5_5_5_1 = 0x8034u, gl_unsigned_short_5_6_5 = 0x8363u,
    gl_unsigned_short_5_6_5_rev = 0x8364u, gl_unsigned_short_4_4_4_4_rev = 0x8365u,
    gl_unsigned_short_1_5_5_5_rev = 0x8366u,
    gl_unsigned_int_8_8_8_8 = 0x8035u, gl_unsigned_int_10_10_10_2 = 0x8036u, gl_unsigned_int_8_8_8_8_rev = 0x8367u,
    gl_unsigned_int_2_10_10_10_rev = 0x8368u, gl_unsigned_int_24_8 = 0x84FAu,
    gl_unsigned_int_10f_11f_11f_rev = 0x8C3Bu, gl_unsigned_int_5_9_9_9_rev = 0x8C3Eu,
    gl_float_32_unsigned_int_24_8_rev = 0x8DADu,
};

constexpr int32_t default_alignment = 4;

}

PixelStore::PixelStore() : values_{default_alignment, 0, 0, 0, 0, 0}
{
    AV_SCOPE("PixelStore::PixelStore");
}

bool PixelStore::set(Field field, int32_t value)
{
    AV_SCOPE("PixelStore::set");
    const bool accepted = field == Field::alignment ? value == 1 || value == 2 || value == 4 || value == 8 : value >= 0;
    if (accepted) values_[static_cast<uint32_t>(field)] = value;
    return accepted;
}

int32_t PixelStore::get(Field field) const
{
    AV_SCOPE("PixelStore::get");
    return values_[static_cast<uint32_t>(field)];
}

size_t PixelStore::span(uint32_t format, uint32_t type, int32_t width, int32_t height, int32_t depth) const
{
    AV_SCOPE("PixelStore::span");
    const size_t count = components(format);
    const size_t packed = packed_bytes(type);
    const size_t value = packed != 0u ? packed : value_bytes(type);
    if (count == 0u || value == 0u || width <= 0 || height <= 0 || depth <= 0) return 0u;
    if (packed != 0u ? !packed_fits(format, type) : format == gl_depth_stencil) return 0u;
    const size_t values_per_pixel = packed != 0u ? 1u : count;
    const size_t pixel = values_per_pixel * value;
    const size_t alignment = static_cast<size_t>(get(Field::alignment));
    const uint64_t row_pixels = static_cast<uint64_t>(get(Field::row_length) > 0 ? get(Field::row_length) : width);
    uint64_t       row = row_pixels * pixel;
    if (value < alignment) row = (row + alignment - 1u) / alignment * alignment;
    const uint64_t image_rows = static_cast<uint64_t>(get(Field::image_height) > 0 ? get(Field::image_height) : height);
    const uint64_t skipped_images = depth == 1 ? 0u : static_cast<uint64_t>(get(Field::skip_images));
    uint64_t       image = 0u, start = 0u, end = 0u;
    if (__builtin_mul_overflow(image_rows, row, &image)) return 0u;
    const PitchedExtent extent(static_cast<uint64_t>(width) * pixel, static_cast<uint64_t>(height),
                               static_cast<uint64_t>(depth), row, image);
    if (!extent.offset_of(static_cast<uint64_t>(get(Field::skip_rows)), skipped_images, start) ||
        __builtin_add_overflow(start, static_cast<uint64_t>(get(Field::skip_pixels)) * pixel, &start) ||
        !extent.end(start, end))
        return 0u;
    if constexpr (sizeof(size_t) < sizeof(uint64_t))
        if (end > SIZE_MAX) return 0u;
    return static_cast<size_t>(end);
}

bool PixelStore::layout(uint32_t format, uint32_t type, int32_t width, int32_t height, uint64_t &start,
                        uint64_t &row_bytes, uint64_t &stride) const
{
    AV_SCOPE("PixelStore::layout");
    const size_t count = components(format);
    const size_t packed = packed_bytes(type);
    const size_t value = packed != 0u ? packed : value_bytes(type);
    if (count == 0u || value == 0u || width <= 0 || height <= 0) return false;
    const size_t   pixel = (packed != 0u ? 1u : count) * value;
    const uint64_t alignment = static_cast<uint64_t>(get(Field::alignment));
    const uint64_t row_pixels = static_cast<uint64_t>(get(Field::row_length) > 0 ? get(Field::row_length) : width);
    stride = row_pixels * pixel;
    if (value < alignment) stride = (stride + alignment - 1u) / alignment * alignment;
    row_bytes = static_cast<uint64_t>(width) * pixel;
    start = static_cast<uint64_t>(get(Field::skip_rows)) * stride + static_cast<uint64_t>(get(Field::skip_pixels)) * pixel;
    return true;
}

size_t PixelStore::bitmap_span(int32_t width, int32_t height) const
{
    AV_SCOPE("PixelStore::bitmap_span");
    if (width <= 0 || height <= 0) return 0u;
    const uint64_t columns = static_cast<uint64_t>(get(Field::row_length) > 0 ? get(Field::row_length) : width);
    const uint64_t alignment = static_cast<uint64_t>(get(Field::alignment));
    uint64_t       stride = (columns + 7u) / 8u;
    stride = (stride + alignment - 1u) / alignment * alignment;
    uint64_t lines = 0u, bytes = 0u;
    const uint64_t last = (static_cast<uint64_t>(get(Field::skip_pixels)) + static_cast<uint64_t>(width) + 7u) / 8u;
    if (__builtin_mul_overflow(static_cast<uint64_t>(get(Field::skip_rows)) + static_cast<uint64_t>(height - 1), stride,
                               &lines) ||
        __builtin_add_overflow(lines, last, &bytes))
        return 0u;
    if constexpr (sizeof(size_t) < sizeof(uint64_t))
        if (bytes > SIZE_MAX) return 0u;
    return static_cast<size_t>(bytes);
}

bool PixelStore::invariants_hold() const
{
    AV_SCOPE("PixelStore::invariants_hold");
    const int32_t alignment = values_[0];
    if (alignment != 1 && alignment != 2 && alignment != 4 && alignment != 8) return false;
    for (uint32_t field = 1u; field < fields; field++)
        if (values_[field] < 0) return false;
    return true;
}

size_t PixelStore::components(uint32_t format) const
{
    AV_SCOPE("PixelStore::components");
    switch (format) {
    case gl_color_index:
    case gl_luminance:
    case gl_alpha_integer:
    case gl_stencil_index:
    case gl_depth_component:
    case gl_red:
    case gl_green:
    case gl_blue:
    case gl_alpha:
    case gl_red_integer:
    case gl_green_integer:
    case gl_blue_integer:
        return 1u;
    case gl_rg:
    case gl_rg_integer:
    case gl_luminance_alpha:
    case gl_depth_stencil:
        return 2u;
    case gl_rgb:
    case gl_bgr:
    case gl_rgb_integer:
    case gl_bgr_integer:
        return 3u;
    case gl_rgba:
    case gl_bgra:
    case gl_rgba_integer:
    case gl_bgra_integer:
        return 4u;
    default:
        return 0u;
    }
}

size_t PixelStore::packed_bytes(uint32_t type) const
{
    AV_SCOPE("PixelStore::packed_bytes");
    switch (type) {
    case gl_unsigned_byte_3_3_2:
    case gl_unsigned_byte_2_3_3_rev:
        return 1u;
    case gl_unsigned_short_4_4_4_4:
    case gl_unsigned_short_5_5_5_1:
    case gl_unsigned_short_5_6_5:
    case gl_unsigned_short_5_6_5_rev:
    case gl_unsigned_short_4_4_4_4_rev:
    case gl_unsigned_short_1_5_5_5_rev:
        return 2u;
    case gl_unsigned_int_8_8_8_8:
    case gl_unsigned_int_10_10_10_2:
    case gl_unsigned_int_8_8_8_8_rev:
    case gl_unsigned_int_2_10_10_10_rev:
    case gl_unsigned_int_24_8:
    case gl_unsigned_int_10f_11f_11f_rev:
    case gl_unsigned_int_5_9_9_9_rev:
        return 4u;
    case gl_float_32_unsigned_int_24_8_rev:
        return 8u;
    default:
        return 0u;
    }
}

size_t PixelStore::value_bytes(uint32_t type) const
{
    AV_SCOPE("PixelStore::value_bytes");
    switch (type) {
    case gl_byte:
    case gl_unsigned_byte:
        return 1u;
    case gl_short:
    case gl_unsigned_short:
    case gl_half_float:
        return 2u;
    case gl_int:
    case gl_unsigned_int:
    case gl_float:
        return 4u;
    default:
        return 0u;
    }
}

bool PixelStore::packed_fits(uint32_t format, uint32_t type) const
{
    AV_SCOPE("PixelStore::packed_fits");
    if (type == gl_unsigned_int_24_8 || type == gl_float_32_unsigned_int_24_8_rev) return format == gl_depth_stencil;
    if (format == gl_depth_stencil) return false;
    const size_t count = components(format);
    switch (type) {
    case gl_unsigned_byte_3_3_2:
    case gl_unsigned_byte_2_3_3_rev:
    case gl_unsigned_short_5_6_5:
    case gl_unsigned_short_5_6_5_rev:
    case gl_unsigned_int_10f_11f_11f_rev:
    case gl_unsigned_int_5_9_9_9_rev:
        return count == 3u;
    default:
        return count == 4u;
    }
}

}
