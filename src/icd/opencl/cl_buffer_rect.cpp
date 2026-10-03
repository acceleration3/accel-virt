#include "icd/opencl/cl_buffer_rect.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

namespace {

/* Read in place of a null argument, so neither side reads what the
 * application did not give; check() refuses the rectangle all the same. */
constexpr size_t nowhere[3] = {};

}  // namespace

ClBufferRect::ClBufferRect(const size_t *buffer_origin, const size_t *host_origin, const size_t *region,
                           size_t buffer_row_pitch, size_t buffer_slice_pitch, size_t host_row_pitch,
                           size_t host_slice_pitch)
    : buffer_side_(buffer_origin != nullptr ? buffer_origin : nowhere, region != nullptr ? region : nowhere, buffer_row_pitch, buffer_slice_pitch, 1u),
      application_(nowhere, region != nullptr ? region : nowhere, host_row_pitch, host_slice_pitch, 1u), host_offset_(0u),
      named_(buffer_origin != nullptr && host_origin != nullptr && region != nullptr), reachable_(false)
{
    AV_SCOPE("ClBufferRect::ClBufferRect");
    reachable_ = application_.offset_of(host_origin != nullptr ? host_origin : nowhere, host_offset_);
}

cl_int ClBufferRect::check() const
{
    AV_SCOPE("ClBufferRect::check");
    if (!named_ || !buffer_side_.valid() || !application_.valid()) return CL_INVALID_VALUE;
    if (!buffer_side_.slices_are_whole_rows() || !application_.slices_are_whole_rows() || !reachable_)
        return CL_INVALID_VALUE;
    if (application_.width() > most_countable || application_.rows() > most_countable ||
        application_.row_pitch() > most_countable)
        return CL_OUT_OF_RESOURCES;
    return CL_SUCCESS;
}

const ClImageRegion &ClBufferRect::buffer_side() const
{
    AV_SCOPE("ClBufferRect::buffer_side");
    return buffer_side_;
}

const ClImageRegion &ClBufferRect::application() const
{
    AV_SCOPE("ClBufferRect::application");
    return application_;
}

size_t ClBufferRect::host_offset() const
{
    AV_SCOPE("ClBufferRect::host_offset");
    return host_offset_;
}

}  // namespace av::clicd
