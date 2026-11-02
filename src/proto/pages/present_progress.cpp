#include "proto/pages/present_progress.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

PresentProgress::PresentProgress() : completed_(0u), refused_(0u)
{
    AV_SCOPE("PresentProgress::PresentProgress");
}

PresentProgress::PresentProgress(uint64_t completed, uint64_t refused) : completed_(completed), refused_(refused)
{
    AV_SCOPE("PresentProgress::PresentProgress");
}

uint64_t PresentProgress::completed() const
{
    AV_SCOPE("PresentProgress::completed");
    return completed_;
}

uint64_t PresentProgress::refused() const
{
    AV_SCOPE("PresentProgress::refused");
    return refused_;
}

Status PresentProgress::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("PresentProgress::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u32(version);
    writer.write_u64(completed_);
    writer.write_u64(refused_);
    return writer.status();
}

Status PresentProgress::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("PresentProgress::decode");
    WireReader     reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint32_t found_version = reader.read_u32();
    const uint64_t completed = reader.read_u64();
    const uint64_t refused = reader.read_u64();
    if (reader.status() != Status::ok) return reader.status();
    if (found_magic != magic || found_version != version || refused > completed) return Status::malformed;
    completed_ = completed;
    refused_ = refused;
    return Status::ok;
}

}
