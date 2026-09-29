#include "icd/opencl/cl_property_list.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

/* What lets one walk serve all three of the API's spellings. */
static_assert(sizeof(cl_context_properties) == sizeof(uint64_t),
              "a 32-bit ICD needs its own walk (feature 061)");
static_assert(sizeof(cl_queue_properties) == sizeof(uint64_t), "cl_queue_properties");
static_assert(sizeof(cl_mem_properties) == sizeof(uint64_t), "cl_mem_properties");

ClPropertyList::ClPropertyList() : names_{}, values_{}, pairs_(0u), repeated_(false)
{
    AV_SCOPE("ClPropertyList::ClPropertyList");
}

bool ClPropertyList::read(const uint64_t *list)
{
    AV_SCOPE("ClPropertyList::read");
    pairs_ = 0u;
    repeated_ = false;
    if (list == nullptr) return true;
    while (list[pairs_ * 2u] != 0u) {
        if (pairs_ == max_pairs) {
            /* No terminator inside the bound. Everything read so far is
             * dropped: a half-read list is not a list. */
            pairs_ = 0u;
            return false;
        }
        const uint64_t name = list[pairs_ * 2u];
        const uint64_t value = list[pairs_ * 2u + 1u];
        for (uint32_t earlier = 0u; earlier < pairs_; ++earlier)
            if (names_[earlier] == name) repeated_ = true;
        names_[pairs_] = name;
        values_[pairs_] = value;
        ++pairs_;
    }
    return true;
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

uint32_t ClPropertyList::lay_out(uint64_t *out, uint32_t capacity) const
{
    AV_SCOPE("ClPropertyList::lay_out");
    const uint32_t values = pairs_ * 2u + 1u;
    if (pairs_ == 0u || out == nullptr || capacity < values) return 0u;
    for (uint32_t index = 0u; index < pairs_; ++index) {
        out[index * 2u] = names_[index];
        out[index * 2u + 1u] = values_[index];
    }
    out[values - 1u] = 0u;
    return values;
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
