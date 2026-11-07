#include "executor/opengl/gl_texture_shape.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint32_t gl_texture_1d = 0x0DE0u, gl_texture_2d = 0x0DE1u, gl_texture_3d = 0x806Fu;
constexpr uint32_t gl_texture_1d_array = 0x8C18u, gl_texture_2d_array = 0x8C1Au;
constexpr uint32_t gl_texture_rectangle = 0x84F5u, gl_texture_cube_map = 0x8513u;
constexpr uint32_t gl_texture_cube_map_array = 0x9009u;
constexpr uint32_t gl_texture_2d_multisample = 0x9100u, gl_texture_2d_multisample_array = 0x9102u;
constexpr uint32_t cube_faces = 6u;
constexpr uint8_t  widest_texel = 16u;

struct TargetRow {
    uint32_t target_;
    uint32_t binding_;
};

constexpr TargetRow targets[] = {
    {gl_texture_1d, 0x8068u},
    {gl_texture_2d, 0x8069u},
    {gl_texture_3d, 0x806Au},
    {gl_texture_1d_array, 0x8C1Cu},
    {gl_texture_2d_array, 0x8C1Du},
    {gl_texture_rectangle, 0x84F6u},
    {gl_texture_cube_map, 0x8514u},
    {gl_texture_cube_map_array, 0x900Au},
    {gl_texture_2d_multisample, 0x9104u},
    {gl_texture_2d_multisample_array, 0x9105u},
};

struct TexelRow {
    uint32_t format_;
    uint8_t  bytes_;
};

constexpr TexelRow texels[] = {
    {0x8229u, 1u},  {0x8F94u, 1u},  {0x8232u, 1u},  {0x8231u, 1u},  {0x8D48u, 1u},  {0x803Cu, 1u},  {0x8040u, 1u},
    {0x804Bu, 1u},  {0x822Au, 2u},  {0x8F98u, 2u},  {0x822Du, 2u},  {0x8234u, 2u},  {0x8233u, 2u},  {0x822Bu, 2u},
    {0x8F95u, 2u},  {0x8238u, 2u},  {0x8237u, 2u},  {0x8D62u, 2u},  {0x8057u, 2u},  {0x8056u, 2u},  {0x81A5u, 2u},
    {0x803Eu, 2u},  {0x8042u, 2u},  {0x8045u, 2u},  {0x8050u, 2u},  {0x8058u, 4u},  {0x8F97u, 4u},  {0x8D7Cu, 4u},
    {0x8D8Eu, 4u},  {0x8C43u, 4u},  {0x8051u, 4u},  {0x8C41u, 4u},  {0x8D7Du, 4u},  {0x8D8Fu, 4u},  {0x8F96u, 4u},
    {0x8059u, 4u},  {0x906Fu, 4u},  {0x8C3Au, 4u},  {0x8C3Du, 4u},  {0x822Cu, 4u},  {0x8F99u, 4u},  {0x822Fu, 4u},
    {0x823Au, 4u},  {0x8239u, 4u},  {0x822Eu, 4u},  {0x8236u, 4u},  {0x8235u, 4u},  {0x81A6u, 4u},  {0x81A7u, 4u},
    {0x8CACu, 4u},  {0x88F0u, 4u},  {0x8052u, 4u},  {0x8048u, 4u},  {0x805Bu, 8u},  {0x8F9Bu, 8u},  {0x881Au, 8u},
    {0x8D76u, 8u},  {0x8D88u, 8u},  {0x8054u, 8u},  {0x881Bu, 8u},  {0x8D77u, 8u},  {0x8D89u, 8u},  {0x8F9Au, 8u},
    {0x8230u, 8u},  {0x823Cu, 8u},  {0x823Bu, 8u},  {0x8CADu, 8u},  {0x8815u, 12u}, {0x8D71u, 12u}, {0x8D83u, 12u},
    {0x8814u, 16u}, {0x8D70u, 16u}, {0x8D82u, 16u},
};

constexpr uint32_t half_block_formats[] = {
    0x83F0u, 0x83F1u, 0x8C4Cu, 0x8C4Du, 0x8DBBu, 0x8DBCu, 0x9274u, 0x9275u, 0x9276u, 0x9277u, 0x9270u, 0x9271u,
};

constexpr uint32_t full_block_formats[] = {
    0x83F2u, 0x83F3u, 0x8C4Eu, 0x8C4Fu, 0x8DBDu, 0x8DBEu, 0x8E8Cu, 0x8E8Du, 0x8E8Eu, 0x8E8Fu, 0x9278u, 0x9279u,
    0x9272u, 0x9273u,
};

struct AstcRow {
    uint8_t width_;
    uint8_t height_;
};

constexpr uint32_t first_astc = 0x93B0u, first_astc_srgb = 0x93D0u;
constexpr AstcRow  astc[] = {{4, 4}, {5, 4}, {5, 5}, {6, 5}, {6, 6}, {8, 5}, {8, 6},
                             {8, 8}, {10, 5}, {10, 6}, {10, 8}, {10, 10}, {12, 10}, {12, 12}};
constexpr uint32_t astc_formats = sizeof astc / sizeof astc[0];
constexpr uint8_t  astc_block_bytes = 16u;
constexpr uint8_t  bc_block_edge = 4u;
constexpr uint8_t  half_block_bytes = 8u;
constexpr uint8_t  full_block_bytes = 16u;

bool lists(const uint32_t *rows, uint32_t count, uint32_t format)
{
    AV_SCOPE("lists");
    for (uint32_t at = 0u; at < count; at++)
        if (rows[at] == format) return true;
    return false;
}

uint64_t clamp_edge(int64_t edge, uint32_t level)
{
    AV_SCOPE("clamp_edge");
    const int64_t shifted = edge >> level;
    return shifted > 0 ? static_cast<uint64_t>(shifted) : 1u;
}

}

GlTextureShape::GlTextureShape(uint32_t target, uint32_t format, int64_t levels, int64_t samples, int64_t width,
                               int64_t height, int64_t depth)
    : target_(target), format_(format), levels_(levels), samples_(samples), width_(width), height_(height),
      depth_(depth)
{
    AV_SCOPE("GlTextureShape::GlTextureShape");
}

bool GlTextureShape::tracked() const
{
    AV_SCOPE("GlTextureShape::tracked");
    return binding() != 0u;
}

uint32_t GlTextureShape::binding() const
{
    AV_SCOPE("GlTextureShape::binding");
    for (const TargetRow &row : targets)
        if (row.target_ == target_) return row.binding_;
    return 0u;
}

bool GlTextureShape::valid() const
{
    AV_SCOPE("GlTextureShape::valid");
    return tracked() && levels_ >= 1 && levels_ <= static_cast<int64_t>(most_levels) && samples_ >= 1 && width_ >= 1 &&
           height_ >= 1 && depth_ >= 1;
}

uint32_t GlTextureShape::layers() const
{
    AV_SCOPE("GlTextureShape::layers");
    return target_ == gl_texture_cube_map ? cube_faces : 1u;
}

GlTextureShape::Format GlTextureShape::describe_format() const
{
    AV_SCOPE("GlTextureShape::describe_format");
    for (const TexelRow &row : texels)
        if (row.format_ == format_) return Format{format_, 1u, 1u, row.bytes_};
    if (lists(half_block_formats, sizeof half_block_formats / sizeof half_block_formats[0], format_))
        return Format{format_, bc_block_edge, bc_block_edge, half_block_bytes};
    if (lists(full_block_formats, sizeof full_block_formats / sizeof full_block_formats[0], format_))
        return Format{format_, bc_block_edge, bc_block_edge, full_block_bytes};
    for (uint32_t at = 0u; at < astc_formats; at++)
        if (format_ == first_astc + at || format_ == first_astc_srgb + at)
            return Format{format_, astc[at].width_, astc[at].height_, astc_block_bytes};
    return Format{format_, 1u, 1u, widest_texel};
}

uint64_t GlTextureShape::level_bytes(uint32_t level, const Format &format) const
{
    AV_SCOPE("GlTextureShape::level_bytes");
    const bool     array_rows = target_ == gl_texture_1d_array;
    const bool     array_slices = target_ == gl_texture_2d_array || target_ == gl_texture_cube_map_array ||
                              target_ == gl_texture_2d_multisample_array;
    const bool     volume = target_ == gl_texture_3d;
    const uint64_t w = clamp_edge(width_, level);
    const uint64_t h = array_rows ? static_cast<uint64_t>(height_) : clamp_edge(height_, level);
    const uint64_t d = volume ? clamp_edge(depth_, level) : array_slices ? static_cast<uint64_t>(depth_) : 1u;
    const uint64_t columns = (w + format.block_width_ - 1u) / format.block_width_;
    const uint64_t rows = (h + format.block_height_ - 1u) / format.block_height_;
    uint64_t       bytes = columns;
    if (__builtin_mul_overflow(bytes, rows, &bytes) || __builtin_mul_overflow(bytes, d, &bytes) ||
        __builtin_mul_overflow(bytes, uint64_t{format.block_bytes_}, &bytes) ||
        __builtin_mul_overflow(bytes, static_cast<uint64_t>(layers()), &bytes))
        return UINT64_MAX;
    if (samples_ > 1 && __builtin_mul_overflow(bytes, static_cast<uint64_t>(samples_), &bytes)) return UINT64_MAX;
    return bytes;
}

uint64_t GlTextureShape::least_bytes() const
{
    AV_SCOPE("GlTextureShape::least_bytes");
    if (!valid()) return UINT64_MAX;
    const Format format = describe_format();
    uint64_t     total = 0u;
    for (uint32_t level = 0u; level < static_cast<uint32_t>(levels_); level++) {
        const uint64_t bytes = level_bytes(level, format);
        if (bytes == UINT64_MAX || __builtin_add_overflow(total, bytes, &total)) return UINT64_MAX;
    }
    return total;
}

}
