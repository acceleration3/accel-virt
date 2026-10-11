#include "icd/opencl/dxgi_image_format.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

namespace {

struct Mapping {
    uint32_t         dxgi;
    cl_channel_order order;
    cl_channel_type  type;
};

/* The extension's table, by DXGI_FORMAT's own numbers (dxgiformat.h), with the
 * four Khronos added to it later: BGRA, the two sRGB orders and alpha alone. */
constexpr Mapping mappings[] = {
    {2u, CL_RGBA, CL_FLOAT},          /* R32G32B32A32_FLOAT  */
    {3u, CL_RGBA, CL_UNSIGNED_INT32}, /* R32G32B32A32_UINT   */
    {4u, CL_RGBA, CL_SIGNED_INT32},   /* R32G32B32A32_SINT   */
    {10u, CL_RGBA, CL_HALF_FLOAT},    /* R16G16B16A16_FLOAT  */
    {11u, CL_RGBA, CL_UNORM_INT16},   /* R16G16B16A16_UNORM  */
    {12u, CL_RGBA, CL_UNSIGNED_INT16},/* R16G16B16A16_UINT   */
    {13u, CL_RGBA, CL_SNORM_INT16},   /* R16G16B16A16_SNORM  */
    {14u, CL_RGBA, CL_SIGNED_INT16},  /* R16G16B16A16_SINT   */
    {16u, CL_RG, CL_FLOAT},           /* R32G32_FLOAT        */
    {17u, CL_RG, CL_UNSIGNED_INT32},  /* R32G32_UINT         */
    {18u, CL_RG, CL_SIGNED_INT32},    /* R32G32_SINT         */
    {28u, CL_RGBA, CL_UNORM_INT8},    /* R8G8B8A8_UNORM      */
    {29u, CL_sRGBA, CL_UNORM_INT8},   /* R8G8B8A8_UNORM_SRGB */
    {30u, CL_RGBA, CL_UNSIGNED_INT8}, /* R8G8B8A8_UINT       */
    {31u, CL_RGBA, CL_SNORM_INT8},    /* R8G8B8A8_SNORM      */
    {32u, CL_RGBA, CL_SIGNED_INT8},   /* R8G8B8A8_SINT       */
    {34u, CL_RG, CL_HALF_FLOAT},      /* R16G16_FLOAT        */
    {35u, CL_RG, CL_UNORM_INT16},     /* R16G16_UNORM        */
    {36u, CL_RG, CL_UNSIGNED_INT16},  /* R16G16_UINT         */
    {37u, CL_RG, CL_SNORM_INT16},     /* R16G16_SNORM        */
    {38u, CL_RG, CL_SIGNED_INT16},    /* R16G16_SINT         */
    {41u, CL_R, CL_FLOAT},            /* R32_FLOAT           */
    {42u, CL_R, CL_UNSIGNED_INT32},   /* R32_UINT            */
    {43u, CL_R, CL_SIGNED_INT32},     /* R32_SINT            */
    {49u, CL_RG, CL_UNORM_INT8},      /* R8G8_UNORM          */
    {50u, CL_RG, CL_UNSIGNED_INT8},   /* R8G8_UINT           */
    {51u, CL_RG, CL_SNORM_INT8},      /* R8G8_SNORM          */
    {52u, CL_RG, CL_SIGNED_INT8},     /* R8G8_SINT           */
    {54u, CL_R, CL_HALF_FLOAT},       /* R16_FLOAT           */
    {56u, CL_R, CL_UNORM_INT16},      /* R16_UNORM           */
    {57u, CL_R, CL_UNSIGNED_INT16},   /* R16_UINT            */
    {58u, CL_R, CL_SNORM_INT16},      /* R16_SNORM           */
    {59u, CL_R, CL_SIGNED_INT16},     /* R16_SINT            */
    {61u, CL_R, CL_UNORM_INT8},       /* R8_UNORM            */
    {62u, CL_R, CL_UNSIGNED_INT8},    /* R8_UINT             */
    {63u, CL_R, CL_SNORM_INT8},       /* R8_SNORM            */
    {64u, CL_R, CL_SIGNED_INT8},      /* R8_SINT             */
    {65u, CL_A, CL_UNORM_INT8},       /* A8_UNORM            */
    {87u, CL_BGRA, CL_UNORM_INT8},    /* B8G8R8A8_UNORM      */
    {91u, CL_sBGRA, CL_UNORM_INT8},   /* B8G8R8A8_UNORM_SRGB */
};

}  // namespace

DxgiImageFormat::DxgiImageFormat(uint32_t dxgi_format) : format_{0u, 0u}, mapped_(false)
{
    AV_SCOPE("DxgiImageFormat::DxgiImageFormat");
    for (const Mapping &mapping : mappings) {
        if (mapping.dxgi != dxgi_format) continue;
        format_ = cl_image_format{mapping.order, mapping.type};
        mapped_ = true;
        return;
    }
}

bool DxgiImageFormat::mapped() const
{
    AV_SCOPE("DxgiImageFormat::mapped");
    return mapped_;
}

cl_image_format DxgiImageFormat::format() const
{
    AV_SCOPE("DxgiImageFormat::format");
    return format_;
}

}  // namespace av::clicd
