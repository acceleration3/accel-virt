#include "proto/submission/blob_import_answer.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

BlobImportAnswer::BlobImportAnswer() : status_(imported), device_memory_(0u)
{
    AV_SCOPE("BlobImportAnswer::BlobImportAnswer");
}

BlobImportAnswer::BlobImportAnswer(uint32_t status, uint64_t device_memory)
    : status_(status), device_memory_(device_memory)
    {
        AV_SCOPE("BlobImportAnswer::BlobImportAnswer");
    }

uint32_t BlobImportAnswer::status() const
{
    AV_SCOPE("BlobImportAnswer::status");
    return status_;
}
uint64_t BlobImportAnswer::device_memory() const
{
    AV_SCOPE("BlobImportAnswer::device_memory");
    return device_memory_;
}

Status BlobImportAnswer::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("BlobImportAnswer::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(status_);
    writer.write_zeros(4u);
    writer.write_u64(device_memory_);
    return writer.status();
}

bool BlobImportAnswer::fields_valid() const
{
    AV_SCOPE("BlobImportAnswer::fields_valid");
    if (status_ == imported) return device_memory_ != 0u;
    return status_ <= no_room && device_memory_ == 0u;
}

Status BlobImportAnswer::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("BlobImportAnswer::decode");
    WireReader       reader(bytes, length);
    BlobImportAnswer candidate;
    candidate.status_ = reader.read_u32();
    reader.expect_zero(4u);
    candidate.device_memory_ = reader.read_u64();
    if (reader.status() != Status::ok || !candidate.fields_valid()) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}
