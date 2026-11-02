#include "core/support/alignment.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

Alignment::Alignment(uint64_t value) : value_(value)
{
    AV_SCOPE("Alignment::Alignment");
}

bool Alignment::is_valid() const
{
    AV_SCOPE("Alignment::is_valid");
    return value_ != 0u && (value_ & (value_ - 1u)) == 0u;
}

uint64_t Alignment::value() const
{
    AV_SCOPE("Alignment::value");
    return value_;
}

bool Alignment::divides(uint64_t value) const
{
    AV_SCOPE("Alignment::divides");
    return is_valid() && (value & (value_ - 1u)) == 0u;
}

bool Alignment::align_up(uint64_t address, uint64_t &result) const
{
    AV_SCOPE("Alignment::align_up");
    if (!is_valid()) return false;
    const uint64_t slack = value_ - 1u;
    if (address > UINT64_MAX - slack) return false;
    result = (address + slack) & ~slack;
    return true;
}

}
