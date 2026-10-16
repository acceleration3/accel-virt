#include "proto/resources/pitched_extent.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

PitchedExtent::PitchedExtent(uint64_t line, uint64_t rows, uint64_t slices, uint64_t row_pitch,
                             uint64_t slice_pitch)
    : line_(line), rows_(rows), slices_(slices), row_pitch_(row_pitch), slice_pitch_(slice_pitch)
{
    AV_SCOPE("PitchedExtent::PitchedExtent");
}

bool PitchedExtent::empty() const
{
    AV_SCOPE("PitchedExtent::empty");
    return line_ == 0u || rows_ == 0u || slices_ == 0u;
}

bool PitchedExtent::pitches_hold() const
{
    AV_SCOPE("PitchedExtent::pitches_hold");
    if (empty()) return false;
    if (rows_ > 1u && row_pitch_ < line_) return false;
    if (slices_ <= 1u) return true;
    uint64_t slice = 0u;
    if (__builtin_mul_overflow(rows_ - 1u, row_pitch_, &slice) || __builtin_add_overflow(slice, line_, &slice))
        return false;
    return slice_pitch_ >= slice;
}

bool PitchedExtent::offset_of(uint64_t row, uint64_t slice, uint64_t &offset) const
{
    AV_SCOPE("PitchedExtent::offset_of");
    uint64_t down = 0u, across = 0u;
    return !__builtin_mul_overflow(slice, slice_pitch_, &down) && !__builtin_mul_overflow(row, row_pitch_, &across) &&
           !__builtin_add_overflow(down, across, &offset);
}

bool PitchedExtent::end(uint64_t start, uint64_t &end) const
{
    AV_SCOPE("PitchedExtent::end");
    uint64_t last = 0u;
    if (empty() || !offset_of(rows_ - 1u, slices_ - 1u, last)) return false;
    return !__builtin_add_overflow(start, last, &end) && !__builtin_add_overflow(end, line_, &end);
}

bool PitchedExtent::within(uint64_t size) const
{
    AV_SCOPE("PitchedExtent::within");
    uint64_t reach = 0u;
    return end(0u, reach) && reach <= size;
}

bool PitchedExtent::fits(uint64_t size) const
{
    AV_SCOPE("PitchedExtent::fits");
    return pitches_hold() && within(size);
}

}  // namespace av
