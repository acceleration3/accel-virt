#include "proto/d3d/dxgi_format.hpp"

#include "core/support/four_cc.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

namespace {

enum class Shape : uint8_t {
    single,
    /* Block-compressed: 4x4 texels to an element. */
    block,
    /* Packed 4:2:2: two texels to an element. */
    pair,
};

enum Traits : uint8_t {
    plain = 0u,
    typeless = 1u << 0u,
    srgb = 1u << 1u,
    depth = 1u << 2u,
    /* A scanout's image is copied out to the host's display in it. */
    scanout = 1u << 3u,
};

constexpr uint32_t block_side = 4u;
constexpr uint32_t pair_width = 2u;

/* D3DDDIFORMAT (d3dukmdt.h); NV12 and YUY2 by their FourCCs, as the miniport
 * reads an overlay plane's video surface (OverlaySupport). */
constexpr uint32_t ddi_a8r8g8b8 = 21u;
constexpr uint32_t ddi_x8r8g8b8 = 22u;
constexpr uint32_t ddi_a2b10g10r10 = 31u;
constexpr uint32_t ddi_a8b8g8r8 = 32u;
constexpr uint32_t ddi_a16b16g16r16f = 113u;
constexpr uint32_t ddi_nv12 = FourCc('N', 'V', '1', '2').value();
constexpr uint32_t ddi_yuy2 = FourCc('Y', 'U', 'Y', '2').value();

/* VkFormat (vulkan_core.h). */
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

/* cl_channel_order and cl_channel_type (CL/cl.h). */
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

/* DRM formats (drm_fourcc.h): the bytes in memory read as one little-endian
 * word, so B8G8R8A8 - bytes B, G, R, A - is ARGB8888. */
constexpr uint32_t drm_argb8888 = FourCc('A', 'R', '2', '4').value();
constexpr uint32_t drm_xrgb8888 = FourCc('X', 'R', '2', '4').value();
constexpr uint32_t drm_abgr8888 = FourCc('A', 'B', '2', '4').value();
constexpr uint32_t drm_abgr2101010 = FourCc('A', 'B', '3', '0').value();
constexpr uint32_t drm_abgr16161616f = FourCc('A', 'B', '4', 'H').value();

}  // namespace

struct DxgiFormatRow {
    uint8_t  bytes;
    Shape    shape;
    uint8_t  family;
    uint8_t  sibling;
    /* A typeless head's resolve format; 0 for every other row. */
    uint8_t  resolved;
    Traits   traits;
    uint32_t ddi;
    uint32_t vk;
    uint32_t cl_order;
    uint32_t cl_type;
    uint32_t drm;
};

namespace {

/* Every DXGI_FORMAT (dxgiformat.h), by its value: element bytes and shape,
 * family head, sibling, resolve format, traits, D3DDDIFORMAT, VkFormat, the
 * OpenCL order and type cl_khr_d3d11_sharing's table gives it (with the four
 * Khronos added later: BGRA, the two sRGB orders and alpha alone), and the DRM
 * format. A value DXGI leaves unnamed is a row of nothing. */
constexpr DxgiFormatRow rows[DxgiFormat::count] = {
    {0u, Shape::single, 0u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                           /*   0 UNKNOWN */
    {16u, Shape::single, 1u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},                                                       /*   1 R32G32B32A32_TYPELESS */
    {16u, Shape::single, 1u, 0u, 0u, plain, 0u, vk_r32g32b32a32_sfloat, cl_rgba, cl_float, 0u},                           /*   2 R32G32B32A32_FLOAT */
    {16u, Shape::single, 1u, 0u, 0u, plain, 0u, 0u, cl_rgba, cl_unsigned_int32, 0u},                                      /*   3 R32G32B32A32_UINT */
    {16u, Shape::single, 1u, 0u, 0u, plain, 0u, 0u, cl_rgba, cl_signed_int32, 0u},                                        /*   4 R32G32B32A32_SINT */
    {12u, Shape::single, 5u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},                                                       /*   5 R32G32B32_TYPELESS */
    {12u, Shape::single, 5u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                          /*   6 R32G32B32_FLOAT */
    {12u, Shape::single, 5u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                          /*   7 R32G32B32_UINT */
    {12u, Shape::single, 5u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                          /*   8 R32G32B32_SINT */
    {8u, Shape::single, 9u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},                                                        /*   9 R16G16B16A16_TYPELESS */
    {8u, Shape::single, 9u, 0u, 0u, plain, ddi_a16b16g16r16f, vk_r16g16b16a16_sfloat, cl_rgba, cl_half_float, drm_abgr16161616f}, /*  10 R16G16B16A16_FLOAT */
    {8u, Shape::single, 9u, 0u, 0u, plain, 0u, vk_r16g16b16a16_unorm, cl_rgba, cl_unorm_int16, 0u},                       /*  11 R16G16B16A16_UNORM */
    {8u, Shape::single, 9u, 0u, 0u, plain, 0u, 0u, cl_rgba, cl_unsigned_int16, 0u},                                       /*  12 R16G16B16A16_UINT */
    {8u, Shape::single, 9u, 0u, 0u, plain, 0u, 0u, cl_rgba, cl_snorm_int16, 0u},                                          /*  13 R16G16B16A16_SNORM */
    {8u, Shape::single, 9u, 0u, 0u, plain, 0u, 0u, cl_rgba, cl_signed_int16, 0u},                                         /*  14 R16G16B16A16_SINT */
    {8u, Shape::single, 15u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},                                                       /*  15 R32G32_TYPELESS */
    {8u, Shape::single, 15u, 0u, 0u, plain, 0u, vk_r32g32_sfloat, cl_rg, cl_float, 0u},                                   /*  16 R32G32_FLOAT */
    {8u, Shape::single, 15u, 0u, 0u, plain, 0u, 0u, cl_rg, cl_unsigned_int32, 0u},                                        /*  17 R32G32_UINT */
    {8u, Shape::single, 15u, 0u, 0u, plain, 0u, 0u, cl_rg, cl_signed_int32, 0u},                                          /*  18 R32G32_SINT */
    {8u, Shape::single, 19u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},                                                       /*  19 R32G8X24_TYPELESS */
    {8u, Shape::single, 19u, 0u, 0u, depth, 0u, 0u, 0u, 0u, 0u},                                                          /*  20 D32_FLOAT_S8X24_UINT */
    {8u, Shape::single, 19u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},                                                       /*  21 R32_FLOAT_X8X24_TYPELESS */
    {8u, Shape::single, 19u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},                                                       /*  22 X32_TYPELESS_G8X24_UINT */
    {4u, Shape::single, 23u, 0u, 24u, typeless, ddi_a2b10g10r10, 0u, 0u, 0u, 0u},                                         /*  23 R10G10B10A2_TYPELESS */
    {4u, Shape::single, 23u, 0u, 0u, plain, ddi_a2b10g10r10, vk_a2b10g10r10_unorm, 0u, 0u, drm_abgr2101010},              /*  24 R10G10B10A2_UNORM */
    {4u, Shape::single, 23u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                          /*  25 R10G10B10A2_UINT */
    {4u, Shape::single, 26u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                          /*  26 R11G11B10_FLOAT */
    {4u, Shape::single, 27u, 0u, 28u, typeless, 0u, 0u, 0u, 0u, 0u},                                                      /*  27 R8G8B8A8_TYPELESS */
    {4u, Shape::single, 27u, 29u, 0u, scanout, ddi_a8b8g8r8, vk_r8g8b8a8_unorm, cl_rgba, cl_unorm_int8, drm_abgr8888},              /*  28 R8G8B8A8_UNORM */
    {4u, Shape::single, 27u, 28u, 0u, srgb, 0u, vk_r8g8b8a8_srgb, cl_srgba, cl_unorm_int8, drm_abgr8888},                 /*  29 R8G8B8A8_UNORM_SRGB */
    {4u, Shape::single, 27u, 0u, 0u, plain, 0u, 0u, cl_rgba, cl_unsigned_int8, 0u},                                       /*  30 R8G8B8A8_UINT */
    {4u, Shape::single, 27u, 0u, 0u, plain, 0u, 0u, cl_rgba, cl_snorm_int8, 0u},                                          /*  31 R8G8B8A8_SNORM */
    {4u, Shape::single, 27u, 0u, 0u, plain, 0u, 0u, cl_rgba, cl_signed_int8, 0u},                                         /*  32 R8G8B8A8_SINT */
    {4u, Shape::single, 33u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},                                                       /*  33 R16G16_TYPELESS */
    {4u, Shape::single, 33u, 0u, 0u, plain, 0u, vk_r16g16_sfloat, cl_rg, cl_half_float, 0u},                              /*  34 R16G16_FLOAT */
    {4u, Shape::single, 33u, 0u, 0u, plain, 0u, 0u, cl_rg, cl_unorm_int16, 0u},                                           /*  35 R16G16_UNORM */
    {4u, Shape::single, 33u, 0u, 0u, plain, 0u, 0u, cl_rg, cl_unsigned_int16, 0u},                                        /*  36 R16G16_UINT */
    {4u, Shape::single, 33u, 0u, 0u, plain, 0u, 0u, cl_rg, cl_snorm_int16, 0u},                                           /*  37 R16G16_SNORM */
    {4u, Shape::single, 33u, 0u, 0u, plain, 0u, 0u, cl_rg, cl_signed_int16, 0u},                                          /*  38 R16G16_SINT */
    {4u, Shape::single, 39u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},                                                       /*  39 R32_TYPELESS */
    {4u, Shape::single, 39u, 0u, 0u, depth, 0u, 0u, 0u, 0u, 0u},                                                          /*  40 D32_FLOAT */
    {4u, Shape::single, 39u, 0u, 0u, plain, 0u, vk_r32_sfloat, cl_r, cl_float, 0u},                                       /*  41 R32_FLOAT */
    {4u, Shape::single, 39u, 0u, 0u, plain, 0u, 0u, cl_r, cl_unsigned_int32, 0u},                                         /*  42 R32_UINT */
    {4u, Shape::single, 39u, 0u, 0u, plain, 0u, 0u, cl_r, cl_signed_int32, 0u},                                           /*  43 R32_SINT */
    {4u, Shape::single, 44u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},                                                       /*  44 R24G8_TYPELESS */
    {4u, Shape::single, 44u, 0u, 0u, depth, 0u, 0u, 0u, 0u, 0u},                                                          /*  45 D24_UNORM_S8_UINT */
    {4u, Shape::single, 44u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},                                                       /*  46 R24_UNORM_X8_TYPELESS */
    {4u, Shape::single, 44u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},                                                       /*  47 X24_TYPELESS_G8_UINT */
    {2u, Shape::single, 48u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},                                                       /*  48 R8G8_TYPELESS */
    {2u, Shape::single, 48u, 0u, 0u, plain, 0u, vk_r8g8_unorm, cl_rg, cl_unorm_int8, 0u},                                 /*  49 R8G8_UNORM */
    {2u, Shape::single, 48u, 0u, 0u, plain, 0u, 0u, cl_rg, cl_unsigned_int8, 0u},                                         /*  50 R8G8_UINT */
    {2u, Shape::single, 48u, 0u, 0u, plain, 0u, 0u, cl_rg, cl_snorm_int8, 0u},                                            /*  51 R8G8_SNORM */
    {2u, Shape::single, 48u, 0u, 0u, plain, 0u, 0u, cl_rg, cl_signed_int8, 0u},                                           /*  52 R8G8_SINT */
    {2u, Shape::single, 53u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},                                                       /*  53 R16_TYPELESS */
    {2u, Shape::single, 53u, 0u, 0u, plain, 0u, vk_r16_sfloat, cl_r, cl_half_float, 0u},                                  /*  54 R16_FLOAT */
    {2u, Shape::single, 53u, 0u, 0u, depth, 0u, 0u, 0u, 0u, 0u},                                                          /*  55 D16_UNORM */
    {2u, Shape::single, 53u, 0u, 0u, plain, 0u, 0u, cl_r, cl_unorm_int16, 0u},                                            /*  56 R16_UNORM */
    {2u, Shape::single, 53u, 0u, 0u, plain, 0u, 0u, cl_r, cl_unsigned_int16, 0u},                                         /*  57 R16_UINT */
    {2u, Shape::single, 53u, 0u, 0u, plain, 0u, 0u, cl_r, cl_snorm_int16, 0u},                                            /*  58 R16_SNORM */
    {2u, Shape::single, 53u, 0u, 0u, plain, 0u, 0u, cl_r, cl_signed_int16, 0u},                                           /*  59 R16_SINT */
    {1u, Shape::single, 60u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},                                                       /*  60 R8_TYPELESS */
    {1u, Shape::single, 60u, 0u, 0u, plain, 0u, vk_r8_unorm, cl_r, cl_unorm_int8, 0u},                                    /*  61 R8_UNORM */
    {1u, Shape::single, 60u, 0u, 0u, plain, 0u, 0u, cl_r, cl_unsigned_int8, 0u},                                          /*  62 R8_UINT */
    {1u, Shape::single, 60u, 0u, 0u, plain, 0u, 0u, cl_r, cl_snorm_int8, 0u},                                             /*  63 R8_SNORM */
    {1u, Shape::single, 60u, 0u, 0u, plain, 0u, 0u, cl_r, cl_signed_int8, 0u},                                            /*  64 R8_SINT */
    {1u, Shape::single, 65u, 0u, 0u, plain, 0u, 0u, cl_a, cl_unorm_int8, 0u},                                             /*  65 A8_UNORM */
    {0u, Shape::single, 66u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                          /*  66 R1_UNORM */
    {4u, Shape::single, 67u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                          /*  67 R9G9B9E5_SHAREDEXP */
    {4u, Shape::pair, 68u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                            /*  68 R8G8_B8G8_UNORM */
    {4u, Shape::pair, 69u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                            /*  69 G8R8_G8B8_UNORM */
    {8u, Shape::block, 70u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},                                                        /*  70 BC1_TYPELESS */
    {8u, Shape::block, 70u, 72u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                          /*  71 BC1_UNORM */
    {8u, Shape::block, 70u, 71u, 0u, srgb, 0u, 0u, 0u, 0u, 0u},                                                           /*  72 BC1_UNORM_SRGB */
    {16u, Shape::block, 73u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},                                                       /*  73 BC2_TYPELESS */
    {16u, Shape::block, 73u, 75u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /*  74 BC2_UNORM */
    {16u, Shape::block, 73u, 74u, 0u, srgb, 0u, 0u, 0u, 0u, 0u},                                                          /*  75 BC2_UNORM_SRGB */
    {16u, Shape::block, 76u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},                                                       /*  76 BC3_TYPELESS */
    {16u, Shape::block, 76u, 78u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /*  77 BC3_UNORM */
    {16u, Shape::block, 76u, 77u, 0u, srgb, 0u, 0u, 0u, 0u, 0u},                                                          /*  78 BC3_UNORM_SRGB */
    {8u, Shape::block, 79u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},                                                        /*  79 BC4_TYPELESS */
    {8u, Shape::block, 79u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                           /*  80 BC4_UNORM */
    {8u, Shape::block, 79u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                           /*  81 BC4_SNORM */
    {16u, Shape::block, 82u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},                                                       /*  82 BC5_TYPELESS */
    {16u, Shape::block, 82u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                          /*  83 BC5_UNORM */
    {16u, Shape::block, 82u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                          /*  84 BC5_SNORM */
    {2u, Shape::single, 85u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                          /*  85 B5G6R5_UNORM */
    {2u, Shape::single, 86u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                          /*  86 B5G5R5A1_UNORM */
    {4u, Shape::single, 90u, 91u, 0u, scanout, ddi_a8r8g8b8, vk_b8g8r8a8_unorm, cl_bgra, cl_unorm_int8, drm_argb8888},    /*  87 B8G8R8A8_UNORM */
    {4u, Shape::single, 92u, 93u, 0u, scanout, ddi_x8r8g8b8, vk_b8g8r8a8_unorm, 0u, 0u, drm_xrgb8888},                    /*  88 B8G8R8X8_UNORM */
    {4u, Shape::single, 89u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                          /*  89 R10G10B10_XR_BIAS_A2_UNORM */
    {4u, Shape::single, 90u, 0u, 87u, typeless, ddi_a8r8g8b8, 0u, 0u, 0u, 0u},                                            /*  90 B8G8R8A8_TYPELESS */
    {4u, Shape::single, 90u, 87u, 0u, srgb, ddi_a8r8g8b8, vk_b8g8r8a8_srgb, cl_sbgra, cl_unorm_int8, drm_argb8888},       /*  91 B8G8R8A8_UNORM_SRGB */
    {4u, Shape::single, 92u, 0u, 88u, typeless, ddi_x8r8g8b8, 0u, 0u, 0u, 0u},                                            /*  92 B8G8R8X8_TYPELESS */
    {4u, Shape::single, 92u, 88u, 0u, srgb, ddi_x8r8g8b8, vk_b8g8r8a8_srgb, 0u, 0u, drm_xrgb8888},                        /*  93 B8G8R8X8_UNORM_SRGB */
    {16u, Shape::block, 94u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},                                                       /*  94 BC6H_TYPELESS */
    {16u, Shape::block, 94u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                          /*  95 BC6H_UF16 */
    {16u, Shape::block, 94u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                          /*  96 BC6H_SF16 */
    {16u, Shape::block, 97u, 0u, 0u, typeless, 0u, 0u, 0u, 0u, 0u},                                                       /*  97 BC7_TYPELESS */
    {16u, Shape::block, 97u, 99u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /*  98 BC7_UNORM */
    {16u, Shape::block, 97u, 98u, 0u, srgb, 0u, 0u, 0u, 0u, 0u},                                                          /*  99 BC7_UNORM_SRGB */
    {4u, Shape::single, 100u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 100 AYUV */
    {4u, Shape::single, 101u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 101 Y410 */
    {8u, Shape::single, 102u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 102 Y416 */
    {0u, Shape::single, 103u, 0u, 0u, plain, ddi_nv12, 0u, 0u, 0u, 0u},                                                   /* 103 NV12 */
    {0u, Shape::single, 104u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 104 P010 */
    {0u, Shape::single, 105u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 105 P016 */
    {0u, Shape::single, 106u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 106 420_OPAQUE */
    {4u, Shape::pair, 107u, 0u, 0u, plain, ddi_yuy2, 0u, 0u, 0u, 0u},                                                     /* 107 YUY2 */
    {8u, Shape::pair, 108u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                           /* 108 Y210 */
    {8u, Shape::pair, 109u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                           /* 109 Y216 */
    {0u, Shape::single, 110u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 110 NV11 */
    {1u, Shape::single, 111u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 111 AI44 */
    {1u, Shape::single, 112u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 112 IA44 */
    {1u, Shape::single, 113u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 113 P8 */
    {2u, Shape::single, 114u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 114 A8P8 */
    {2u, Shape::single, 115u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 115 B4G4R4A4_UNORM */
    {0u, Shape::single, 116u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 116 - */
    {0u, Shape::single, 117u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 117 - */
    {0u, Shape::single, 118u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 118 - */
    {0u, Shape::single, 119u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 119 - */
    {0u, Shape::single, 120u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 120 - */
    {0u, Shape::single, 121u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 121 - */
    {0u, Shape::single, 122u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 122 - */
    {0u, Shape::single, 123u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 123 - */
    {0u, Shape::single, 124u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 124 - */
    {0u, Shape::single, 125u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 125 - */
    {0u, Shape::single, 126u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 126 - */
    {0u, Shape::single, 127u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 127 - */
    {0u, Shape::single, 128u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 128 - */
    {0u, Shape::single, 129u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 129 - */
    {0u, Shape::single, 130u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 130 P208 */
    {0u, Shape::single, 131u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 131 V208 */
    {0u, Shape::single, 132u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 132 V408 */
    {0u, Shape::single, 133u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 133 - */
    {0u, Shape::single, 134u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 134 - */
    {0u, Shape::single, 135u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 135 - */
    {0u, Shape::single, 136u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 136 - */
    {0u, Shape::single, 137u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 137 - */
    {0u, Shape::single, 138u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 138 - */
    {0u, Shape::single, 139u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 139 - */
    {0u, Shape::single, 140u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 140 - */
    {0u, Shape::single, 141u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 141 - */
    {0u, Shape::single, 142u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 142 - */
    {0u, Shape::single, 143u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 143 - */
    {0u, Shape::single, 144u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 144 - */
    {0u, Shape::single, 145u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 145 - */
    {0u, Shape::single, 146u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 146 - */
    {0u, Shape::single, 147u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 147 - */
    {0u, Shape::single, 148u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 148 - */
    {0u, Shape::single, 149u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 149 - */
    {0u, Shape::single, 150u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 150 - */
    {0u, Shape::single, 151u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 151 - */
    {0u, Shape::single, 152u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 152 - */
    {0u, Shape::single, 153u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 153 - */
    {0u, Shape::single, 154u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 154 - */
    {0u, Shape::single, 155u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 155 - */
    {0u, Shape::single, 156u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 156 - */
    {0u, Shape::single, 157u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 157 - */
    {0u, Shape::single, 158u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 158 - */
    {0u, Shape::single, 159u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 159 - */
    {0u, Shape::single, 160u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 160 - */
    {0u, Shape::single, 161u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 161 - */
    {0u, Shape::single, 162u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 162 - */
    {0u, Shape::single, 163u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 163 - */
    {0u, Shape::single, 164u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 164 - */
    {0u, Shape::single, 165u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 165 - */
    {0u, Shape::single, 166u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 166 - */
    {0u, Shape::single, 167u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 167 - */
    {0u, Shape::single, 168u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 168 - */
    {0u, Shape::single, 169u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 169 - */
    {0u, Shape::single, 170u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 170 - */
    {0u, Shape::single, 171u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 171 - */
    {0u, Shape::single, 172u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 172 - */
    {0u, Shape::single, 173u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 173 - */
    {0u, Shape::single, 174u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 174 - */
    {0u, Shape::single, 175u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 175 - */
    {0u, Shape::single, 176u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 176 - */
    {0u, Shape::single, 177u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 177 - */
    {0u, Shape::single, 178u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 178 - */
    {0u, Shape::single, 179u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 179 - */
    {0u, Shape::single, 180u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 180 - */
    {0u, Shape::single, 181u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 181 - */
    {0u, Shape::single, 182u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 182 - */
    {0u, Shape::single, 183u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 183 - */
    {0u, Shape::single, 184u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 184 - */
    {0u, Shape::single, 185u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 185 - */
    {0u, Shape::single, 186u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 186 - */
    {0u, Shape::single, 187u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 187 - */
    {0u, Shape::single, 188u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 188 - */
    {0u, Shape::single, 189u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 189 SAMPLER_FEEDBACK_MIN_MIP_OPAQUE */
    {0u, Shape::single, 190u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 190 SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE */
    {2u, Shape::single, 191u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u},                                                         /* 191 A4B4G4R4_UNORM */
};

constexpr DxgiFormatRow nothing = {0u, Shape::single, 0u, 0u, 0u, plain, 0u, 0u, 0u, 0u, 0u};

}  // namespace

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

}  // namespace av
