#include "icd/opengl/buffer_mapping.hpp"

#include "core/platform/block_source.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/common/pieces.hpp"
#include "icd/opengl/buffer_shadows.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/client/transfer_room.hpp"

#include "gl_dispatch_client.h"

namespace av::glicd {

BufferMapping::BufferMapping(BlockSource &memory, BufferShadows &shadows)
    : memory_(memory), shadows_(shadows), block_(nullptr), buffer_(0u), offset_(0u), length_(0u), access_(0u),
      dirty_from_(0u), dirty_to_(0u)
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
    /* Memory nothing fills, or the host answers short, is still the
     * application's to read: zeros, never what the block held before. */
    __builtin_memset(block_, 0, static_cast<size_t>(length));
    start(wire);
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
    if ((access_ & map_flush_explicit) != 0u) {
        const uint64_t from = dirty_from_;
        const uint64_t to = dirty_to_;
        dirty_from_ = 0u;
        dirty_to_ = 0u;
        send(wire, from, to);
        shadows_.wrote(buffer_, offset_ + from, to - from, block_ + from);
        return;
    }
    uint8_t *const known = shadows_.covering(buffer_, offset_, length_);
    if (known != nullptr) {
        send_changes(wire, known);
        return;
    }
    send(wire, 0u, length_);
    shadows_.keep(buffer_, offset_, length_, block_);
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

void BufferMapping::start(ClientTransport &wire)
{
    AV_SCOPE("BufferMapping::start");
    if ((access_ & map_read) != 0u) {
        read(wire);
        if ((access_ & map_write) != 0u) shadows_.keep(buffer_, offset_, length_, block_);
        return;
    }
    const uint8_t *const known = shadows_.covering(buffer_, offset_, length_);
    if (known != nullptr) {
        __builtin_memcpy(block_, known, static_cast<size_t>(length_));
        return;
    }
    const uint32_t keeps_nothing = map_invalidate_range | map_invalidate_buffer | map_flush_explicit;
    if ((access_ & keeps_nothing) != 0u || shadows_.fresh(buffer_)) return;
    read(wire);
    shadows_.keep(buffer_, offset_, length_, block_);
}

void BufferMapping::read(ClientTransport &wire)
{
    AV_SCOPE("BufferMapping::read");
    icd::Pieces pieces(length_, wire.room().inline_read());
    while (pieces.next())
        glproto::av_glGetNamedBufferSubData(wire, buffer_, static_cast<GLintptr>(offset_ + pieces.offset()),
                                             static_cast<GLsizeiptr>(pieces.count()), block_ + pieces.offset());
}

void BufferMapping::send(ClientTransport &wire, uint64_t at, uint64_t end)
{
    AV_SCOPE("BufferMapping::send");
    while (at < end) {
        const size_t chunk = wire.room().write_piece(wire, end - at);
        glproto::av_glNamedBufferSubData(wire, buffer_, static_cast<GLintptr>(offset_ + at),
                                          static_cast<GLsizeiptr>(chunk), block_ + at);
        at += chunk;
    }
}

void BufferMapping::send_changes(ClientTransport &wire, uint8_t *known)
{
    AV_SCOPE("BufferMapping::send_changes");
    uint64_t from = next_change(known, 0u);
    while (from < length_) {
        const uint64_t to = change_end(known, from);
        send(wire, from, to);
        __builtin_memcpy(known + from, block_ + from, static_cast<size_t>(to - from));
        from = next_change(known, to);
    }
}

/* Word by word, each compare a load apiece: a scan of a 256 KiB staging
 * range runs once per unmap, and calls per word would cost more than the
 * bytes it saves. */
uint64_t BufferMapping::next_change(const uint8_t *known, uint64_t at) const
{
    AV_SCOPE("BufferMapping::next_change");
    for (; at + word <= length_; at += word)
        if (__builtin_memcmp(block_ + at, known + at, word) != 0) return at;
    const bool tail_differs =
        at < length_ && __builtin_memcmp(block_ + at, known + at, static_cast<size_t>(length_ - at)) != 0;
    return tail_differs ? at : length_;
}

uint64_t BufferMapping::change_end(const uint8_t *known, uint64_t from) const
{
    AV_SCOPE("BufferMapping::change_end");
    uint64_t end = from + word < length_ ? from + word : length_;
    uint64_t at = end;
    for (; at + word <= length_ && at - end < merge_gap; at += word)
        if (__builtin_memcmp(block_ + at, known + at, word) != 0) end = at + word;
    if (at < length_ && at - end < merge_gap &&
        __builtin_memcmp(block_ + at, known + at, static_cast<size_t>(length_ - at)) != 0)
        end = length_;
    return end;
}

}  // namespace av::glicd
