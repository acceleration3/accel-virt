#include "executor/opencl/cl_buffer_bounds.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

bool ClBufferBounds::size_of(cl_mem buffer, size_t &bytes)
{
    AV_SCOPE("ClBufferBounds::size_of");
    bytes = 0u;
    return buffer != nullptr && ::clGetMemObjectInfo(buffer, CL_MEM_SIZE, sizeof bytes, &bytes, nullptr) == CL_SUCCESS;
}

bool ClBufferBounds::range_inside(cl_mem buffer, size_t offset, size_t size)
{
    AV_SCOPE("ClBufferBounds::range_inside");
    size_t whole = 0u;
    if (!size_of(buffer, whole)) return true;
    return offset <= whole && size <= whole - offset;
}

bool ClBufferBounds::rect_inside(cl_mem buffer, const size_t *origin, const size_t *region, size_t row_pitch,
                                 size_t slice_pitch)
{
    AV_SCOPE("ClBufferBounds::rect_inside");
    size_t whole = 0u;
    if (!size_of(buffer, whole)) return true;
    if (origin == nullptr || region == nullptr) return true;
    if (region[0] == 0u || region[1] == 0u || region[2] == 0u) return true;
    const size_t row = row_pitch != 0u ? row_pitch : region[0];
    const size_t slice = slice_pitch != 0u ? slice_pitch : region[1] * row;
    if (row < region[0] || slice < region[1] * row) return true;
    if (origin[2] > whole / slice || origin[1] > whole / row) return false;
    size_t start = origin[2] * slice;
    const size_t rows = origin[1] * row;
    if (rows > whole - start) return false;
    start += rows;
    if (origin[0] > whole - start) return false;
    start += origin[0];
    const size_t span_slices = (region[2] - 1u) * slice;
    const size_t span_rows = (region[1] - 1u) * row;
    if ((region[2] - 1u) != 0u && span_slices / (region[2] - 1u) != slice) return false;
    if ((region[1] - 1u) != 0u && span_rows / (region[1] - 1u) != row) return false;
    if (span_slices > whole - start) return false;
    start += span_slices;
    if (span_rows > whole - start) return false;
    start += span_rows;
    return region[0] <= whole - start;
}

}
