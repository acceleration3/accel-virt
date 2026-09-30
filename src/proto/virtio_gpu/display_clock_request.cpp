#include "proto/virtio_gpu/display_clock_request.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

DisplayClockRequest::DisplayClockRequest() : address_(0u)
{
    AV_SCOPE("DisplayClockRequest::DisplayClockRequest");
}

Status DisplayClockRequest::decode(WireReader &reader)
{
    AV_SCOPE("DisplayClockRequest::decode");
    if (reader.remaining() < encoded_size) return Status::malformed;
    for (size_t byte = 0u; byte < header_size; byte++) static_cast<void>(reader.read_u8());
    const uint64_t address = reader.read_u64();
    if (address % page_size != 0u) return Status::malformed;
    address_ = address;
    return Status::ok;
}

uint64_t DisplayClockRequest::address() const
{
    AV_SCOPE("DisplayClockRequest::address");
    return address_;
}

}  // namespace av
