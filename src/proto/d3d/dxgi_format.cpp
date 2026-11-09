#include "proto/d3d/dxgi_format.hpp"

#include "core/support/four_cc.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

namespace {

enum class Shape : uint8_t {
    single,
    block,
    pair,
};

enum Traits : uint8_t {
    plain = 0u,
    typeless = 1u << 0u,
    srgb = 1u << 1u,
    depth = 1u << 2u,
    scanout = 1u << 3u,
};

constexpr uint32_t block_side = 4u;
constexpr uint32_t pair_width = 2u;

constexpr uint32_t ddi_a8r8g8b8 = 21u;
constexpr uint32_t ddi_x8r8g8b8 = 22u;
constexpr uint32_t ddi_a2b10g10r10 = 31u;
constexpr uint32_t ddi_a8b8g8r8 = 32u;
constexpr uint32_t ddi_a16b16g16r16f = 113u;
constexpr uint32_t ddi_nv12 = FourCc('N', 'V', '1', '2').value();
constexpr uint32_t nv12 = 103u, p010 = 104u, p016 = 105u, opaque_420 = 106u;
constexpr uint32_t ddi_yuy2 = FourCc('Y', 'U', 'Y', '2').value();

constexpr uint32_t vk_r8_unorm = 9u;
constexpr uint32_t vk_r8g8_unorm = 16u;
constexpr uint32_t vk_r8g8b8a8_unorm = 37u;
constexpr uint32_t vk_r8g8b8a8_srgb = 43u;
constexpr uint32_t vk_b8g8r8a8_unorm = 44u;
constexpr uint32_t vk_b8g8r8a8_srgb = 50u;
constexpr uint32_t vk_a2b10g10r10_unorm = 64u;
constexpr uint32_t vk_r16_sfloat = 76u;
constexpr uint32_t vk_r16g16_sfloat = 83u;
constexpr uint32_t vk_r16g16b16a16_unorm = 91u;
constexpr uint32_t vk_r16g16b16a16_sfloat = 97u;
constexpr uint32_t vk_r32_sfloat = 100u;
constexpr uint32_t vk_r32g32_sfloat = 103u;
constexpr uint32_t vk_r32g32b32a32_sfloat = 109u;

constexpr uint32_t cl_r = 0x10B0u;
constexpr uint32_t cl_a = 0x10B1u;
constexpr uint32_t cl_rg = 0x10B2u;
constexpr uint32_t cl_rgba = 0x10B5u;
constexpr uint32_t cl_bgra = 0x10B6u;
constexpr uint32_t cl_srgba = 0x10C1u;
constexpr uint32_t cl_sbgra = 0x10C2u;
constexpr uint32_t cl_snorm_int8 = 0x10D0u;
constexpr uint32_t cl_snorm_int16 = 0x10D1u;
constexpr uint32_t cl_unorm_int8 = 0x10D2u;
constexpr uint32_t cl_unorm_int16 = 0x10D3u;
constexpr uint32_t cl_signed_int8 = 0x10D7u;
constexpr uint32_t cl_signed_int16 = 0x10D8u;
constexpr uint32_t cl_signed_int32 = 0x10D9u;
constexpr uint32_t cl_unsigned_int8 = 0x10DAu;
constexpr uint32_t cl_unsigned_int16 = 0x10DBu;
constexpr uint32_t cl_unsigned_int32 = 0x10DCu;
constexpr uint32_t cl_half_float = 0x10DDu;
constexpr uint32_t cl_float = 0x10DEu;

constexpr uint32_t drm_argb8888 = FourCc('A', 'R', '2', '4').value();
constexpr uint32_t drm_xrgb8888 = FourCc('X', 'R', '2', '4').value();
constexpr uint32_t drm_abgr8888 = FourCc('A', 'B', '2', '4').value();
constexpr uint32_t drm_abgr2101010 = FourCc('A', 'B', '3', '0').value();
constexpr uint32_t drm_abgr16161616f = FourCc('A', 'B', '4', 'H').value();

}

struct DxgiFormatRow {
    uint8_t  bytes;
    Shape    shape;
    uint8_t  family;
    uint8_t  sibling;
    uint8_t  resolved;
    Traits   traits;
    uint32_t ddi;
    uint32_t vk;
    uint32_t cl_order;
    uint32_t cl_type;
    uint32_t drm;
};

namespace {
constexpr DxgiFormatRow rows[DxgiFormat::count] = {
    {0u, Shape::single, 0u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {16u, Shape::single, 1u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},
    {16u, Shape::single, 1u, 0u, 0u, plain, 0u, vk_r32g32b32a32_sfloat, cl_rgba, cl_float, 0u},
    {16u, Shape::single, 1u, 0u, 0u, plain, 0u, 0u, cl_rgba, cl_unsigned_int32, 0u},
    {16u, Shape::single, 1u, 0u, 0u, plain, 0u, 0u, cl_rgba, cl_signed_int32, 0u},
    {12u, Shape::single, 5u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},
    {12u, Shape::single, 5u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {12u, Shape::single, 5u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {12u, Shape::single, 5u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {8u, Shape::single, 9u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},
    {8u, Shape::single, 9u, 0u, 0u, plain, ddi_a16b16g16r16f, vk_r16g16b16a16_sfloat, cl_rgba, cl_half_float, drm_abgr16161616f},
    {8u, Shape::single, 9u, 0u, 0u, plain, 0u, vk_r16g16b16a16_unorm, cl_rgba, cl_unorm_int16, 0u},
    {8u, Shape::single, 9u, 0u, 0u, plain, 0u, 0u, cl_rgba, cl_unsigned_int16, 0u},
    {8u, Shape::single, 9u, 0u, 0u, plain, 0u, 0u, cl_rgba, cl_snorm_int16, 0u},
    {8u, Shape::single, 9u, 0u, 0u, plain, 0u, 0u, cl_rgba, cl_signed_int16, 0u},
    {8u, Shape::single, 15u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},
    {8u, Shape::single, 15u, 0u, 0u, plain, 0u, vk_r32g32_sfloat, cl_rg, cl_float, 0u},
    {8u, Shape::single, 15u, 0u, 0u, plain, 0u, 0u, cl_rg, cl_unsigned_int32, 0u},
    {8u, Shape::single, 15u, 0u, 0u, plain, 0u, 0u, cl_rg, cl_signed_int32, 0u},
    {8u, Shape::single, 19u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},
    {8u, Shape::single, 19u, 0u, 0u, depth, 0u, 0u, 0u, 0u, 0u},
    {8u, Shape::single, 19u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},
    {8u, Shape::single, 19u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},
    {4u, Shape::single, 23u, 0u, 24u, typeless, ddi_a2b10g10r10, 0u, 0u, 0u, 0u},
    {4u, Shape::single, 23u, 0u, 0u, plain, ddi_a2b10g10r10, vk_a2b10g10r10_unorm, 0u, 0u, drm_abgr2101010},
    {4u, Shape::single, 23u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {4u, Shape::single, 26u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {4u, Shape::single, 27u, 0u, 28u, typeless, 0u, 0u, 0u, 0u, 0u},
    {4u, Shape::single, 27u, 29u, 0u, scanout, ddi_a8b8g8r8, vk_r8g8b8a8_unorm, cl_rgba, cl_unorm_int8, drm_abgr8888},
    {4u, Shape::single, 27u, 28u, 0u, srgb, 0u, vk_r8g8b8a8_srgb, cl_srgba, cl_unorm_int8, drm_abgr8888},
    {4u, Shape::single, 27u, 0u, 0u, plain, 0u, 0u, cl_rgba, cl_unsigned_int8, 0u},
    {4u, Shape::single, 27u, 0u, 0u, plain, 0u, 0u, cl_rgba, cl_snorm_int8, 0u},
    {4u, Shape::single, 27u, 0u, 0u, plain, 0u, 0u, cl_rgba, cl_signed_int8, 0u},
    {4u, Shape::single, 33u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},
    {4u, Shape::single, 33u, 0u, 0u, plain, 0u, vk_r16g16_sfloat, cl_rg, cl_half_float, 0u},
    {4u, Shape::single, 33u, 0u, 0u, plain, 0u, 0u, cl_rg, cl_unorm_int16, 0u},
    {4u, Shape::single, 33u, 0u, 0u, plain, 0u, 0u, cl_rg, cl_unsigned_int16, 0u},
    {4u, Shape::single, 33u, 0u, 0u, plain, 0u, 0u, cl_rg, cl_snorm_int16, 0u},
    {4u, Shape::single, 33u, 0u, 0u, plain, 0u, 0u, cl_rg, cl_signed_int16, 0u},
    {4u, Shape::single, 39u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},
    {4u, Shape::single, 39u, 0u, 0u, depth, 0u, 0u, 0u, 0u, 0u},
    {4u, Shape::single, 39u, 0u, 0u, plain, 0u, vk_r32_sfloat, cl_r, cl_float, 0u},
    {4u, Shape::single, 39u, 0u, 0u, plain, 0u, 0u, cl_r, cl_unsigned_int32, 0u},
    {4u, Shape::single, 39u, 0u, 0u, plain, 0u, 0u, cl_r, cl_signed_int32, 0u},
    {4u, Shape::single, 44u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},
    {4u, Shape::single, 44u, 0u, 0u, depth, 0u, 0u, 0u, 0u, 0u},
    {4u, Shape::single, 44u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},
    {4u, Shape::single, 44u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},
    {2u, Shape::single, 48u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},
    {2u, Shape::single, 48u, 0u, 0u, plain, 0u, vk_r8g8_unorm, cl_rg, cl_unorm_int8, 0u},
    {2u, Shape::single, 48u, 0u, 0u, plain, 0u, 0u, cl_rg, cl_unsigned_int8, 0u},
    {2u, Shape::single, 48u, 0u, 0u, plain, 0u, 0u, cl_rg, cl_snorm_int8, 0u},
    {2u, Shape::single, 48u, 0u, 0u, plain, 0u, 0u, cl_rg, cl_signed_int8, 0u},
    {2u, Shape::single, 53u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},
    {2u, Shape::single, 53u, 0u, 0u, plain, 0u, vk_r16_sfloat, cl_r, cl_half_float, 0u},
    {2u, Shape::single, 53u, 0u, 0u, depth, 0u, 0u, 0u, 0u, 0u},
    {2u, Shape::single, 53u, 0u, 0u, plain, 0u, 0u, cl_r, cl_unorm_int16, 0u},
    {2u, Shape::single, 53u, 0u, 0u, plain, 0u, 0u, cl_r, cl_unsigned_int16, 0u},
    {2u, Shape::single, 53u, 0u, 0u, plain, 0u, 0u, cl_r, cl_snorm_int16, 0u},
    {2u, Shape::single, 53u, 0u, 0u, plain, 0u, 0u, cl_r, cl_signed_int16, 0u},
    {1u, Shape::single, 60u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},
    {1u, Shape::single, 60u, 0u, 0u, plain, 0u, vk_r8_unorm, cl_r, cl_unorm_int8, 0u},
    {1u, Shape::single, 60u, 0u, 0u, plain, 0u, 0u, cl_r, cl_unsigned_int8, 0u},
    {1u, Shape::single, 60u, 0u, 0u, plain, 0u, 0u, cl_r, cl_snorm_int8, 0u},
    {1u, Shape::single, 60u, 0u, 0u, plain, 0u, 0u, cl_r, cl_signed_int8, 0u},
    {1u, Shape::single, 65u, 0u, 0u, plain, 0u, 0u, cl_a, cl_unorm_int8, 0u},
    {0u, Shape::single, 66u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {4u, Shape::single, 67u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {4u, Shape::pair, 68u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {4u, Shape::pair, 69u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {8u, Shape::block, 70u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},
    {8u, Shape::block, 70u, 72u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {8u, Shape::block, 70u, 71u, 0u, srgb, 0u, 0u, 0u, 0u, 0u},
    {16u, Shape::block, 73u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},
    {16u, Shape::block, 73u, 75u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {16u, Shape::block, 73u, 74u, 0u, srgb, 0u, 0u, 0u, 0u, 0u},
    {16u, Shape::block, 76u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},
    {16u, Shape::block, 76u, 78u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {16u, Shape::block, 76u, 77u, 0u, srgb, 0u, 0u, 0u, 0u, 0u},
    {8u, Shape::block, 79u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},
    {8u, Shape::block, 79u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {8u, Shape::block, 79u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {16u, Shape::block, 82u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},
    {16u, Shape::block, 82u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {16u, Shape::block, 82u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {2u, Shape::single, 85u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {2u, Shape::single, 86u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {4u, Shape::single, 90u, 91u, 0u, scanout, ddi_a8r8g8b8, vk_b8g8r8a8_unorm, cl_bgra, cl_unorm_int8, drm_argb8888},
    {4u, Shape::single, 92u, 93u, 0u, scanout, ddi_x8r8g8b8, vk_b8g8r8a8_unorm, 0u, 0u, drm_xrgb8888},
    {4u, Shape::single, 89u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {4u, Shape::single, 90u, 0u, 87u, typeless, ddi_a8r8g8b8, 0u, 0u, 0u, 0u},
    {4u, Shape::single, 90u, 87u, 0u, srgb, ddi_a8r8g8b8, vk_b8g8r8a8_srgb, cl_sbgra, cl_unorm_int8, drm_argb8888},
    {4u, Shape::single, 92u, 0u, 88u, typeless, ddi_x8r8g8b8, 0u, 0u, 0u, 0u},
    {4u, Shape::single, 92u, 88u, 0u, srgb, ddi_x8r8g8b8, vk_b8g8r8a8_srgb, 0u, 0u, drm_xrgb8888},
    {16u, Shape::block, 94u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},
    {16u, Shape::block, 94u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {16u, Shape::block, 94u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {16u, Shape::block, 97u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},
    {16u, Shape::block, 97u, 99u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {16u, Shape::block, 97u, 98u, 0u, srgb, 0u, 0u, 0u, 0u, 0u},
    {4u, Shape::single, 100u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {4u, Shape::single, 101u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {8u, Shape::single, 102u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 103u, 0u, 0u, plain, ddi_nv12, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 104u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 105u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 106u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {4u, Shape::pair, 107u, 0u, 0u, plain, ddi_yuy2, 0u, 0u, 0u, 0u},
    {8u, Shape::pair, 108u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {8u, Shape::pair, 109u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 110u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {1u, Shape::single, 111u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {1u, Shape::single, 112u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {1u, Shape::single, 113u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {2u, Shape::single, 114u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {2u, Shape::single, 115u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 116u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 117u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 118u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 119u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 120u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 121u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 122u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 123u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 124u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 125u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 126u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 127u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 128u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 129u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 130u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 131u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 132u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 133u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 134u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 135u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 136u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 137u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 138u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 139u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 140u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 141u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 142u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 143u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 144u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 145u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 146u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 147u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 148u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 149u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 150u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 151u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 152u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 153u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 154u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 155u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 156u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 157u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 158u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 159u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 160u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 161u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 162u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 163u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 164u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 165u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 166u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 167u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 168u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 169u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 170u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 171u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 172u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 173u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 174u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 175u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 176u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 177u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 178u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 179u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 180u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 181u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 182u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 183u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 184u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 185u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 186u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 187u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 188u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 189u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {0u, Shape::single, 190u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
    {2u, Shape::single, 191u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},
};

constexpr DxgiFormatRow nothing = {0u, Shape::single, 0u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u};

}

DxgiFormat::DxgiFormat(uint32_t format) : format_(format), row_(format < count ? rows[format] : nothing)
{
    AV_SCOPE("DxgiFormat::DxgiFormat");
}

bool DxgiFormat::known() const
{
    AV_SCOPE("DxgiFormat::known");
    return format_ < count;
}

uint32_t DxgiFormat::value() const
{
    AV_SCOPE("DxgiFormat::value");
    return format_;
}

uint32_t DxgiFormat::element_bytes() const
{
    AV_SCOPE("DxgiFormat::element_bytes");
    return row_.bytes;
}

uint32_t DxgiFormat::element_width() const
{
    AV_SCOPE("DxgiFormat::element_width");
    if (row_.shape == Shape::block) return block_side;
    return row_.shape == Shape::pair ? pair_width : 1u;
}

uint32_t DxgiFormat::element_height() const
{
    AV_SCOPE("DxgiFormat::element_height");
    return row_.shape == Shape::block ? block_side : 1u;
}

uint32_t DxgiFormat::planar_sample_bytes() const
{
    AV_SCOPE("DxgiFormat::planar_sample_bytes");
    if (format_ == nv12 || format_ == opaque_420) return 1u;
    return format_ == p010 || format_ == p016 ? 2u : 0u;
}

uint32_t DxgiFormat::family() const
{
    AV_SCOPE("DxgiFormat::family");
    return known() ? row_.family : 0u;
}

bool DxgiFormat::typeless() const
{
    AV_SCOPE("DxgiFormat::typeless");
    return (row_.traits & Traits::typeless) != 0u;
}

bool DxgiFormat::family_head() const
{
    AV_SCOPE("DxgiFormat::family_head");
    return typeless() && row_.family == format_;
}

bool DxgiFormat::depth() const
{
    AV_SCOPE("DxgiFormat::depth");
    return (row_.traits & Traits::depth) != 0u;
}

bool DxgiFormat::srgb() const
{
    AV_SCOPE("DxgiFormat::srgb");
    return (row_.traits & Traits::srgb) != 0u;
}

uint32_t DxgiFormat::sibling() const
{
    AV_SCOPE("DxgiFormat::sibling");
    return row_.sibling;
}

uint32_t DxgiFormat::resolved() const
{
    AV_SCOPE("DxgiFormat::resolved");
    if (!known()) return 0u;
    return typeless() ? row_.resolved : format_;
}

uint32_t DxgiFormat::ddi_format() const
{
    AV_SCOPE("DxgiFormat::ddi_format");
    return row_.ddi;
}

uint32_t DxgiFormat::vk_format() const
{
    AV_SCOPE("DxgiFormat::vk_format");
    return row_.vk;
}

uint32_t DxgiFormat::vk_sibling() const
{
    AV_SCOPE("DxgiFormat::vk_sibling");
    return row_.sibling != 0u ? rows[row_.sibling].vk : 0u;
}

uint32_t DxgiFormat::cl_order() const
{
    AV_SCOPE("DxgiFormat::cl_order");
    return row_.cl_order;
}

uint32_t DxgiFormat::cl_type() const
{
    AV_SCOPE("DxgiFormat::cl_type");
    return row_.cl_type;
}

bool DxgiFormat::cl_image_on_host() const
{
    AV_SCOPE("DxgiFormat::cl_image_on_host");
    return row_.cl_order != 0u && row_.vk != 0u && !srgb();
}

uint32_t DxgiFormat::drm_fourcc() const
{
    AV_SCOPE("DxgiFormat::drm_fourcc");
    return row_.drm;
}

bool DxgiFormat::scanned_out() const
{
    AV_SCOPE("DxgiFormat::scanned_out");
    return (row_.traits & Traits::scanout) != 0u;
}

}
