#include "proto/submission/blob_import_answer.hpp"

#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

BlobImportAnswer::BlobImportAnswer() : status_(imported), device_memory_(0u) {}

BlobImportAnswer::BlobImportAnswer(uint32_t status, uint64_t device_memory)
    : status_(status), device_memory_(device_memory) {}

uint32_t BlobImportAnswer::status() const        { return status_; }
uint64_t BlobImportAnswer::device_memory() const { return device_memory_; }

Status BlobImportAnswer::encode(uint8_t *bytes, size_t capacity) const
{
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(status_);
    writer.write_zeros(4u);
    writer.write_u64(device_memory_);
    return writer.status();
}

bool BlobImportAnswer::fields_valid() const
{
    if (status_ == imported) return device_memory_ != 0u;
    return status_ <= no_room && device_memory_ == 0u;
}

Status BlobImportAnswer::decode(const uint8_t *bytes, size_t length)
{
    WireReader       reader(bytes, length);
    BlobImportAnswer candidate;
    candidate.status_ = reader.read_u32();
    reader.expect_zero(4u);
    candidate.device_memory_ = reader.read_u64();
    if (reader.status() != Status::ok || !candidate.fields_valid()) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}  // namespace av
