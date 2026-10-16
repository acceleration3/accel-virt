#include "proto/resources/pitched_rows.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

PitchedRows::PitchedRows(uint8_t *base, uint64_t pitch, uint64_t bytes)
    : base_(base), source_(base), pitch_(pitch), bytes_(base == nullptr ? 0u : bytes)
{
    AV_SCOPE("PitchedRows::PitchedRows");
}

/* Rows only read from: nothing is ever written through them. */
PitchedRows::PitchedRows(const uint8_t *base, uint64_t pitch, uint64_t bytes)
    : base_(nullptr), source_(base), pitch_(pitch), bytes_(base == nullptr ? 0u : bytes)
{
    AV_SCOPE("PitchedRows::PitchedRows");
}

/* row * pitch cannot wrap: a row past the memory is refused before it is
 * multiplied by more than the memory's size allows. */
bool PitchedRows::holds(uint32_t row, uint64_t line) const
{
    AV_SCOPE("PitchedRows::holds");
    if (line > bytes_) return false;
    if (pitch_ == 0u) return row == 0u;
    return row <= (bytes_ - line) / pitch_;
}

uint32_t PitchedRows::copy_from(const PitchedRows &from, uint32_t rows)
{
    AV_SCOPE("PitchedRows::copy_from");
    return copy_lines(from, rows, from.pitch_ < pitch_ ? from.pitch_ : pitch_);
}

uint32_t PitchedRows::copy_lines(const PitchedRows &from, uint32_t rows, uint64_t line)
{
    AV_SCOPE("PitchedRows::copy_lines");
    if (base_ == nullptr || line == 0u) return 0u;
    uint32_t row = 0u;
    for (; row < rows && holds(row, line) && from.holds(row, line); row++)
        __builtin_memcpy(base_ + static_cast<size_t>(uint64_t{row} * pitch_),
                         from.source_ + static_cast<size_t>(uint64_t{row} * from.pitch_), static_cast<size_t>(line));
    return row;
}

}  // namespace av
