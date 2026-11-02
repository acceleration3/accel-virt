#include "icd/d3d/d3d9_state_entry.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

D3d9StateEntry::D3d9StateEntry() : kind_(Kind::none), a_(0u), b_(0u), value_{}
{
    AV_SCOPE("D3d9StateEntry::D3d9StateEntry");
}

D3d9StateEntry::D3d9StateEntry(Kind kind, uint32_t a, uint32_t b, const uint32_t *value, uint32_t words)
    : kind_(kind), a_(a), b_(b), value_{}
{
    AV_SCOPE("D3d9StateEntry::D3d9StateEntry");
    for (uint32_t index = 0u; index < words && index < value_words && value != nullptr; index++)
        value_[index] = value[index];
}

D3d9StateEntry::D3d9StateEntry(Kind kind, uint32_t a, const void *handle, uint32_t second, uint32_t third)
    : kind_(kind), a_(a), b_(0u), value_{}
{
    AV_SCOPE("D3d9StateEntry::D3d9StateEntry");
    const uint64_t address = reinterpret_cast<uintptr_t>(handle);
    value_[0] = static_cast<uint32_t>(address);
    value_[1] = static_cast<uint32_t>(address >> 32u);
    value_[2] = second;
    value_[3] = third;
}

D3d9StateEntry::Kind D3d9StateEntry::kind() const
{
    AV_SCOPE("D3d9StateEntry::kind");
    return kind_;
}

uint32_t D3d9StateEntry::a() const
{
    AV_SCOPE("D3d9StateEntry::a");
    return a_;
}

uint32_t D3d9StateEntry::b() const
{
    AV_SCOPE("D3d9StateEntry::b");
    return b_;
}

const uint32_t *D3d9StateEntry::value() const
{
    AV_SCOPE("D3d9StateEntry::value");
    return value_;
}

void *D3d9StateEntry::handle() const
{
    AV_SCOPE("D3d9StateEntry::handle");
    const uint64_t address = (uint64_t{value_[1]} << 32u) | value_[0];
    return reinterpret_cast<void *>(static_cast<uintptr_t>(address));
}

bool D3d9StateEntry::names(const D3d9StateEntry &other) const
{
    AV_SCOPE("D3d9StateEntry::names");
    return kind_ == other.kind_ && a_ == other.a_ && b_ == other.b_;
}

void D3d9StateEntry::take_value(const D3d9StateEntry &other)
{
    AV_SCOPE("D3d9StateEntry::take_value");
    for (uint32_t index = 0u; index < value_words; index++) value_[index] = other.value_[index];
}

}
