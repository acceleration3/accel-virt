#include "icd/opencl/cl_buffer_rect.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

namespace {
constexpr size_t nowhere[3] = {};

}

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

bool ClBufferRect::buffer_span(uint64_t &first, uint64_t &end) const
{
    AV_SCOPE("ClBufferRect::buffer_span");
    size_t origin[3] = {};
    buffer_side_.origin_of(0u, 0u, 0u, origin);
    size_t start = origin[0];
    size_t part = 0u;
    if (__builtin_mul_overflow(origin[1], buffer_side_.row_pitch(), &part) ||
        __builtin_add_overflow(start, part, &start) ||
        __builtin_mul_overflow(origin[2], buffer_side_.slice_pitch(), &part) ||
        __builtin_add_overflow(start, part, &start))
        return false;
    size_t reach = 0u;
    if (__builtin_add_overflow(start, buffer_side_.row_offset(buffer_side_.rows() - 1u, buffer_side_.slices() - 1u),
                               &reach) ||
        __builtin_add_overflow(reach, buffer_side_.row_bytes(), &reach))
        return false;
    first = start;
    end = reach;
    return true;
}

bool ClBufferRect::host_span(uint64_t &end) const
{
    AV_SCOPE("ClBufferRect::host_span");
    size_t reach = 0u;
    if (__builtin_add_overflow(host_offset_, application_.row_offset(application_.rows() - 1u, application_.slices() - 1u),
                               &reach) ||
        __builtin_add_overflow(reach, application_.row_bytes(), &reach))
        return false;
    end = reach;
    return true;
}

}
