#include "executor/opencl/dxgi_cl_format.hpp"

#include "core/trace/function_scope.hpp"

#include <CL/cl.h>

namespace av {

namespace {

struct Named {
    uint32_t dxgi;
    uint32_t order;
    uint32_t type;
};

/* DXGI_FORMAT values on the left, as dxgiformat.h numbers them. */
constexpr Named formats[] = {
    {2u, CL_RGBA, CL_FLOAT},          /* R32G32B32A32_FLOAT */
    {10u, CL_RGBA, CL_HALF_FLOAT},    /* R16G16B16A16_FLOAT */
    {11u, CL_RGBA, CL_UNORM_INT16},   /* R16G16B16A16_UNORM */
    {16u, CL_RG, CL_FLOAT},           /* R32G32_FLOAT */
    {28u, CL_RGBA, CL_UNORM_INT8},    /* R8G8B8A8_UNORM */
    {34u, CL_RG, CL_HALF_FLOAT},      /* R16G16_FLOAT */
    {41u, CL_R, CL_FLOAT},            /* R32_FLOAT */
    {49u, CL_RG, CL_UNORM_INT8},      /* R8G8_UNORM */
    {54u, CL_R, CL_HALF_FLOAT},       /* R16_FLOAT */
    {61u, CL_R, CL_UNORM_INT8},       /* R8_UNORM */
    {87u, CL_BGRA, CL_UNORM_INT8},    /* B8G8R8A8_UNORM */
};

}  // namespace

DxgiClFormat::DxgiClFormat(uint32_t dxgi) : order_(0u), type_(0u)
{
    AV_SCOPE("DxgiClFormat::DxgiClFormat");
    for (const Named &named : formats)
        if (named.dxgi == dxgi) {
            order_ = named.order;
            type_ = named.type;
        }
}

bool DxgiClFormat::known() const
{
    AV_SCOPE("DxgiClFormat::known");
    return order_ != 0u;
}

uint32_t DxgiClFormat::order() const
{
    AV_SCOPE("DxgiClFormat::order");
    return order_;
}

uint32_t DxgiClFormat::type() const
{
    AV_SCOPE("DxgiClFormat::type");
    return type_;
}

}  // namespace av
