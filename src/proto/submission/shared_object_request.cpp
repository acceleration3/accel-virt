#include "proto/submission/shared_object_request.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

SharedObjectRequest::SharedObjectRequest() : object_id_(0u), kind_(), wire_handle_(0u), size_(0u)
{
    AV_SCOPE("SharedObjectRequest::SharedObjectRequest");
}

SharedObjectRequest::SharedObjectRequest(uint64_t object_id, const SharedObjectKind &kind,
                                         uint64_t wire_handle, uint64_t size)
    : object_id_(object_id), kind_(kind), wire_handle_(wire_handle), size_(size)
    {
        AV_SCOPE("SharedObjectRequest::SharedObjectRequest");
    }

uint64_t SharedObjectRequest::object_id() const
{
    AV_SCOPE("SharedObjectRequest::object_id");
    return object_id_;
}

const SharedObjectKind &SharedObjectRequest::kind() const
{
    AV_SCOPE("SharedObjectRequest::kind");
    return kind_;
}

uint64_t SharedObjectRequest::wire_handle() const
{
    AV_SCOPE("SharedObjectRequest::wire_handle");
    return wire_handle_;
}

uint64_t SharedObjectRequest::size() const
{
    AV_SCOPE("SharedObjectRequest::size");
    return size_;
}

Status SharedObjectRequest::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("SharedObjectRequest::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u64(object_id_);
    writer.write_u32(kind_.value());
    writer.write_zeros(4u);
    writer.write_u64(wire_handle_);
    writer.write_u64(size_);
    return writer.status();
}

Status SharedObjectRequest::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("SharedObjectRequest::decode");
    if (length != encoded_size) return Status::malformed;
    WireReader          reader(bytes, length);
    SharedObjectRequest candidate;
    candidate.object_id_ = reader.read_u64();
    candidate.kind_ = SharedObjectKind(reader.read_u32());
    const uint32_t reserved = reader.read_u32();
    candidate.wire_handle_ = reader.read_u64();
    candidate.size_ = reader.read_u64();
    if (candidate.object_id_ == 0u || !candidate.kind_.known() || reserved != 0u ||
        !candidate.kind_.sized(candidate.size_))
        return Status::invalid_parameter;
    *this = candidate;
    return Status::ok;
}

}  // namespace av
