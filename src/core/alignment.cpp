#include "alignment.hpp"

namespace av {

Alignment::Alignment(uint64_t value) : value_(value) {}

bool Alignment::is_valid() const
{
    return value_ != 0u && (value_ & (value_ - 1u)) == 0u;
}

uint64_t Alignment::value() const { return value_; }

bool Alignment::divides(uint64_t value) const
{
    return is_valid() && (value & (value_ - 1u)) == 0u;
}

bool Alignment::align_up(uint64_t address, uint64_t &result) const
{
    if (!is_valid()) return false;
    const uint64_t slack = value_ - 1u;
    if (address > UINT64_MAX - slack) return false;
    result = (address + slack) & ~slack;
    return true;
}

}  // namespace av
