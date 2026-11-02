#include "icd/d3d/row_band.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

RowBand::RowBand() : first_(0u), end_(0u)
{
    AV_SCOPE("RowBand::RowBand");
}

RowBand::RowBand(uint32_t first, uint32_t end) : first_(first), end_(end < first ? first : end)
{
    AV_SCOPE("RowBand::RowBand");
}

bool RowBand::empty() const
{
    AV_SCOPE("RowBand::empty");
    return end_ == first_;
}

uint32_t RowBand::first() const
{
    AV_SCOPE("RowBand::first");
    return first_;
}

uint32_t RowBand::end() const
{
    AV_SCOPE("RowBand::end");
    return end_;
}

RowBand RowBand::joined(const RowBand &other) const
{
    AV_SCOPE("RowBand::joined");
    if (other.empty()) return *this;
    if (empty()) return other;
    return RowBand(first_ < other.first_ ? first_ : other.first_, end_ > other.end_ ? end_ : other.end_);
}

}
