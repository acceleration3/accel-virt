#include "proto/virtio_gpu/page_request.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"

namespace av {

PageRequest::PageRequest() : address_(0u)
{
    AV_SCOPE("PageRequest::PageRequest");
}

Status PageRequest::decode(WireReader &reader)
{
    AV_SCOPE("PageRequest::decode");
    if (reader.remaining() < encoded_size) return Status::malformed;
    reader.skip(header_size);
    const uint64_t address = reader.read_u64();
    if (address % page_size != 0u) return Status::malformed;
    address_ = address;
    return Status::ok;
}

uint64_t PageRequest::address() const
{
    AV_SCOPE("PageRequest::address");
    return address_;
}

}  // namespace av
