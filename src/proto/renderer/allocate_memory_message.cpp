#include "proto/renderer/allocate_memory_message.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

AllocateMemoryMessage::AllocateMemoryMessage() : context_(0u), request_(0u), key_(0u), size_(0u)
{
    AV_SCOPE("AllocateMemoryMessage::AllocateMemoryMessage");
}

AllocateMemoryMessage::AllocateMemoryMessage(uint32_t context, uint32_t request, uint64_t key, uint64_t size)
    : context_(context), request_(request), key_(key), size_(size)
{
    AV_SCOPE("AllocateMemoryMessage::AllocateMemoryMessage");
}

uint32_t AllocateMemoryMessage::context() const
{
    AV_SCOPE("AllocateMemoryMessage::context");
    return context_;
}

uint32_t AllocateMemoryMessage::request() const
{
    AV_SCOPE("AllocateMemoryMessage::request");
    return request_;
}

uint64_t AllocateMemoryMessage::key() const
{
    AV_SCOPE("AllocateMemoryMessage::key");
    return key_;
}

uint64_t AllocateMemoryMessage::size() const
{
    AV_SCOPE("AllocateMemoryMessage::size");
    return size_;
}

bool AllocateMemoryMessage::fields_valid() const
{
    AV_SCOPE("AllocateMemoryMessage::fields_valid");
    return context_ != 0u && key_ != 0u && size_ != 0u;
}

Status AllocateMemoryMessage::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("AllocateMemoryMessage::encode");
    if (bytes == nullptr || capacity < encoded_size) return Status::too_large;
    if (!fields_valid()) return Status::invalid_parameter;
    WireWriter writer(bytes, capacity);
    writer.write_u32(context_);
    writer.write_u32(request_);
    writer.write_u64(key_);
    writer.write_u64(size_);
    return writer.status();
}

Status AllocateMemoryMessage::decode(const uint8_t *bytes, size_t length, uint32_t descriptor_count)
{
    AV_SCOPE("AllocateMemoryMessage::decode");
    if (bytes == nullptr || length != encoded_size || descriptor_count != 0u) return Status::malformed;
    WireReader            reader(bytes, length);
    AllocateMemoryMessage candidate;
    candidate.context_ = reader.read_u32();
    candidate.request_ = reader.read_u32();
    candidate.key_ = reader.read_u64();
    candidate.size_ = reader.read_u64();
    if (!candidate.fields_valid()) return Status::malformed;
    *this = candidate;
    return Status::ok;
}

}
