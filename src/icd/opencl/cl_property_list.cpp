#include "icd/opencl/cl_property_list.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

/* The two spellings read as uint64_t lists, on every target. */
static_assert(sizeof(cl_queue_properties) == sizeof(uint64_t), "cl_queue_properties");
static_assert(sizeof(cl_mem_properties) == sizeof(uint64_t), "cl_mem_properties");

ClPropertyList::ClPropertyList() : names_{}, values_{}, pairs_(0u), repeated_(false)
{
    AV_SCOPE("ClPropertyList::ClPropertyList");
}

uint32_t ClPropertyList::pairs() const
{
    AV_SCOPE("ClPropertyList::pairs");
    return pairs_;
}

uint64_t ClPropertyList::name_at(uint32_t index) const
{
    AV_SCOPE("ClPropertyList::name_at");
    return index < pairs_ ? names_[index] : 0u;
}

uint64_t ClPropertyList::value_at(uint32_t index) const
{
    AV_SCOPE("ClPropertyList::value_at");
    return index < pairs_ ? values_[index] : 0u;
}

bool ClPropertyList::names(uint64_t name, uint64_t &value) const
{
    AV_SCOPE("ClPropertyList::names");
    if (name == 0u) return false;
    for (uint32_t index = 0u; index < pairs_; ++index) {
        if (names_[index] != name) continue;
        value = values_[index];
        return true;
    }
    return false;
}

bool ClPropertyList::repeated() const
{
    AV_SCOPE("ClPropertyList::repeated");
    return repeated_;
}

bool ClPropertyList::invariants_hold() const
{
    AV_SCOPE("ClPropertyList::invariants_hold");
    if (pairs_ > max_pairs) return false;
    /* A zero name is the terminator, so no pair carries one. */
    for (uint32_t index = 0u; index < pairs_; ++index)
        if (names_[index] == 0u) return false;
    return true;
}

}  // namespace av::clicd
