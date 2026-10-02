#include "icd/d3d/pitched_rows.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

PitchedRows::PitchedRows(uint8_t *base, uint32_t pitch, size_t bytes)
    : base_(base), pitch_(pitch), bytes_(base == nullptr ? 0u : bytes)
{
    AV_SCOPE("PitchedRows::PitchedRows");
}

bool PitchedRows::holds(uint32_t row) const
{
    AV_SCOPE("PitchedRows::holds");
    return pitch_ != 0u && (uint64_t{row} + 1u) * pitch_ <= bytes_;
}

uint32_t PitchedRows::copy_from(const PitchedRows &from, uint32_t rows)
{
    AV_SCOPE("PitchedRows::copy_from");
    const uint32_t line = from.pitch_ < pitch_ ? from.pitch_ : pitch_;
    uint32_t       row = 0u;
    for (; row < rows && holds(row) && from.holds(row); row++)
        __builtin_memcpy(base_ + size_t{row} * pitch_, from.base_ + size_t{row} * from.pitch_, line);
    return row;
}

}  // namespace av::d3d
