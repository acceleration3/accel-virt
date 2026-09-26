#include "device/renderer/forwarded_request.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ForwardedRequest::ForwardedRequest(const uint8_t *bytes, size_t length) : bytes_(bytes), length_(length)
{
    AV_SCOPE("ForwardedRequest::ForwardedRequest");
}

size_t ForwardedRequest::read(size_t offset, uint8_t *buffer, size_t size) const
{
    AV_SCOPE("ForwardedRequest::read");
    if (offset >= length_) return 0u;
    const size_t count = size < length_ - offset ? size : length_ - offset;
    __builtin_memcpy(buffer, bytes_ + offset, count);
    return count;
}

}  // namespace av
