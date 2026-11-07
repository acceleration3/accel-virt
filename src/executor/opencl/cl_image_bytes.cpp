#include "executor/opencl/cl_image_bytes.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint32_t default_texel_bytes = 4u;

uint32_t channel_count(cl_channel_order order)
{
    AV_SCOPE("channel_count");
    switch (order) {
    case CL_R:
    case CL_A:
    case CL_INTENSITY:
    case CL_LUMINANCE:
    case CL_DEPTH: return 1u;
    case CL_RG:
    case CL_RA:
    case CL_Rx: return 2u;
    case CL_RGB:
    case CL_RGx: return 3u;
    default: return 4u;
    }
}

uint32_t channel_bytes(cl_channel_type type)
{
    AV_SCOPE("channel_bytes");
    switch (type) {
    case CL_SNORM_INT8:
    case CL_UNORM_INT8:
    case CL_SIGNED_INT8:
    case CL_UNSIGNED_INT8: return 1u;
    case CL_SNORM_INT16:
    case CL_UNORM_INT16:
    case CL_SIGNED_INT16:
    case CL_UNSIGNED_INT16:
    case CL_HALF_FLOAT: return 2u;
    default: return 4u;
    }
}

bool packed(cl_channel_type type)
{
    AV_SCOPE("packed");
    return type == CL_UNORM_SHORT_565 || type == CL_UNORM_SHORT_555 || type == CL_UNORM_INT_101010;
}

}

ClImageBytes::ClImageBytes(const cl_image_format *format, const cl_image_desc *desc) : format_(format), desc_(desc)
{
    AV_SCOPE("ClImageBytes::ClImageBytes");
}

uint32_t ClImageBytes::texel_bytes() const
{
    AV_SCOPE("ClImageBytes::texel_bytes");
    if (format_ == nullptr) return default_texel_bytes;
    if (packed(format_->image_channel_data_type)) return format_->image_channel_data_type == CL_UNORM_INT_101010 ? 4u : 2u;
    return channel_count(format_->image_channel_order) * channel_bytes(format_->image_channel_data_type);
}

uint64_t ClImageBytes::bytes() const
{
    AV_SCOPE("ClImageBytes::bytes");
    if (desc_ == nullptr || desc_->mem_object != nullptr) return 0u;
    const uint64_t height = desc_->image_height != 0u ? desc_->image_height : 1u;
    const uint64_t depth = desc_->image_depth != 0u ? desc_->image_depth : 1u;
    const uint64_t layers = desc_->image_array_size != 0u ? desc_->image_array_size : 1u;
    uint64_t       total = static_cast<uint64_t>(desc_->image_width) * height * depth * layers * texel_bytes();
    if (desc_->num_mip_levels > 1u) total += total / 3u;
    return total;
}

}
