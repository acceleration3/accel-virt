#include "proto/submission/shared_object_answer.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

SharedObjectAnswer::SharedObjectAnswer() : status_(done), wire_handle_(0u)
{
    AV_SCOPE("SharedObjectAnswer::SharedObjectAnswer");
}

SharedObjectAnswer::SharedObjectAnswer(uint32_t status, uint64_t wire_handle)
    : status_(status), wire_handle_(wire_handle)
    {
        AV_SCOPE("SharedObjectAnswer::SharedObjectAnswer");
    }

uint32_t SharedObjectAnswer::status() const
{
    AV_SCOPE("SharedObjectAnswer::status");
    return status_;
}

uint64_t SharedObjectAnswer::wire_handle() const
{
    AV_SCOPE("SharedObjectAnswer::wire_handle");
    return wire_handle_;
}

bool SharedObjectAnswer::succeeded() const
{
    AV_SCOPE("SharedObjectAnswer::succeeded");
    return status_ == done;
}

Status SharedObjectAnswer::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("SharedObjectAnswer::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(status_);
    writer.write_zeros(4u);
    writer.write_u64(wire_handle_);
    return writer.status();
}

bool SharedObjectAnswer::fields_valid() const
{
    AV_SCOPE("SharedObjectAnswer::fields_valid");
    if (status_ == done) return true;
    return status_ <= conflict && wire_handle_ == 0u;
}

Status SharedObjectAnswer::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("SharedObjectAnswer::decode");
    WireReader         reader(bytes, length);
    SharedObjectAnswer candidate;
    candidate.status_ = reader.read_u32();
    reader.expect_zero(4u);
    candidate.wire_handle_ = reader.read_u64();
    if (reader.status() != Status::ok || !candidate.fields_valid()) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}
