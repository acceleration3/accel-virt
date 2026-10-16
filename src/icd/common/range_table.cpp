#include "icd/common/range_table.hpp"

#include "core/trace/function_scope.hpp"

namespace av::icd {

RangeTable::RangeTable(BlockSource &memory, uint64_t alignment)
    : alignment_(alignment), begin_(0u), end_(0u), ranges_(memory, Pool::paged)
{
    AV_SCOPE("RangeTable::RangeTable");
}

void RangeTable::reset(uint64_t begin, uint64_t end)
{
    AV_SCOPE("RangeTable::reset");
    const uint64_t mask = alignment_.value() - 1u;
    if (!alignment_.align_up(begin, begin_)) begin_ = end & ~mask;
    end_ = end & ~mask;
    if (end_ < begin_) end_ = begin_;
    ranges_.clear();
}

bool RangeTable::gap(uint64_t bytes, uint64_t &offset, uint32_t &row) const
{
    AV_SCOPE("RangeTable::gap");
    uint64_t at = begin_;
    for (row = 0u; row < ranges_.size(); row++) {
        if (ranges_.at(row).offset_ - at >= bytes) break;
        at = ranges_.at(row).offset_ + ranges_.at(row).size_;
    }
    if (end_ - at < bytes) return false;
    offset = at;
    return true;
}

uint32_t RangeTable::row_at_or_after(uint64_t offset) const
{
    AV_SCOPE("RangeTable::row_at_or_after");
    uint32_t low = 0u;
    uint32_t high = ranges_.size();
    while (low < high) {
        const uint32_t middle = low + (high - low) / 2u;
        if (ranges_.at(middle).offset_ < offset) low = middle + 1u;
        else high = middle;
    }
    return low;
}

Status RangeTable::place(uint64_t bytes, uint64_t &offset)
{
    AV_SCOPE("RangeTable::place");
    uint64_t rounded = 0u;
    if (bytes == 0u || !alignment_.align_up(bytes, rounded)) return Status::invalid_parameter;
    uint64_t at = 0u;
    uint32_t row = 0u;
    if (!gap(rounded, at, row)) return Status::no_space;
    const Status inserted = ranges_.insert_at(row, Range{at, rounded});
    if (inserted != Status::ok) return inserted;
    offset = at;
    return Status::ok;
}

Status RangeTable::remove(uint64_t offset)
{
    AV_SCOPE("RangeTable::remove");
    const uint32_t row = row_at_or_after(offset);
    if (row == ranges_.size() || ranges_.at(row).offset_ != offset) return Status::invalid_parameter;
    ranges_.remove_at(row);
    return Status::ok;
}

uint64_t RangeTable::size_at(uint64_t offset) const
{
    AV_SCOPE("RangeTable::size_at");
    const uint32_t row = row_at_or_after(offset);
    return row != ranges_.size() && ranges_.at(row).offset_ == offset ? ranges_.at(row).size_ : 0u;
}

uint32_t RangeTable::placed() const
{
    AV_SCOPE("RangeTable::placed");
    return ranges_.size();
}

uint64_t RangeTable::bytes_placed() const
{
    AV_SCOPE("RangeTable::bytes_placed");
    uint64_t total = 0u;
    for (const Range &range : ranges_) total += range.size_;
    return total;
}

uint64_t RangeTable::begin() const
{
    AV_SCOPE("RangeTable::begin");
    return begin_;
}

uint64_t RangeTable::end() const
{
    AV_SCOPE("RangeTable::end");
    return end_;
}

uint64_t RangeTable::alignment() const
{
    AV_SCOPE("RangeTable::alignment");
    return alignment_.value();
}

bool RangeTable::invariants_hold() const
{
    AV_SCOPE("RangeTable::invariants_hold");
    if (!alignment_.is_valid() || end_ < begin_) return false;
    if (!alignment_.divides(begin_) || !alignment_.divides(end_)) return false;
    uint64_t at = begin_;
    for (const Range &range : ranges_) {
        if (range.size_ == 0u || range.offset_ < at || !alignment_.divides(range.offset_) ||
            !alignment_.divides(range.size_))
            return false;
        if (range.offset_ > end_ || range.size_ > end_ - range.offset_) return false;
        at = range.offset_ + range.size_;
    }
    return true;
}

}  // namespace av::icd
