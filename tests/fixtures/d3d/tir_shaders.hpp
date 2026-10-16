/* The shaders the target-independent rasterization tests draw with
 * (test_d3d_tir_device), as the DDI hands them: token streams and register
 * signatures. No shader compiler is pinned here, so these are assembled by
 * hand, each token beside the instruction it encodes; dxbc-spirv's
 * disassembler reads them back as the listings say.
 *
 * Direct2D's shape of the feature: a vertex shader passing a position and
 * clip distances through, and a pixel shader writing SV_Coverage as an
 * integer, OR'd into the target by the output merger. The UAV pixel shader
 * ORs the same coverage into a UAV, for drawing with no render target at all.
 * The attribute shaders interpolate a window coordinate at one sample of the
 * forced pattern by its index (pull-model interpolation), and the clip pixel
 * shader reads a clip distance. */
#pragma once

#include "proto/generated/d3d/d3d_wire_types.h"

#include <cstdint>

/* vs_4_0
 *   dcl_input v0.xyzw                    (POSITION: clip space)
 *   dcl_input v1.xy                      (CLIP: two clip distances)
 *   dcl_output_siv o0.xyzw, position
 *   dcl_output_siv o1.xy, clip_distance
 *   mov o0.xyzw, v0.xyzw
 *   mov o1.xy, v1.xyxx
 *   ret */
namespace tir_vs {

inline constexpr std::uint32_t tokens[] = {
    0x00010040u, 0x0000001bu,                                     // vs_4_0, 27 tokens
    0x0300005fu, 0x001010f2u, 0x00000000u,                        // dcl_input v0.xyzw
    0x0300005fu, 0x00101032u, 0x00000001u,                        // dcl_input v1.xy
    0x04000067u, 0x001020f2u, 0x00000000u, 0x00000001u,           // dcl_output_siv o0.xyzw, position
    0x04000067u, 0x00102032u, 0x00000001u, 0x00000002u,           // dcl_output_siv o1.xy, clip_distance
    0x05000036u, 0x001020f2u, 0x00000000u, 0x00101e46u, 0x00000000u,  // mov o0.xyzw, v0.xyzw
    0x05000036u, 0x00102032u, 0x00000001u, 0x00101046u, 0x00000001u,  // mov o1.xy, v1.xyxx
    0x0100003eu,                                                  // ret
};

inline constexpr av::d3dproto::SignatureEntry inputs[] = {
    {0u, 0u, 15u, 3u, 0u, 0u},
    {0u, 1u, 3u, 3u, 0u, 0u},
};
inline constexpr std::uint32_t inputs_count = 2u;

inline constexpr av::d3dproto::SignatureEntry outputs[] = {
    {1u, 0u, 15u, 3u, 0u, 0u},
    {2u, 1u, 3u, 3u, 0u, 0u},
};
inline constexpr std::uint32_t outputs_count = 2u;

}  // namespace tir_vs

/* ps_5_0
 *   dcl_input vCoverage
 *   dcl_output o0.x                      (SV_Target: uint)
 *   mov o0.x, vCoverage
 *   ret */
namespace tir_coverage_ps {

inline constexpr std::uint32_t tokens[] = {
    0x00000050u, 0x0000000cu,                                     // ps_5_0, 12 tokens
    0x0200005fu, 0x00023001u,                                     // dcl_input vCoverage
    0x03000065u, 0x00102012u, 0x00000000u,                        // dcl_output o0.x
    0x04000036u, 0x00102012u, 0x00000000u, 0x00023001u,           // mov o0.x, vCoverage
    0x0100003eu,                                                  // ret
};

inline constexpr av::d3dproto::SignatureEntry inputs[] = {
    {0u, 0u, 0u, 0u, 0u, 0u},
};
inline constexpr std::uint32_t inputs_count = 0u;

inline constexpr av::d3dproto::SignatureEntry outputs[] = {
    {0u, 0u, 1u, 1u, 0u, 0u},
};
inline constexpr std::uint32_t outputs_count = 1u;

}  // namespace tir_coverage_ps

/* ps_5_0
 *   dcl_uav_typed_texture2d (uint, uint, uint, uint) u0
 *   dcl_input_ps_siv linear noperspective v0.xy, position
 *   dcl_input vCoverage
 *   dcl_temps 1
 *   ftou r0.xy, v0.xyxx
 *   atomic_or u0, r0.xyxx, vCoverage
 *   ret */
namespace tir_uav_ps {

inline constexpr std::uint32_t tokens[] = {
    0x00000050u, 0x0000001au,                                     // ps_5_0, 26 tokens
    0x0400189cu, 0x0011e000u, 0x00000000u, 0x00004444u,           // dcl_uav_typed_texture2d (uint) u0
    0x04002064u, 0x00101032u, 0x00000000u, 0x00000001u,           // dcl_input_ps_siv v0.xy, position
    0x0200005fu, 0x00023001u,                                     // dcl_input vCoverage
    0x02000068u, 0x00000001u,                                     // dcl_temps 1
    0x0500001cu, 0x00100032u, 0x00000000u, 0x00101046u, 0x00000000u,  // ftou r0.xy, v0.xyxx
    0x060000aau, 0x0011e000u, 0x00000000u, 0x00100046u, 0x00000000u,  // atomic_or u0, r0.xyxx, vCoverage
    0x00023001u,
    0x0100003eu,                                                  // ret
};

inline constexpr av::d3dproto::SignatureEntry inputs[] = {
    {1u, 0u, 3u, 3u, 0u, 0u},
};
inline constexpr std::uint32_t inputs_count = 1u;

inline constexpr av::d3dproto::SignatureEntry outputs[] = {
    {0u, 0u, 0u, 0u, 0u, 0u},
};
inline constexpr std::uint32_t outputs_count = 0u;

}  // namespace tir_uav_ps

/* vs_4_0
 *   dcl_input v0.xy                      (POSITION: clip-space x, y)
 *   dcl_output_siv o0.xyzw, position
 *   dcl_output o1.x                      (ATTRIBUTE: window x for a 64-wide viewport)
 *   mov o0.xy, v0.xyxx
 *   mov o0.zw, l(0, 0, 0, 1.0)
 *   mad o1.x, v0.xxxx, l(32.0), l(32.0)
 *   ret */
namespace tir_attribute_vs {

inline constexpr std::uint32_t tokens[] = {
    0x00010040u, 0x00000023u,                                     // vs_4_0, 35 tokens
    0x0300005fu, 0x00101032u, 0x00000000u,                        // dcl_input v0.xy
    0x04000067u, 0x001020f2u, 0x00000000u, 0x00000001u,           // dcl_output_siv o0.xyzw, position
    0x03000065u, 0x00102012u, 0x00000001u,                        // dcl_output o1.x
    0x05000036u, 0x00102032u, 0x00000000u, 0x00101046u, 0x00000000u,  // mov o0.xy, v0.xyxx
    0x08000036u, 0x001020c2u, 0x00000000u, 0x00004002u,           // mov o0.zw, l(0, 0, 0, 1.0)
    0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u,
    0x09000032u, 0x00102012u, 0x00000001u, 0x00101006u, 0x00000000u,  // mad o1.x, v0.xxxx, l(32.0), l(32.0)
    0x00004001u, 0x42000000u, 0x00004001u, 0x42000000u,
    0x0100003eu,                                                  // ret
};

inline constexpr av::d3dproto::SignatureEntry inputs[] = {
    {0u, 0u, 3u, 3u, 0u, 0u},
};
inline constexpr std::uint32_t inputs_count = 1u;

inline constexpr av::d3dproto::SignatureEntry outputs[] = {
    {1u, 0u, 15u, 3u, 0u, 0u},
    {0u, 1u, 1u, 3u, 0u, 0u},
};
inline constexpr std::uint32_t outputs_count = 2u;

}  // namespace tir_attribute_vs

/* ps_5_0
 *   dcl_input_ps linear v1.x             (ATTRIBUTE)
 *   dcl_output o0.x                      (SV_Target: uint)
 *   dcl_temps 1
 *   eval_sample_index r0.x, v1.xxxx, l(12)
 *   mad r0.x, r0.xxxx, l(16.0), l(0.5)
 *   ftou o0.x, r0.xxxx
 *   ret
 * Sample 12 of the 16-sample pattern sits half a pixel left of the center,
 * so the target gets the pixel's left edge in sixteenths. */
namespace tir_eval_ps {

inline constexpr std::uint32_t tokens[] = {
    0x00000050u, 0x00000020u,                                     // ps_5_0, 32 tokens
    0x03001062u, 0x00101012u, 0x00000001u,                        // dcl_input_ps linear v1.x
    0x03000065u, 0x00102012u, 0x00000000u,                        // dcl_output o0.x
    0x02000068u, 0x00000001u,                                     // dcl_temps 1
    0x070000ccu, 0x00100012u, 0x00000000u, 0x00101006u, 0x00000001u,  // eval_sample_index r0.x, v1.xxxx, l(12)
    0x00004001u, 0x0000000cu,
    0x09000032u, 0x00100012u, 0x00000000u, 0x00100006u, 0x00000000u,  // mad r0.x, r0.xxxx, l(16.0), l(0.5)
    0x00004001u, 0x41800000u, 0x00004001u, 0x3f000000u,
    0x0500001cu, 0x00102012u, 0x00000000u, 0x00100006u, 0x00000000u,  // ftou o0.x, r0.xxxx
    0x0100003eu,                                                  // ret
};

inline constexpr av::d3dproto::SignatureEntry inputs[] = {
    {0u, 1u, 1u, 3u, 0u, 0u},
};
inline constexpr std::uint32_t inputs_count = 1u;

inline constexpr av::d3dproto::SignatureEntry outputs[] = {
    {0u, 0u, 1u, 1u, 0u, 0u},
};
inline constexpr std::uint32_t outputs_count = 1u;

}  // namespace tir_eval_ps

/* ps_5_0
 *   dcl_input_ps_siv linear v1.x, clip_distance
 *   dcl_input vCoverage
 *   dcl_output o0.x                      (SV_Target: uint)
 *   dcl_temps 1
 *   ge r0.x, v1.xxxx, l(-1e30)
 *   and o0.x, r0.xxxx, vCoverage
 *   ret
 * The coverage again, from a shader that reads a clip distance, which the
 * stage before it must then hand on. */
namespace tir_clip_ps {

inline constexpr std::uint32_t tokens[] = {
    0x00000050u, 0x0000001bu,                                     // ps_5_0, 27 tokens
    0x04001064u, 0x00101012u, 0x00000001u, 0x00000002u,           // dcl_input_ps_siv linear v1.x, clip_distance
    0x0200005fu, 0x00023001u,                                     // dcl_input vCoverage
    0x03000065u, 0x00102012u, 0x00000000u,                        // dcl_output o0.x
    0x02000068u, 0x00000001u,                                     // dcl_temps 1
    0x0700001du, 0x00100012u, 0x00000000u, 0x00101006u, 0x00000001u,  // ge r0.x, v1.xxxx, l(-1e30)
    0x00004001u, 0xf149f2cau,
    0x06000001u, 0x00102012u, 0x00000000u, 0x00100006u, 0x00000000u,  // and o0.x, r0.xxxx, vCoverage
    0x00023001u,
    0x0100003eu,                                                  // ret
};

inline constexpr av::d3dproto::SignatureEntry inputs[] = {
    {2u, 1u, 1u, 3u, 0u, 0u},
};
inline constexpr std::uint32_t inputs_count = 1u;

inline constexpr av::d3dproto::SignatureEntry outputs[] = {
    {0u, 0u, 1u, 1u, 0u, 0u},
};
inline constexpr std::uint32_t outputs_count = 1u;

}  // namespace tir_clip_ps
