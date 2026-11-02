#include "executor/opencl/gl_cl_format.hpp"

#include "core/trace/function_scope.hpp"

#include <CL/cl.h>

namespace av {

namespace {

constexpr uint32_t gl_byte = 0x1400u;
constexpr uint32_t gl_unsigned_byte = 0x1401u;
constexpr uint32_t gl_short = 0x1402u;
constexpr uint32_t gl_unsigned_short = 0x1403u;
constexpr uint32_t gl_int = 0x1404u;
constexpr uint32_t gl_unsigned_int = 0x1405u;
constexpr uint32_t gl_float = 0x1406u;
constexpr uint32_t gl_half_float = 0x140Bu;

constexpr uint32_t gl_red = 0x1903u;
constexpr uint32_t gl_rg = 0x8227u;
constexpr uint32_t gl_rgba = 0x1908u;
constexpr uint32_t gl_red_integer = 0x8D94u;
constexpr uint32_t gl_rg_integer = 0x8228u;
constexpr uint32_t gl_rgba_integer = 0x8D99u;

struct Named {
    uint32_t internal;
    uint32_t order;
    uint32_t type;
    uint32_t format;
    uint32_t transfer;
    uint32_t bytes;
};

constexpr Named formats[] = {
    {0x1908u, CL_RGBA, CL_UNORM_INT8, gl_rgba, gl_unsigned_byte, 4u},
    {0x8058u, CL_RGBA, CL_UNORM_INT8, gl_rgba, gl_unsigned_byte, 4u},
    {0x8C43u, CL_sRGBA, CL_UNORM_INT8, gl_rgba, gl_unsigned_byte, 4u},
    {0x8F97u, CL_RGBA, CL_SNORM_INT8, gl_rgba, gl_byte, 4u},
    {0x805Bu, CL_RGBA, CL_UNORM_INT16, gl_rgba, gl_unsigned_short, 8u},
    {0x8F9Bu, CL_RGBA, CL_SNORM_INT16, gl_rgba, gl_short, 8u},
    {0x881Au, CL_RGBA, CL_HALF_FLOAT, gl_rgba, gl_half_float, 8u},
    {0x8814u, CL_RGBA, CL_FLOAT, gl_rgba, gl_float, 16u},
    {0x8D8Eu, CL_RGBA, CL_SIGNED_INT8, gl_rgba_integer, gl_byte, 4u},
    {0x8D88u, CL_RGBA, CL_SIGNED_INT16, gl_rgba_integer, gl_short, 8u},
    {0x8D82u, CL_RGBA, CL_SIGNED_INT32, gl_rgba_integer, gl_int, 16u},
    {0x8D7Cu, CL_RGBA, CL_UNSIGNED_INT8, gl_rgba_integer, gl_unsigned_byte, 4u},
    {0x8D76u, CL_RGBA, CL_UNSIGNED_INT16, gl_rgba_integer, gl_unsigned_short, 8u},
    {0x8D70u, CL_RGBA, CL_UNSIGNED_INT32, gl_rgba_integer, gl_unsigned_int, 16u},
    {0x8227u, CL_RG, CL_UNORM_INT8, gl_rg, gl_unsigned_byte, 2u},
    {0x822Bu, CL_RG, CL_UNORM_INT8, gl_rg, gl_unsigned_byte, 2u},
    {0x8F95u, CL_RG, CL_SNORM_INT8, gl_rg, gl_byte, 2u},
    {0x822Cu, CL_RG, CL_UNORM_INT16, gl_rg, gl_unsigned_short, 4u},
    {0x8F99u, CL_RG, CL_SNORM_INT16, gl_rg, gl_short, 4u},
    {0x822Fu, CL_RG, CL_HALF_FLOAT, gl_rg, gl_half_float, 4u},
    {0x8230u, CL_RG, CL_FLOAT, gl_rg, gl_float, 8u},
    {0x8237u, CL_RG, CL_SIGNED_INT8, gl_rg_integer, gl_byte, 2u},
    {0x8239u, CL_RG, CL_SIGNED_INT16, gl_rg_integer, gl_short, 4u},
    {0x823Bu, CL_RG, CL_SIGNED_INT32, gl_rg_integer, gl_int, 8u},
    {0x8238u, CL_RG, CL_UNSIGNED_INT8, gl_rg_integer, gl_unsigned_byte, 2u},
    {0x823Au, CL_RG, CL_UNSIGNED_INT16, gl_rg_integer, gl_unsigned_short, 4u},
    {0x823Cu, CL_RG, CL_UNSIGNED_INT32, gl_rg_integer, gl_unsigned_int, 8u},
    {0x1903u, CL_R, CL_UNORM_INT8, gl_red, gl_unsigned_byte, 1u},
    {0x8229u, CL_R, CL_UNORM_INT8, gl_red, gl_unsigned_byte, 1u},
    {0x8F94u, CL_R, CL_SNORM_INT8, gl_red, gl_byte, 1u},
    {0x822Au, CL_R, CL_UNORM_INT16, gl_red, gl_unsigned_short, 2u},
    {0x8F98u, CL_R, CL_SNORM_INT16, gl_red, gl_short, 2u},
    {0x822Du, CL_R, CL_HALF_FLOAT, gl_red, gl_half_float, 2u},
    {0x822Eu, CL_R, CL_FLOAT, gl_red, gl_float, 4u},
    {0x8231u, CL_R, CL_SIGNED_INT8, gl_red_integer, gl_byte, 1u},
    {0x8233u, CL_R, CL_SIGNED_INT16, gl_red_integer, gl_short, 2u},
    {0x8235u, CL_R, CL_SIGNED_INT32, gl_red_integer, gl_int, 4u},
    {0x8232u, CL_R, CL_UNSIGNED_INT8, gl_red_integer, gl_unsigned_byte, 1u},
    {0x8234u, CL_R, CL_UNSIGNED_INT16, gl_red_integer, gl_unsigned_short, 2u},
    {0x8236u, CL_R, CL_UNSIGNED_INT32, gl_red_integer, gl_unsigned_int, 4u},
};

}

GlClFormat::GlClFormat(uint32_t internal_format)
    : order_(0u), type_(0u), transfer_format_(0u), transfer_type_(0u), texel_bytes_(0u)
{
    AV_SCOPE("GlClFormat::GlClFormat");
    for (const Named &named : formats)
        if (named.internal == internal_format) {
            order_ = named.order;
            type_ = named.type;
            transfer_format_ = named.format;
            transfer_type_ = named.transfer;
            texel_bytes_ = named.bytes;
        }
}

bool GlClFormat::known() const
{
    AV_SCOPE("GlClFormat::known");
    return order_ != 0u;
}

uint32_t GlClFormat::order() const
{
    AV_SCOPE("GlClFormat::order");
    return order_;
}

uint32_t GlClFormat::type() const
{
    AV_SCOPE("GlClFormat::type");
    return type_;
}

uint32_t GlClFormat::transfer_format() const
{
    AV_SCOPE("GlClFormat::transfer_format");
    return transfer_format_;
}

uint32_t GlClFormat::transfer_type() const
{
    AV_SCOPE("GlClFormat::transfer_type");
    return transfer_type_;
}

uint32_t GlClFormat::texel_bytes() const
{
    AV_SCOPE("GlClFormat::texel_bytes");
    return texel_bytes_;
}

}
