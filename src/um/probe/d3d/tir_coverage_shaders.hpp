/* The target-independent rasterization probe's shaders (TirCoverageWork),
 * as DXBC containers the runtime takes: no shader compiler is pinned here,
 * so the token streams are assembled by hand and the containers built
 * around them - ISGN, OSGN and the code chunk, as Direct2D's own shaders
 * carry, and the checksum D3D computes over everything past it (DXBC's MD5
 * variant; DxbcDigest computes the same on the host). dxbc-spirv's
 * disassembler reads them back as the listings say.
 *
 * vs_4_0                                  ps_5_0
 *   dcl_input v0.xy        (POSITION)       dcl_input vCoverage
 *   dcl_output_siv o0.xyzw, position        dcl_output o0.x   (SV_TARGET: uint)
 *   mov o0.xy, v0.xyxx                      mov o0.x, vCoverage
 *   mov o0.zw, l(0, 0, 0, 1.0)              ret
 *   ret */
#ifndef AV_TIR_COVERAGE_SHADERS_HPP
#define AV_TIR_COVERAGE_SHADERS_HPP

#include <stdint.h>

namespace av {

inline constexpr uint32_t tir_coverage_vs[] = {
    0x43425844u, 0xa3b967a7u, 0x36bd4ec0u, 0xb8982984u, 0xd559d32bu, 0x00000001u, 0x000000f8u, 0x00000003u,
    0x0000002cu, 0x00000060u, 0x00000094u, 0x4e475349u, 0x0000002cu, 0x00000001u, 0x00000008u, 0x00000020u,
    0x00000000u, 0x00000000u, 0x00000003u, 0x00000000u, 0x00000303u, 0x49534f50u, 0x4e4f4954u, 0xababab00u,
    0x4e47534fu, 0x0000002cu, 0x00000001u, 0x00000008u, 0x00000020u, 0x00000000u, 0x00000001u, 0x00000003u,
    0x00000000u, 0x0000000fu, 0x505f5653u, 0x5449534fu, 0x004e4f49u, 0x52444853u, 0x0000005cu, 0x00010040u,
    0x00000017u, 0x0300005fu, 0x00101032u, 0x00000000u, 0x04000067u, 0x001020f2u, 0x00000000u, 0x00000001u,
    0x05000036u, 0x00102032u, 0x00000000u, 0x00101046u, 0x00000000u, 0x08000036u, 0x001020c2u, 0x00000000u,
    0x00004002u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x0100003eu,
};

inline constexpr uint32_t tir_coverage_ps[] = {
    0x43425844u, 0x5cf9c6e8u, 0x6c06b873u, 0x417f1009u, 0xa194871au, 0x00000001u, 0x000000a8u, 0x00000003u,
    0x0000002cu, 0x0000003cu, 0x00000070u, 0x4e475349u, 0x00000008u, 0x00000000u, 0x00000008u, 0x4e47534fu,
    0x0000002cu, 0x00000001u, 0x00000008u, 0x00000020u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u,
    0x00000001u, 0x545f5653u, 0x45475241u, 0xabab0054u, 0x58454853u, 0x00000030u, 0x00000050u, 0x0000000cu,
    0x0200005fu, 0x00023001u, 0x03000065u, 0x00102012u, 0x00000000u, 0x04000036u, 0x00102012u, 0x00000000u,
    0x00023001u, 0x0100003eu,
};

}  // namespace av

#endif  // AV_TIR_COVERAGE_SHADERS_HPP
