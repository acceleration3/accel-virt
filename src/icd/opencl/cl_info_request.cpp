#include "icd/opencl/cl_info_request.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClInfoRequest::ClInfoRequest(size_t value_bytes, void *value, size_t *size_ret)
    : value_bytes_(value_bytes), value_(value), size_ret_(size_ret)
{
    AV_SCOPE("ClInfoRequest::ClInfoRequest");
}

cl_int ClInfoRequest::give(const void *from, size_t bytes) const
{
    AV_SCOPE("ClInfoRequest::give");
    if (value_ != nullptr) {
        if (value_bytes_ < bytes) return CL_INVALID_VALUE;
        const char *const source = static_cast<const char *>(from);
        char *const       out = static_cast<char *>(value_);
        for (size_t index = 0u; index < bytes; ++index) out[index] = source[index];
    }
    if (size_ret_ != nullptr) *size_ret_ = bytes;
    return CL_SUCCESS;
}

}
