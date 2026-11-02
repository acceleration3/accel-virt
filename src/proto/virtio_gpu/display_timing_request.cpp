#include "proto/virtio_gpu/display_timing_request.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

DisplayTimingRequest::DisplayTimingRequest() : head_(0u), refresh_millihz_(0u)
{
    AV_SCOPE("DisplayTimingRequest::DisplayTimingRequest");
}

Status DisplayTimingRequest::decode(WireReader &reader)
{
    AV_SCOPE("DisplayTimingRequest::decode");
    if (reader.remaining() < encoded_size) return Status::malformed;
    reader.skip(header_size);
    const uint32_t head = reader.read_u32();
    const uint32_t refresh = reader.read_u32();
    if (refresh < least_millihz || refresh > most_millihz) return Status::malformed;
    head_ = head;
    refresh_millihz_ = refresh;
    return Status::ok;
}

uint32_t DisplayTimingRequest::head() const
{
    AV_SCOPE("DisplayTimingRequest::head");
    return head_;
}

uint32_t DisplayTimingRequest::refresh_millihz() const
{
    AV_SCOPE("DisplayTimingRequest::refresh_millihz");
    return refresh_millihz_;
}

}
