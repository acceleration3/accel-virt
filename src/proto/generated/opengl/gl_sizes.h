// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m rvk_codegen.opengl --xml gl.xml --wgl wgl.xml
//                  --profile tools/codegen/profiles/opengl.toml --out ...

#pragma once

#include <cstddef>
#include <cstdint>

namespace av::glproto {

// How many values a query with this enum answers with.
//
// The registry says COMPSIZE(pname) and stops. Almost every
// enum answers with one value; the ones that do not are
// listed in the profile, and both sides read this table, so
// they agree about how much came back.
[[nodiscard]] inline std::size_t pname_count(std::uint32_t pname) noexcept {
    switch (pname) {
        case 0x0B80: return 4;  // GL_ACCUM_CLEAR_VALUE
        case 0x846E: return 2;  // GL_ALIASED_LINE_WIDTH_RANGE
        case 0x8005: return 4;  // GL_BLEND_COLOR
        case 0x0C22: return 4;  // GL_COLOR_CLEAR_VALUE
        case 0x80B1: return 16;  // GL_COLOR_MATRIX
        case 0x0C23: return 4;  // GL_COLOR_WRITEMASK
        case 0x86A3: return 128;  // GL_COMPRESSED_TEXTURE_FORMATS
        case 0x8267: return 3;  // GL_COMPUTE_WORK_GROUP_SIZE
        case 0x0B00: return 4;  // GL_CURRENT_COLOR
        case 0x0B02: return 3;  // GL_CURRENT_NORMAL
        case 0x0B04: return 4;  // GL_CURRENT_RASTER_COLOR
        case 0x0B07: return 4;  // GL_CURRENT_RASTER_POSITION
        case 0x845F: return 4;  // GL_CURRENT_RASTER_SECONDARY_COLOR
        case 0x0B06: return 4;  // GL_CURRENT_RASTER_TEXTURE_COORDS
        case 0x8459: return 4;  // GL_CURRENT_SECONDARY_COLOR
        case 0x0B03: return 4;  // GL_CURRENT_TEXTURE_COORDS
        case 0x0B70: return 2;  // GL_DEPTH_RANGE
        case 0x0B66: return 4;  // GL_FOG_COLOR
        case 0x0B53: return 4;  // GL_LIGHT_MODEL_AMBIENT
        case 0x0B22: return 2;  // GL_LINE_WIDTH_RANGE
        case 0x0DD0: return 2;  // GL_MAP1_GRID_DOMAIN
        case 0x0DD2: return 4;  // GL_MAP2_GRID_DOMAIN
        case 0x0DD3: return 2;  // GL_MAP2_GRID_SEGMENTS
        case 0x0D3A: return 2;  // GL_MAX_VIEWPORT_DIMS
        case 0x0BA6: return 16;  // GL_MODELVIEW_MATRIX
        case 0x8E73: return 2;  // GL_PATCH_DEFAULT_INNER_LEVEL
        case 0x8E74: return 4;  // GL_PATCH_DEFAULT_OUTER_LEVEL
        case 0x8129: return 3;  // GL_POINT_DISTANCE_ATTENUATION
        case 0x0000: return 2;  // GL_POINT_SIZE_IMPLEMENTATION_DEPENDENT
        case 0x0B12: return 2;  // GL_POINT_SIZE_RANGE
        case 0x87FF: return 16;  // GL_PROGRAM_BINARY_FORMATS
        case 0x0BA7: return 16;  // GL_PROJECTION_MATRIX
        case 0x0C10: return 4;  // GL_SCISSOR_BOX
        case 0x8DF8: return 16;  // GL_SHADER_BINARY_FORMATS, GL_SMOOTH_LINE_WIDTH_RANGE, GL_SMOOTH_POINT_SIZE_RANGE
        case 0x1004: return 4;  // GL_TEXTURE_BORDER_COLOR
        case 0x2201: return 4;  // GL_TEXTURE_ENV_COLOR
        case 0x0BA8: return 16;  // GL_TEXTURE_MATRIX
        case 0x8E46: return 4;  // GL_TEXTURE_SWIZZLE_RGBA
        case 0x84E6: return 16;  // GL_TRANSPOSE_COLOR_MATRIX
        case 0x84E3: return 16;  // GL_TRANSPOSE_MODELVIEW_MATRIX
        case 0x84E4: return 16;  // GL_TRANSPOSE_PROJECTION_MATRIX
        case 0x84E5: return 16;  // GL_TRANSPOSE_TEXTURE_MATRIX
        case 0x0BA2: return 4;  // GL_VIEWPORT
        default: return 1;
    }
}

// How wide one element of an index or vertex array is.
[[nodiscard]] inline std::size_t type_size(std::uint32_t type) noexcept {
    switch (type) {
        case 0x1400: return 1;  // GL_BYTE
        case 0x1401: return 1;  // GL_UNSIGNED_BYTE
        case 0x1402: return 2;  // GL_SHORT
        case 0x1403: return 2;  // GL_UNSIGNED_SHORT
        case 0x1404: return 4;  // GL_INT
        case 0x1405: return 4;  // GL_UNSIGNED_INT
        case 0x1406: return 4;  // GL_FLOAT
        case 0x140A: return 8;  // GL_DOUBLE
        case 0x140B: return 2;  // GL_HALF_FLOAT
        default: return 4;
    }
}

}  // namespace av::glproto
