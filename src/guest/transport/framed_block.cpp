#include "guest/transport/framed_block.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/submission/command_pieces.hpp"

namespace av {

FramedBlock::FramedBlock(uint8_t *block, size_t capacity)
    : block_(block), capacity_(block == nullptr ? 0u : capacity), used_(0u)
{
    AV_SCOPE("FramedBlock::FramedBlock");
}

bool FramedBlock::fits(size_t bytes) const
{
    AV_SCOPE("FramedBlock::fits");
    return capacity_ - used_ >= bytes;
}

WireWriter FramedBlock::lend(size_t room)
{
    AV_SCOPE("FramedBlock::lend");
    const size_t left = capacity_ - used_;
    if (left < header_bytes) return WireWriter(nullptr, 0u);
    return WireWriter(block_ + used_ + header_bytes, left - header_bytes < room ? left - header_bytes : room);
}

WireWriter FramedBlock::lend(size_t room, WireSpill &spill)
{
    AV_SCOPE("FramedBlock::lend");
    const size_t left = capacity_ - used_;
    if (left < header_bytes) return WireWriter(nullptr, 0u);
    return WireWriter(block_ + used_ + header_bytes, left - header_bytes < room ? left - header_bytes : room, spill);
}

bool FramedBlock::frame_piece(CommandPieces &pieces)
{
    AV_SCOPE("FramedBlock::frame_piece");
    const size_t left = capacity_ - used_;
    const bool   whole = left >= header_bytes && pieces.rest() <= left - header_bytes;
    if (used_ != 0u && !whole && left < header_bytes + CommandPieces::least_piece) return false;
    const size_t framed = pieces.frame_next(block_ + used_, left);
    used_ += framed;
    return framed != 0u;
}

bool FramedBlock::frame(uint32_t command_id, const WireWriter &writer, uint8_t flags)
{
    AV_SCOPE("FramedBlock::frame");
    const size_t size = header_bytes + writer.offset();
    if (writer.status() != Status::ok || !fits(size)) return false;
    /* The room lent is far below the largest size a header carries. */
    static_cast<void>(CommandHeader(command_id, static_cast<uint32_t>(size), flags).encode(block_ + used_, header_bytes));
    used_ += size;
    return true;
}

bool FramedBlock::append(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("FramedBlock::append");
    if (!fits(length)) return false;
    if (length != 0u) __builtin_memcpy(block_ + used_, bytes, length);
    used_ += length;
    return true;
}

const uint8_t *FramedBlock::bytes() const
{
    AV_SCOPE("FramedBlock::bytes");
    return block_;
}

uint8_t *FramedBlock::bytes()
{
    AV_SCOPE("FramedBlock::bytes");
    return block_;
}

size_t FramedBlock::used() const
{
    AV_SCOPE("FramedBlock::used");
    return used_;
}

size_t FramedBlock::capacity() const
{
    AV_SCOPE("FramedBlock::capacity");
    return capacity_;
}

void FramedBlock::clear()
{
    AV_SCOPE("FramedBlock::clear");
    used_ = 0u;
}

void FramedBlock::adopt(uint8_t *block, size_t capacity)
{
    AV_SCOPE("FramedBlock::adopt");
    block_ = block;
    capacity_ = block == nullptr ? 0u : capacity;
}

bool FramedBlock::invariants_hold() const
{
    AV_SCOPE("FramedBlock::invariants_hold");
    return used_ <= capacity_ && (block_ != nullptr || capacity_ == 0u);
}

}  // namespace av
