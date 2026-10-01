/* Ranges of a region placed first fit, kept sorted by offset so a gap is what
 * lies between two of them: the suballocator under OpenCL's arena-backed
 * buffers (ClArenaBlocks), Direct3D 12's CPU-visible heaps (D3d12HeapArena)
 * and Vulkan's host-visible chunk (ChunkSpace). Each range is a whole number
 * of the alignment and starts on it; a range given back leaves a gap the next
 * one that fits reuses.
 *
 * The table takes no lock: an owner placing from several threads holds its
 * own around every call, as D3d12HeapArena does. */
#ifndef AV_ICD_RANGE_TABLE_HPP
#define AV_ICD_RANGE_TABLE_HPP

#include "core/support/alignment.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"

#include <stdint.h>

namespace av::icd {

struct RangeTableProbe;

template <uint32_t Max>
class RangeTable {
public:
    static constexpr uint32_t capacity = Max;

    /* `alignment` is a power of two. */
    explicit RangeTable(uint64_t alignment);
    RangeTable(const RangeTable &) = delete;
    RangeTable &operator=(const RangeTable &) = delete;

    /* Places in [begin, end) from now on, both rounded inward to the
     * alignment, forgetting every range placed before. */
    void                 reset(uint64_t begin, uint64_t end);
    /* The offset of a range of `bytes` rounded up to the alignment, `offset`
     * untouched unless ok. invalid_parameter for no bytes or a size that does
     * not round; no_space when every row is used or no gap is large enough. */
    [[nodiscard]] Status place(uint64_t bytes, uint64_t &offset);
    /* The range at `offset` gives its bytes back; invalid_parameter for an
     * offset no range starts at. */
    [[nodiscard]] Status remove(uint64_t offset);
    /* The bytes of the range starting at `offset`; 0 for none. */
    uint64_t             size_at(uint64_t offset) const;

    uint32_t placed() const;
    uint64_t bytes_placed() const;
    uint64_t begin() const;
    uint64_t end() const;
    uint64_t alignment() const;

    /* Ranges of at least the alignment, aligned, sorted, apart and inside the
     * region; rows past the count empty, so no range outlives its removal. */
    bool invariants_hold() const;

private:
    friend struct RangeTableProbe;

    /* The first gap of `bytes` and the row the range placed there takes;
     * false when none. */
    bool gap(uint64_t bytes, uint64_t &offset, uint32_t &row) const;

    Alignment alignment_;
    uint64_t  begin_;
    uint64_t  end_;
    uint64_t  offsets_[Max];
    uint64_t  sizes_[Max];
    uint32_t  count_;
};

template <uint32_t Max>
RangeTable<Max>::RangeTable(uint64_t alignment)
    : alignment_(alignment), begin_(0u), end_(0u), offsets_{}, sizes_{}, count_(0u)
{
    AV_SCOPE("RangeTable::RangeTable");
}

template <uint32_t Max>
void RangeTable<Max>::reset(uint64_t begin, uint64_t end)
{
    AV_SCOPE("RangeTable::reset");
    const uint64_t mask = alignment_.value() - 1u;
    if (!alignment_.align_up(begin, begin_)) begin_ = end & ~mask;
    end_ = end & ~mask;
    if (end_ < begin_) end_ = begin_;
    for (uint32_t row = 0u; row < count_; row++) offsets_[row] = sizes_[row] = 0u;
    count_ = 0u;
}

template <uint32_t Max>
bool RangeTable<Max>::gap(uint64_t bytes, uint64_t &offset, uint32_t &row) const
{
    AV_SCOPE("RangeTable::gap");
    uint64_t at = begin_;
    for (row = 0u; row < count_; row++) {
        if (offsets_[row] - at >= bytes) break;
        at = offsets_[row] + sizes_[row];
    }
    if (end_ - at < bytes) return false;
    offset = at;
    return true;
}

template <uint32_t Max>
Status RangeTable<Max>::place(uint64_t bytes, uint64_t &offset)
{
    AV_SCOPE("RangeTable::place");
    uint64_t rounded = 0u;
    if (bytes == 0u || !alignment_.align_up(bytes, rounded)) return Status::invalid_parameter;
    uint64_t at = 0u;
    uint32_t row = 0u;
    if (count_ == Max || !gap(rounded, at, row)) return Status::no_space;
    for (uint32_t moved = count_; moved > row; moved--) {
        offsets_[moved] = offsets_[moved - 1u];
        sizes_[moved] = sizes_[moved - 1u];
    }
    offsets_[row] = at;
    sizes_[row] = rounded;
    count_++;
    offset = at;
    return Status::ok;
}

template <uint32_t Max>
Status RangeTable<Max>::remove(uint64_t offset)
{
    AV_SCOPE("RangeTable::remove");
    uint32_t row = 0u;
    while (row < count_ && offsets_[row] != offset) row++;
    if (row == count_) return Status::invalid_parameter;
    for (; row + 1u < count_; row++) {
        offsets_[row] = offsets_[row + 1u];
        sizes_[row] = sizes_[row + 1u];
    }
    count_--;
    offsets_[count_] = sizes_[count_] = 0u;
    return Status::ok;
}

template <uint32_t Max>
uint64_t RangeTable<Max>::size_at(uint64_t offset) const
{
    AV_SCOPE("RangeTable::size_at");
    for (uint32_t row = 0u; row < count_; row++)
        if (offsets_[row] == offset) return sizes_[row];
    return 0u;
}

template <uint32_t Max>
uint32_t RangeTable<Max>::placed() const
{
    AV_SCOPE("RangeTable::placed");
    return count_;
}

template <uint32_t Max>
uint64_t RangeTable<Max>::bytes_placed() const
{
    AV_SCOPE("RangeTable::bytes_placed");
    uint64_t total = 0u;
    for (uint32_t row = 0u; row < count_; row++) total += sizes_[row];
    return total;
}

template <uint32_t Max>
uint64_t RangeTable<Max>::begin() const
{
    AV_SCOPE("RangeTable::begin");
    return begin_;
}

template <uint32_t Max>
uint64_t RangeTable<Max>::end() const
{
    AV_SCOPE("RangeTable::end");
    return end_;
}

template <uint32_t Max>
uint64_t RangeTable<Max>::alignment() const
{
    AV_SCOPE("RangeTable::alignment");
    return alignment_.value();
}

template <uint32_t Max>
bool RangeTable<Max>::invariants_hold() const
{
    AV_SCOPE("RangeTable::invariants_hold");
    if (!alignment_.is_valid() || count_ > Max || end_ < begin_) return false;
    if (!alignment_.divides(begin_) || !alignment_.divides(end_)) return false;
    uint64_t at = begin_;
    for (uint32_t row = 0u; row < count_; row++) {
        const uint64_t offset = offsets_[row], size = sizes_[row];
        if (size == 0u || offset < at || !alignment_.divides(offset) || !alignment_.divides(size)) return false;
        if (offset > end_ || size > end_ - offset) return false;
        at = offset + size;
    }
    for (uint32_t row = count_; row < Max; row++)
        if (offsets_[row] != 0u || sizes_[row] != 0u) return false;
    return true;
}

}  // namespace av::icd

#endif  // AV_ICD_RANGE_TABLE_HPP
