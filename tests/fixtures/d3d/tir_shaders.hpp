#pragma once

#include "proto/generated/d3d/d3d_wire_types.h"

#include <cstdint>

namespace tir_vs {

inline constexpr std::uint32_t tokens[] = {
    0x00010040u, 0x0000001bu,
    0x0300005fu, 0x001010f2u, 0x00000000u,
    0x0300005fu, 0x00101032u, 0x00000001u,
    0x04000067u, 0x001020f2u, 0x00000000u, 0x00000001u,
    0x04000067u, 0x00102032u, 0x00000001u, 0x00000002u,
    0x05000036u, 0x001020f2u, 0x00000000u, 0x00101e46u, 0x00000000u,
    0x05000036u, 0x00102032u, 0x00000001u, 0x00101046u, 0x00000001u,
    0x0100003eu,
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

}

namespace tir_coverage_ps {

inline constexpr std::uint32_t tokens[] = {
    0x00000050u, 0x0000000cu,
    0x0200005fu, 0x00023001u,
    0x03000065u, 0x00102012u, 0x00000000u,
    0x04000036u, 0x00102012u, 0x00000000u, 0x00023001u,
    0x0100003eu,
};

inline constexpr av::d3dproto::SignatureEntry inputs[] = {
    {0u, 0u, 0u, 0u, 0u, 0u},
};
inline constexpr std::uint32_t inputs_count = 0u;

inline constexpr av::d3dproto::SignatureEntry outputs[] = {
    {0u, 0u, 1u, 1u, 0u, 0u},
};
inline constexpr std::uint32_t outputs_count = 1u;

}

namespace tir_uav_ps {

inline constexpr std::uint32_t tokens[] = {
    0x00000050u, 0x0000001au,
    0x0400189cu, 0x0011e000u, 0x00000000u, 0x00004444u,
    0x04002064u, 0x00101032u, 0x00000000u, 0x00000001u,
    0x0200005fu, 0x00023001u,
    0x02000068u, 0x00000001u,
    0x0500001cu, 0x00100032u, 0x00000000u, 0x00101046u, 0x00000000u,
    0x060000aau, 0x0011e000u, 0x00000000u, 0x00100046u, 0x00000000u,
    0x00023001u,
    0x0100003eu,
};

inline constexpr av::d3dproto::SignatureEntry inputs[] = {
    {1u, 0u, 3u, 3u, 0u, 0u},
};
inline constexpr std::uint32_t inputs_count = 1u;

inline constexpr av::d3dproto::SignatureEntry outputs[] = {
    {0u, 0u, 0u, 0u, 0u, 0u},
};
inline constexpr std::uint32_t outputs_count = 0u;

}

namespace tir_attribute_vs {

inline constexpr std::uint32_t tokens[] = {
    0x00010040u, 0x00000023u,
    0x0300005fu, 0x00101032u, 0x00000000u,
    0x04000067u, 0x001020f2u, 0x00000000u, 0x00000001u,
    0x03000065u, 0x00102012u, 0x00000001u,
    0x05000036u, 0x00102032u, 0x00000000u, 0x00101046u, 0x00000000u,
    0x08000036u, 0x001020c2u, 0x00000000u, 0x00004002u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u,
    0x09000032u, 0x00102012u, 0x00000001u, 0x00101006u, 0x00000000u,
    0x00004001u, 0x42000000u, 0x00004001u, 0x42000000u,
    0x0100003eu,
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

}

namespace tir_eval_ps {

inline constexpr std::uint32_t tokens[] = {
    0x00000050u, 0x00000020u,
    0x03001062u, 0x00101012u, 0x00000001u,
    0x03000065u, 0x00102012u, 0x00000000u,
    0x02000068u, 0x00000001u,
    0x070000ccu, 0x00100012u, 0x00000000u, 0x00101006u, 0x00000001u,
    0x00004001u, 0x0000000cu,
    0x09000032u, 0x00100012u, 0x00000000u, 0x00100006u, 0x00000000u,
    0x00004001u, 0x41800000u, 0x00004001u, 0x3f000000u,
    0x0500001cu, 0x00102012u, 0x00000000u, 0x00100006u, 0x00000000u,
    0x0100003eu,
};

inline constexpr av::d3dproto::SignatureEntry inputs[] = {
    {0u, 1u, 1u, 3u, 0u, 0u},
};
inline constexpr std::uint32_t inputs_count = 1u;

inline constexpr av::d3dproto::SignatureEntry outputs[] = {
    {0u, 0u, 1u, 1u, 0u, 0u},
};
inline constexpr std::uint32_t outputs_count = 1u;

}

namespace tir_clip_ps {

inline constexpr std::uint32_t tokens[] = {
    0x00000050u, 0x0000001bu,
    0x04001064u, 0x00101012u, 0x00000001u, 0x00000002u,
    0x0200005fu, 0x00023001u,
    0x03000065u, 0x00102012u, 0x00000000u,
    0x02000068u, 0x00000001u,
    0x0700001du, 0x00100012u, 0x00000000u, 0x00101006u, 0x00000001u,
    0x00004001u, 0xf149f2cau,
    0x06000001u, 0x00102012u, 0x00000000u, 0x00100006u, 0x00000000u,
    0x00023001u,
    0x0100003eu,
};

inline constexpr av::d3dproto::SignatureEntry inputs[] = {
    {2u, 1u, 1u, 3u, 0u, 0u},
};
inline constexpr std::uint32_t inputs_count = 1u;

inline constexpr av::d3dproto::SignatureEntry outputs[] = {
    {0u, 0u, 1u, 1u, 0u, 0u},
};
inline constexpr std::uint32_t outputs_count = 1u;

}
