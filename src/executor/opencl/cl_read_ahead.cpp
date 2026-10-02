#include "executor/opencl/cl_read_ahead.hpp"

#include "core/trace/function_scope.hpp"

#include <sys/mman.h>

namespace av {

ClReadAhead::ClReadAhead() : bytes_(nullptr), queue_(nullptr), buffer_(nullptr), offset_(0u), held_(0u)
{
    AV_SCOPE("ClReadAhead::ClReadAhead");
}

ClReadAhead::~ClReadAhead()
{
    AV_SCOPE("ClReadAhead::~ClReadAhead");
    if (bytes_ != nullptr) static_cast<void>(munmap(bytes_, cache_bytes));
}

bool ClReadAhead::read(cl_command_queue queue, cl_mem buffer, size_t offset, size_t size, void *into, cl_int &result)
{
    AV_SCOPE("ClReadAhead::read");
    if (size == 0u || size > cache_bytes || into == nullptr) return false;
    if (held_ != 0u && queue == queue_ && buffer == buffer_ && offset >= offset_ && size <= held_ &&
        offset - offset_ <= held_ - size) {
        __builtin_memcpy(into, bytes_ + (offset - offset_), size);
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
    if (bytes_ == nullptr) {
        void *const mapped = mmap(nullptr, cache_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped == MAP_FAILED) return false;
        bytes_ = static_cast<uint8_t *>(mapped);
    }
    if (::clEnqueueReadBuffer(queue, buffer, CL_TRUE, offset, span, bytes_, 0u, nullptr, nullptr) != CL_SUCCESS)
        return false;
    queue_ = queue;
    buffer_ = buffer;
    offset_ = offset;
    held_ = span;
    __builtin_memcpy(into, bytes_, size);
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
