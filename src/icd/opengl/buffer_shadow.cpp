#include "icd/opengl/buffer_shadow.hpp"

#include "core/platform/block_source.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av::glicd {

BufferShadow::BufferShadow(BlockSource &memory)
    : memory_(memory), block_(nullptr), buffer_(0u), offset_(0u), length_(0u), stamp_(0u)
{
    AV_SCOPE("BufferShadow::BufferShadow");
}

BufferShadow::~BufferShadow()
{
    AV_SCOPE("BufferShadow::~BufferShadow");
    if (block_ != nullptr) memory_.release(block_);
}

Status BufferShadow::hold(uint32_t buffer, uint64_t offset, uint64_t length, const uint8_t *bytes, uint64_t stamp)
{
    AV_SCOPE("BufferShadow::hold");
    if (block_ != nullptr) return Status::invalid_state;
    if (buffer == 0u || length == 0u || bytes == nullptr) return Status::invalid_parameter;
    block_ = static_cast<uint8_t *>(memory_.allocate(static_cast<size_t>(length), Pool::paged));
    if (block_ == nullptr) return Status::no_memory;
    __builtin_memcpy(block_, bytes, static_cast<size_t>(length));
    buffer_ = buffer;
    offset_ = offset;
    length_ = length;
    stamp_ = stamp;
    return Status::ok;
}

uint32_t BufferShadow::buffer() const
{
    AV_SCOPE("BufferShadow::buffer");
    return buffer_;
}

uint64_t BufferShadow::length() const
{
    AV_SCOPE("BufferShadow::length");
    return length_;
}

uint64_t BufferShadow::stamp() const
{
    AV_SCOPE("BufferShadow::stamp");
    return stamp_;
}

bool BufferShadow::covers(uint32_t buffer, uint64_t offset, uint64_t length) const
{
    AV_SCOPE("BufferShadow::covers");
    return buffer == buffer_ && buffer_ != 0u && offset >= offset_ && offset - offset_ <= length_ &&
           length <= length_ - (offset - offset_);
}

uint8_t *BufferShadow::at(uint64_t offset, uint64_t stamp)
{
    AV_SCOPE("BufferShadow::at");
    stamp_ = stamp;
    return block_ + (offset - offset_);
}

void BufferShadow::overwrite(uint64_t offset, uint64_t length, const uint8_t *bytes)
{
    AV_SCOPE("BufferShadow::overwrite");
    const uint64_t end = offset + length;
    const uint64_t from = offset > offset_ ? offset : offset_;
    const uint64_t to = end < offset_ + length_ ? end : offset_ + length_;
    if (from >= to) return;
    __builtin_memcpy(block_ + (from - offset_), bytes + (from - offset), static_cast<size_t>(to - from));
}

void BufferShadow::report(StateWriter &writer, uint32_t index) const
{
    AV_SCOPE("BufferShadow::report");
    writer.open("shadow", index);
    writer.number("buffer", buffer_);
    writer.number("offset", offset_);
    writer.number("length", length_);
    writer.number("stamp", stamp_);
    writer.close();
}

bool BufferShadow::invariants_hold() const
{
    AV_SCOPE("BufferShadow::invariants_hold");
    return (block_ == nullptr) == (buffer_ == 0u) && (block_ == nullptr) == (length_ == 0u) &&
           offset_ + length_ >= offset_;
}

}
