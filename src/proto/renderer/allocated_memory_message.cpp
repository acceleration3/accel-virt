#include "proto/renderer/allocated_memory_message.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

AllocatedMemoryMessage::AllocatedMemoryMessage() : caching_(0u), size_(0u)
{
    AV_SCOPE("AllocatedMemoryMessage::AllocatedMemoryMessage");
}

AllocatedMemoryMessage::AllocatedMemoryMessage(uint32_t caching, uint64_t size) : caching_(caching), size_(size)
{
    AV_SCOPE("AllocatedMemoryMessage::AllocatedMemoryMessage");
}

bool AllocatedMemoryMessage::allocated() const
{
    AV_SCOPE("AllocatedMemoryMessage::allocated");
    return caching_ != 0u;
}

uint32_t AllocatedMemoryMessage::caching() const
{
    AV_SCOPE("AllocatedMemoryMessage::caching");
    return caching_;
}

uint64_t AllocatedMemoryMessage::size() const
{
    AV_SCOPE("AllocatedMemoryMessage::size");
    return size_;
}

uint32_t AllocatedMemoryMessage::descriptors() const
{
    AV_SCOPE("AllocatedMemoryMessage::descriptors");
    return caching_ != 0u ? 1u : 0u;
}

bool AllocatedMemoryMessage::fields_valid() const
{
    AV_SCOPE("AllocatedMemoryMessage::fields_valid");
    if (caching_ == 0u) return size_ == 0u;
    return (caching_ == write_back || caching_ == write_combined) && size_ != 0u;
}

Status AllocatedMemoryMessage::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("AllocatedMemoryMessage::encode");
    if (bytes == nullptr || capacity < encoded_size) return Status::too_large;
    if (!fields_valid()) return Status::invalid_parameter;
    WireWriter writer(bytes, capacity);
    writer.write_u32(caching_);
    writer.write_u32(0u);
    writer.write_u64(size_);
    return writer.status();
}

Status AllocatedMemoryMessage::decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count)
{
    AV_SCOPE("AllocatedMemoryMessage::decode");
    if (bytes == nullptr || length != encoded_size) return Status::malformed;
    WireReader             reader(bytes, length);
    AllocatedMemoryMessage candidate;
    candidate.caching_ = reader.read_u32();
    const uint32_t reserved = reader.read_u32();
    candidate.size_ = reader.read_u64();
    if (reserved != 0u || !candidate.fields_valid() || descriptor_count != candidate.descriptors())
        return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}  // namespace av
