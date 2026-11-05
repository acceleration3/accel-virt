#include "proto/client/client_blob_arena.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ClientBlobLease::ClientBlobLease() : data_(nullptr), offset_(0u), bytes_(0u)
{
    AV_SCOPE("ClientBlobLease::ClientBlobLease");
}

ClientBlobLease::ClientBlobLease(void *data, uint64_t offset, size_t bytes)
    : data_(data), offset_(offset), bytes_(bytes)
    {
        AV_SCOPE("ClientBlobLease::ClientBlobLease");
    }

bool     ClientBlobLease::taken() const
{
    AV_SCOPE("ClientBlobLease::taken");
    return data_ != nullptr;
}
void    *ClientBlobLease::data() const
{
    AV_SCOPE("ClientBlobLease::data");
    return data_;
}
uint64_t ClientBlobLease::offset() const
{
    AV_SCOPE("ClientBlobLease::offset");
    return offset_;
}
size_t   ClientBlobLease::bytes() const
{
    AV_SCOPE("ClientBlobLease::bytes");
    return bytes_;
}

ClientBlobArena::ClientBlobArena()
    : block_(nullptr), lent_(0u), capacity_(0u), base_(0u), top_(0u), end_(0u), peak_ring_(0u), wrapped_(false), lap_(0u),
      base_lap_(0u), sealed_(0u), sealed_lap_(0u), marks_{}, first_mark_(0u), mark_count_(0u)
{
    AV_SCOPE("ClientBlobArena::ClientBlobArena");
}

ClientBlobArena::ClientBlobArena(uint8_t *block, size_t capacity) : ClientBlobArena()
{
    AV_SCOPE("ClientBlobArena::ClientBlobArena");
    lend(block, capacity);
}

void ClientBlobArena::lend(uint8_t *block, size_t capacity)
{
    AV_SCOPE("ClientBlobArena::lend");
    block_ = block;
    lent_ = block == nullptr ? 0u : capacity;
    capacity_ = lent_;
    peak_ring_ = 0u;
    empty();
}

bool ClientBlobArena::ready() const
{
    AV_SCOPE("ClientBlobArena::ready");
    return block_ != nullptr;
}

ClientBlobLease ClientBlobArena::take(size_t count)
{
    AV_SCOPE("ClientBlobArena::take");
    if (count == 0u || block_ == nullptr) return ClientBlobLease{};
    if (wrapped_) {
        if (count > base_ - top_) return ClientBlobLease{};
    } else if (count > capacity_ - top_) {
        if (count > base_) return ClientBlobLease{};
        end_ = top_;
        top_ = 0u;
        wrapped_ = true;
        lap_++;
    }
    const size_t at = top_;
    top_ += count;
    const size_t in_ring = wrapped_ ? end_ - base_ + top_ : top_ - base_;
    if (in_ring > peak_ring_) peak_ring_ = in_ring;
    return ClientBlobLease(block_ + at, at, count);
}

void ClientBlobArena::give_back(const ClientBlobLease &lease)
{
    AV_SCOPE("ClientBlobArena::give_back");
    if (!lease.taken() || lease.offset() + lease.bytes() != top_ || (sealed_ == top_ && sealed_lap_ == lap_)) return;
    top_ = static_cast<size_t>(lease.offset());
    if (wrapped_ && top_ == 0u) {
        top_ = end_;
        wrapped_ = false;
        lap_--;
    }
}

void ClientBlobArena::empty()
{
    AV_SCOPE("ClientBlobArena::empty");
    base_ = top_ = end_ = sealed_ = 0u;
    wrapped_ = false;
    base_lap_ = sealed_lap_ = lap_;
    first_mark_ = mark_count_ = 0u;
}

void ClientBlobArena::seal()
{
    AV_SCOPE("ClientBlobArena::seal");
    sealed_ = top_;
    sealed_lap_ = lap_;
}

void ClientBlobArena::mark(uint64_t fence)
{
    AV_SCOPE("ClientBlobArena::mark");
    if (mark_count_ != 0u) {
        Mark &newest = marks_[(first_mark_ + mark_count_ - 1u) % max_marks];
        if ((newest.top_ == sealed_ && newest.lap_ == sealed_lap_) || mark_count_ == max_marks) {
            newest = Mark{fence, sealed_, sealed_lap_};
            return;
        }
    }
    marks_[(first_mark_ + mark_count_) % max_marks] = Mark{fence, sealed_, sealed_lap_};
    mark_count_++;
}

void ClientBlobArena::retire_through(uint64_t fence)
{
    AV_SCOPE("ClientBlobArena::retire_through");
    while (mark_count_ != 0u && marks_[first_mark_].fence_ <= fence) {
        const Mark &done = marks_[first_mark_];
        if (done.lap_ != base_lap_) {
            wrapped_ = false;
            base_lap_ = done.lap_;
        }
        base_ = done.top_;
        first_mark_ = (first_mark_ + 1u) % max_marks;
        mark_count_--;
    }
    if (!wrapped_ && base_ == top_) empty();
}

ClientBlobLease ClientBlobArena::set_aside(size_t count)
{
    AV_SCOPE("ClientBlobArena::set_aside");
    const size_t highest = wrapped_ ? end_ : top_;
    if (count == 0u || block_ == nullptr || count > capacity_ - highest) return ClientBlobLease{};
    capacity_ -= count;
    return ClientBlobLease(block_ + capacity_, capacity_, count);
}

void ClientBlobArena::restore(const ClientBlobLease &aside)
{
    AV_SCOPE("ClientBlobArena::restore");
    if (aside.taken() && aside.offset() == capacity_) capacity_ += aside.bytes();
}

bool ClientBlobArena::holds(const void *bytes, size_t count, uint64_t &offset) const
{
    AV_SCOPE("ClientBlobArena::holds");
    if (block_ == nullptr || count == 0u) return false;
    const uintptr_t into = reinterpret_cast<uintptr_t>(bytes) - reinterpret_cast<uintptr_t>(block_);
    if (into > lent_ || count > lent_ - into) return false;
    offset = static_cast<uint64_t>(into);
    return true;
}

size_t ClientBlobArena::used() const
{
    AV_SCOPE("ClientBlobArena::used");
    return wrapped_ ? end_ - base_ + top_ : top_ - base_;
}

size_t ClientBlobArena::room() const
{
    AV_SCOPE("ClientBlobArena::room");
    if (block_ == nullptr) return 0u;
    if (wrapped_) return base_ - top_;
    const size_t above = capacity_ - top_;
    return above > base_ ? above : base_;
}

size_t ClientBlobArena::lent() const
{
    AV_SCOPE("ClientBlobArena::lent");
    return lent_;
}

size_t ClientBlobArena::peak_ring() const
{
    AV_SCOPE("ClientBlobArena::peak_ring");
    return peak_ring_;
}

size_t ClientBlobArena::set_aside_bytes() const
{
    AV_SCOPE("ClientBlobArena::set_aside_bytes");
    return lent_ - capacity_;
}

bool ClientBlobArena::invariants_hold() const
{
    AV_SCOPE("ClientBlobArena::invariants_hold");
    const bool laid = wrapped_ ? top_ <= base_ && base_ <= end_ && end_ <= capacity_
                               : base_ <= top_ && top_ <= capacity_;
    const bool sealed = sealed_lap_ == base_lap_ ? base_ <= sealed_ && sealed_ <= (wrapped_ ? end_ : top_)
                                                 : wrapped_ && sealed_lap_ == lap_ && sealed_ <= top_;
    return laid && sealed && capacity_ <= lent_ && (block_ != nullptr || lent_ == 0u) && peak_ring_ <= lent_ && mark_count_ <= max_marks &&
           first_mark_ < max_marks && (wrapped_ ? lap_ == base_lap_ + 1u : lap_ == base_lap_);
}

}
