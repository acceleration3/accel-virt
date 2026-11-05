#include "proto/client/blob_ring.hpp"

#include "core/trace/function_scope.hpp"

namespace av {


BlobRing::BlobRing()
    : block_(nullptr), lent_(0u), base_(0u), top_(0u), end_(0u), peak_ring_(0u), wrapped_(false), lap_(0u),
      base_lap_(0u), sealed_(0u), sealed_lap_(0u), marks_{}, first_mark_(0u), mark_count_(0u)
{
    AV_SCOPE("BlobRing::BlobRing");
}

BlobRing::BlobRing(uint8_t *block, size_t capacity) : BlobRing()
{
    AV_SCOPE("BlobRing::BlobRing");
    lend(block, capacity);
}

void BlobRing::lend(uint8_t *block, size_t capacity)
{
    AV_SCOPE("BlobRing::lend");
    block_ = block;
    lent_ = block == nullptr ? 0u : capacity;
        peak_ring_ = 0u;
    empty();
}

bool BlobRing::ready() const
{
    AV_SCOPE("BlobRing::ready");
    return block_ != nullptr;
}

ClientBlobLease BlobRing::take(size_t count)
{
    AV_SCOPE("BlobRing::take");
    if (count == 0u || block_ == nullptr) return ClientBlobLease{};
    if (wrapped_) {
        if (count > base_ - top_) return ClientBlobLease{};
    } else if (count > lent_ - top_) {
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

void BlobRing::give_back(const ClientBlobLease &lease)
{
    AV_SCOPE("BlobRing::give_back");
    if (!lease.taken() || lease.offset() + lease.bytes() != top_ || (sealed_ == top_ && sealed_lap_ == lap_)) return;
    top_ = static_cast<size_t>(lease.offset());
    if (wrapped_ && top_ == 0u) {
        top_ = end_;
        wrapped_ = false;
        lap_--;
    }
}

void BlobRing::empty()
{
    AV_SCOPE("BlobRing::empty");
    base_ = top_ = end_ = sealed_ = 0u;
    wrapped_ = false;
    base_lap_ = sealed_lap_ = lap_;
    first_mark_ = mark_count_ = 0u;
}

void BlobRing::seal()
{
    AV_SCOPE("BlobRing::seal");
    sealed_ = top_;
    sealed_lap_ = lap_;
}

void BlobRing::mark(uint64_t fence)
{
    AV_SCOPE("BlobRing::mark");
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

void BlobRing::retire_through(uint64_t fence)
{
    AV_SCOPE("BlobRing::retire_through");
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

bool BlobRing::holds(const void *bytes, size_t count, uint64_t &offset) const
{
    AV_SCOPE("BlobRing::holds");
    if (block_ == nullptr || count == 0u) return false;
    const uintptr_t into = reinterpret_cast<uintptr_t>(bytes) - reinterpret_cast<uintptr_t>(block_);
    if (into > lent_ || count > lent_ - into) return false;
    offset = static_cast<uint64_t>(into);
    return true;
}

size_t BlobRing::used() const
{
    AV_SCOPE("BlobRing::used");
    return wrapped_ ? end_ - base_ + top_ : top_ - base_;
}

size_t BlobRing::room() const
{
    AV_SCOPE("BlobRing::room");
    if (block_ == nullptr) return 0u;
    if (wrapped_) return base_ - top_;
    const size_t above = lent_ - top_;
    return above > base_ ? above : base_;
}

size_t BlobRing::lent() const
{
    AV_SCOPE("BlobRing::lent");
    return lent_;
}

bool BlobRing::idle() const
{
    AV_SCOPE("BlobRing::idle");
    return block_ != nullptr && used() == 0u;
}

size_t BlobRing::peak_ring() const
{
    AV_SCOPE("BlobRing::peak_ring");
    return peak_ring_;
}

bool BlobRing::invariants_hold() const
{
    AV_SCOPE("BlobRing::invariants_hold");
    const bool laid = wrapped_ ? top_ <= base_ && base_ <= end_ && end_ <= lent_
                               : base_ <= top_ && top_ <= lent_;
    const bool sealed = sealed_lap_ == base_lap_ ? base_ <= sealed_ && sealed_ <= (wrapped_ ? end_ : top_)
                                                 : wrapped_ && sealed_lap_ == lap_ && sealed_ <= top_;
    return laid && sealed && (block_ != nullptr || lent_ == 0u) && peak_ring_ <= lent_ && mark_count_ <= max_marks &&
           first_mark_ < max_marks && (wrapped_ ? lap_ == base_lap_ + 1u : lap_ == base_lap_);
}

}
