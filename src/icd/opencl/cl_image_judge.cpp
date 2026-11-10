#include "icd/opencl/cl_image_judge.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClImageJudge::ClImageJudge(const ClImageLimits &limits) : limits_(limits)
{
    AV_SCOPE("ClImageJudge::ClImageJudge");
}

bool ClImageJudge::over(size_t value, size_t limit) const
{
    AV_SCOPE("ClImageJudge::over");
    return value == 0u || (limit != 0u && value > limit);
}

cl_int ClImageJudge::judge_size(const cl_image_desc &desc) const
{
    AV_SCOPE("ClImageJudge::judge_size");
    switch (desc.image_type) {
    case CL_MEM_OBJECT_IMAGE1D:
        return over(desc.image_width, limits_.width_2d_) ? CL_INVALID_IMAGE_SIZE : CL_SUCCESS;
    case CL_MEM_OBJECT_IMAGE1D_BUFFER:
        return over(desc.image_width, limits_.buffer_size_) ? CL_INVALID_IMAGE_SIZE : CL_SUCCESS;
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        return over(desc.image_width, limits_.width_2d_) || over(desc.image_array_size, limits_.array_size_)
                   ? CL_INVALID_IMAGE_SIZE
                   : CL_SUCCESS;
    case CL_MEM_OBJECT_IMAGE2D:
        return over(desc.image_width, limits_.width_2d_) || over(desc.image_height, limits_.height_2d_)
                   ? CL_INVALID_IMAGE_SIZE
                   : CL_SUCCESS;
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        return over(desc.image_width, limits_.width_2d_) || over(desc.image_height, limits_.height_2d_) ||
                       over(desc.image_array_size, limits_.array_size_)
                   ? CL_INVALID_IMAGE_SIZE
                   : CL_SUCCESS;
    case CL_MEM_OBJECT_IMAGE3D:
        return over(desc.image_width, limits_.width_3d_) || over(desc.image_height, limits_.height_3d_) ||
                       over(desc.image_depth, limits_.depth_3d_)
                   ? CL_INVALID_IMAGE_SIZE
                   : CL_SUCCESS;
    default:
        return CL_INVALID_IMAGE_DESCRIPTOR;
    }
}

cl_int ClImageJudge::judge(const cl_image_desc &desc, uint32_t element_bytes, bool has_buffer, uint64_t buffer_bytes,
                           bool has_host_pointer) const
{
    AV_SCOPE("ClImageJudge::judge");
    const cl_int sized = judge_size(desc);
    if (sized != CL_SUCCESS) return sized;
    const bool buffer_type = desc.image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER;
    const bool may_have_buffer = buffer_type || desc.image_type == CL_MEM_OBJECT_IMAGE2D;
    if (buffer_type && !has_buffer) return CL_INVALID_IMAGE_DESCRIPTOR;
    if (has_buffer && !may_have_buffer) return CL_INVALID_IMAGE_DESCRIPTOR;
    if (has_buffer && has_host_pointer) return CL_INVALID_IMAGE_DESCRIPTOR;
    if (element_bytes == 0u) return CL_SUCCESS;
    const bool    backed = has_buffer || has_host_pointer;
    const uint64_t row_bytes = static_cast<uint64_t>(desc.image_width) * element_bytes;
    if (!backed) return desc.image_row_pitch == 0u && desc.image_slice_pitch == 0u ? CL_SUCCESS : CL_INVALID_IMAGE_DESCRIPTOR;
    if (desc.image_row_pitch != 0u) {
        if (desc.image_row_pitch < row_bytes || desc.image_row_pitch % element_bytes != 0u)
            return CL_INVALID_IMAGE_DESCRIPTOR;
        if (has_buffer && limits_.pitch_alignment_ != 0u &&
            (desc.image_row_pitch / element_bytes) % limits_.pitch_alignment_ != 0u)
            return CL_INVALID_IMAGE_DESCRIPTOR;
    }
    if (has_buffer) {
        const uint64_t pitch = desc.image_row_pitch != 0u ? desc.image_row_pitch : row_bytes;
        const uint64_t bytes = buffer_type ? row_bytes : pitch * desc.image_height;
        if (bytes > buffer_bytes) return CL_INVALID_IMAGE_DESCRIPTOR;
    }
    return CL_SUCCESS;
}

}
