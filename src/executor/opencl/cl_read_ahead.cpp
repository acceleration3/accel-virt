#include "executor/opencl/cl_read_ahead.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ClReadAhead::ClReadAhead() : bytes_(), queue_(nullptr), buffer_(nullptr), offset_(0u), held_(0u)
{
    AV_SCOPE("ClReadAhead::ClReadAhead");
}

bool ClReadAhead::read(cl_command_queue queue, cl_mem buffer, size_t offset, size_t size, void *into, cl_int &result)
{
    AV_SCOPE("ClReadAhead::read");
    if (size == 0u || size > cache_bytes || into == nullptr) return false;
    if (held_ != 0u && queue == queue_ && buffer == buffer_ && offset >= offset_ && size <= held_ &&
        offset - offset_ <= held_ - size) {
        __builtin_memcpy(into, bytes_.bytes() + (offset - offset_), size);
        result = CL_SUCCESS;
        return true;
    }
    forget();
    size_t total = 0u;
    if (::clGetMemObjectInfo(buffer, CL_MEM_SIZE, sizeof total, &total, nullptr) != CL_SUCCESS || offset >= total)
        return false;
    const size_t span = total - offset < cache_bytes ? total - offset : cache_bytes;
    /* A read that is the rest of the buffer already gains nothing. */
    if (span <= size) return false;
    if (!bytes_.reserve(span) ||
        ::clEnqueueReadBuffer(queue, buffer, CL_TRUE, offset, span, bytes_.bytes(), 0u, nullptr, nullptr) != CL_SUCCESS)
        return false;
    queue_ = queue;
    buffer_ = buffer;
    offset_ = offset;
    held_ = span;
    __builtin_memcpy(into, bytes_.bytes(), size);
    result = CL_SUCCESS;
    return true;
}

void ClReadAhead::forget()
{
    AV_SCOPE("ClReadAhead::forget");
    held_ = 0u;
    queue_ = nullptr;
    buffer_ = nullptr;
}

}  // namespace av
