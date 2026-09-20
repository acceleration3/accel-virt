#include "proto/virtio_gpu/flush_request.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

FlushRequest::FlushRequest() : resource_id_(0u), decoded_(false)
{
    AV_SCOPE("FlushRequest::FlushRequest");
}

Status FlushRequest::decode(WireReader &reader)
{
    AV_SCOPE("FlushRequest::decode");
    for (size_t byte = 0u; byte < header_size; byte++) static_cast<void>(reader.read_u8());
    if (rect_.decode(reader) != Status::ok) {
        forget();
        return Status::malformed;
    }
    resource_id_ = reader.read_u32();
    const uint32_t padding = reader.read_u32();
    if (reader.status() != Status::ok || padding != 0u) {
        forget();
        return Status::malformed;
    }
    decoded_ = true;
    return Status::ok;
}

void FlushRequest::forget()
{
    AV_SCOPE("FlushRequest::forget");
    rect_ = GpuRect();
    resource_id_ = 0u;
    decoded_ = false;
}

const GpuRect &FlushRequest::rect() const
{
    AV_SCOPE("FlushRequest::rect");
    return rect_;
}

uint32_t FlushRequest::resource_id() const
{
    AV_SCOPE("FlushRequest::resource_id");
    return resource_id_;
}

bool FlushRequest::decoded() const
{
    AV_SCOPE("FlushRequest::decoded");
    return decoded_;
}

bool FlushRequest::invariants_hold() const
{
    AV_SCOPE("FlushRequest::invariants_hold");
    if (!rect_.invariants_hold()) return false;
    return decoded_ || resource_id_ == 0u;
}

}  // namespace av
