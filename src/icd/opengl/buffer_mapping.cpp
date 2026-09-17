#include "icd/opengl/buffer_mapping.hpp"

#include "core/platform/block_source.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "proto/client_blob_arena.hpp"
#include "proto/client_transport.hpp"

#include "gl_dispatch_client.h"

namespace av::glicd {

BufferMapping::BufferMapping(BlockSource &memory)
    : memory_(memory), block_(nullptr), buffer_(0u), offset_(0u), length_(0u), access_(0u), dirty_from_(0u),
      dirty_to_(0u)
{
    AV_SCOPE("BufferMapping::BufferMapping");
}

BufferMapping::~BufferMapping()
{
    AV_SCOPE("BufferMapping::~BufferMapping");
    if (block_ != nullptr) memory_.release(block_);
}

Status BufferMapping::map(ClientTransport &wire, uint32_t buffer, uint64_t offset, uint64_t length, uint32_t access)
{
    AV_SCOPE("BufferMapping::map");
    if (block_ != nullptr) return Status::invalid_state;
    if (buffer == 0u || length == 0u || length > max_length) return Status::invalid_parameter;
    block_ = static_cast<uint8_t *>(memory_.allocate(static_cast<size_t>(length), Pool::paged));
    if (block_ == nullptr) return Status::no_memory;
    buffer_ = buffer;
    offset_ = offset;
    length_ = length;
    access_ = access;
    dirty_from_ = 0u;
    dirty_to_ = 0u;
    const bool fills = (access & map_read) != 0u && (access & (map_invalidate_range | map_invalidate_buffer)) == 0u;
    /* Memory the host does not fill, or answers short, is still the
     * application's to read: zeros, never what the block held before. */
    __builtin_memset(block_, 0, static_cast<size_t>(length));
    if (fills) read(wire);
    return Status::ok;
}

uint32_t BufferMapping::buffer() const
{
    AV_SCOPE("BufferMapping::buffer");
    return buffer_;
}

void *BufferMapping::pointer() const
{
    AV_SCOPE("BufferMapping::pointer");
    return block_;
}

uint64_t BufferMapping::offset() const
{
    AV_SCOPE("BufferMapping::offset");
    return offset_;
}

uint64_t BufferMapping::length() const
{
    AV_SCOPE("BufferMapping::length");
    return length_;
}

uint32_t BufferMapping::access() const
{
    AV_SCOPE("BufferMapping::access");
    return access_;
}

bool BufferMapping::persistent_writes() const
{
    AV_SCOPE("BufferMapping::persistent_writes");
    return (access_ & (map_persistent | map_write)) == (map_persistent | map_write);
}

Status BufferMapping::mark(uint64_t offset, uint64_t length)
{
    AV_SCOPE("BufferMapping::mark");
    if ((access_ & (map_write | map_flush_explicit)) != (map_write | map_flush_explicit))
        return Status::invalid_state;
    if (offset > length_ || length > length_ - offset) return Status::invalid_parameter;
    if (length == 0u) return Status::ok;
    const uint64_t end = offset + length;
    const bool     clean = dirty_from_ == dirty_to_;
    dirty_from_ = clean || offset < dirty_from_ ? offset : dirty_from_;
    dirty_to_ = clean || end > dirty_to_ ? end : dirty_to_;
    return Status::ok;
}

void BufferMapping::write_back(ClientTransport &wire)
{
    AV_SCOPE("BufferMapping::write_back");
    if ((access_ & map_write) == 0u) return;
    const bool     explicit_flush = (access_ & map_flush_explicit) != 0u;
    uint64_t       at = explicit_flush ? dirty_from_ : 0u;
    const uint64_t end = explicit_flush ? dirty_to_ : length_;
    dirty_from_ = 0u;
    dirty_to_ = 0u;
    while (at < end) {
        const size_t chunk = chunk_for(wire, end - at);
        glproto::rvk_glNamedBufferSubData(wire, buffer_, static_cast<GLintptr>(offset_ + at),
                                          static_cast<GLsizeiptr>(chunk), block_ + at);
        at += chunk;
    }
}

void BufferMapping::report(StateWriter &writer, uint32_t index) const
{
    AV_SCOPE("BufferMapping::report");
    writer.open("mapping", index);
    writer.number("buffer", buffer_);
    writer.number("offset", offset_);
    writer.number("length", length_);
    writer.hex("access", access_);
    writer.number("dirty_from", dirty_from_);
    writer.number("dirty_to", dirty_to_);
    writer.close();
}

/* A mapping holds its block exactly while it names a buffer, and what is
 * marked lies inside it, only on an explicitly flushed write map. */
bool BufferMapping::invariants_hold() const
{
    AV_SCOPE("BufferMapping::invariants_hold");
    if ((block_ == nullptr) != (buffer_ == 0u) || (block_ == nullptr) != (length_ == 0u)) return false;
    if (dirty_from_ > dirty_to_ || dirty_to_ > length_) return false;
    const bool marks = (access_ & (map_write | map_flush_explicit)) == (map_write | map_flush_explicit);
    return marks || dirty_to_ == 0u;
}

void BufferMapping::read(ClientTransport &wire)
{
    AV_SCOPE("BufferMapping::read");
    for (uint64_t at = 0u; at < length_; at += read_bytes) {
        const uint64_t left = length_ - at;
        const size_t   chunk = left < read_bytes ? static_cast<size_t>(left) : read_bytes;
        glproto::rvk_glGetNamedBufferSubData(wire, buffer_, static_cast<GLintptr>(offset_ + at),
                                             static_cast<GLsizeiptr>(chunk), block_ + at);
    }
}

size_t BufferMapping::chunk_for(ClientTransport &wire, uint64_t left) const
{
    AV_SCOPE("BufferMapping::chunk_for");
    ClientBlobArena &blobs = wire.blobs();
    const size_t     inline_chunk = left < inline_write_bytes ? static_cast<size_t>(left) : inline_write_bytes;
    if (!blobs.ready()) return inline_chunk;
    const size_t wanted = left < blob_write_bytes ? static_cast<size_t>(left) : blob_write_bytes;
    /* Any answered call retires every lease: the host has read them all. */
    if (blobs.room() < wanted) static_cast<void>(glproto::rvk_glIsBuffer(wire, buffer_));
    return blobs.room() >= wanted ? wanted : inline_chunk;
}

}  // namespace av::glicd
