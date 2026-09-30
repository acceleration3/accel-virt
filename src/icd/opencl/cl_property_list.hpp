/* A property list as the application wrote it: pairs of a name and a value,
 * terminated by a zero name.
 *
 * Three OpenCL calls take one - clCreateContext, clCreateCommandQueueWithProperties
 * and clCreateBufferWithProperties - and all three lists are the same shape.
 * The ICD has to walk each of them before it can refuse an unknown property,
 * which the API requires it to do (CL_INVALID_PROPERTY): a driver that ignores
 * a property it does not implement is a driver that silently does the wrong
 * thing. Walking it is also the one place the ICD reads an unbounded array the
 * application chose the length of, so the bound is the point: a list with no
 * terminator inside `max_pairs` is refused rather than read off the end.
 *
 * The three spellings are not one width: cl_queue_properties and
 * cl_mem_properties are a cl_bitfield, 64 bits everywhere, while
 * cl_context_properties is an intptr_t, 32 bits in a 32-bit process. So a list
 * is read, and laid out again, in the element type the API gave it, and held
 * as 64-bit names and values either way. */
#ifndef AV_CLICD_CL_PROPERTY_LIST_HPP
#define AV_CLICD_CL_PROPERTY_LIST_HPP

#include "core/trace/function_scope.hpp"

#include <CL/cl.h>

#include <stdint.h>

namespace av::clicd {

struct ClPropertyListProbe;

class ClPropertyList {
public:
    /* Pairs one list may carry. The API names a handful of properties; a list
     * this long is an application that has lost its terminator. */
    static constexpr uint32_t max_pairs = 32u;

    ClPropertyList();
    ClPropertyList(const ClPropertyList &) = delete;
    ClPropertyList &operator=(const ClPropertyList &) = delete;

    /* Walks `list` until its zero name. True for a list that ended inside the
     * bound, including a null list and an empty one; false for one that did
     * not, which is refused. A second read replaces the first. `Property` is
     * the list's element type: uint64_t, or cl_context_properties. */
    template <typename Property>
    [[nodiscard]] bool read(const Property *list);

    uint32_t pairs() const;
    uint64_t name_at(uint32_t index) const;
    uint64_t value_at(uint32_t index) const;
    /* The value the last pair with this name carried, as the API says a
     * repeated property is an error rather than a last-one-wins - which is
     * what `repeated()` is for. */
    bool     names(uint64_t name, uint64_t &value) const;
    bool     repeated() const;
    /* The list as it was read, pairs then its zero name, into `out` as
     * `Property` elements; how many values that is, or 0 for a list with no
     * pair or too little room. */
    template <typename Property>
    uint32_t lay_out(Property *out, uint32_t capacity) const;

    bool invariants_hold() const;

private:
    friend struct ClPropertyListProbe;

    uint64_t names_[max_pairs];
    uint64_t values_[max_pairs];
    uint32_t pairs_;
    bool     repeated_;
};

/* A narrower element - a 32-bit process's intptr_t - widens without its sign,
 * so a handle or address past 2 GiB is the number the application wrote. */
template <typename Property>
bool ClPropertyList::read(const Property *list)
{
    AV_SCOPE("ClPropertyList::read");
    static_assert(sizeof(Property) <= sizeof(uint64_t));
    pairs_ = 0u;
    repeated_ = false;
    if (list == nullptr) return true;
    while (list[pairs_ * 2u] != 0) {
        if (pairs_ == max_pairs) {
            /* No terminator inside the bound. Everything read so far is
             * dropped: a half-read list is not a list. */
            pairs_ = 0u;
            return false;
        }
        uint64_t name = 0u;
        uint64_t value = 0u;
        __builtin_memcpy(&name, &list[pairs_ * 2u], sizeof(Property));
        __builtin_memcpy(&value, &list[pairs_ * 2u + 1u], sizeof(Property));
        for (uint32_t earlier = 0u; earlier < pairs_; ++earlier)
            if (names_[earlier] == name) repeated_ = true;
        names_[pairs_] = name;
        values_[pairs_] = value;
        ++pairs_;
    }
    return true;
}

template <typename Property>
uint32_t ClPropertyList::lay_out(Property *out, uint32_t capacity) const
{
    AV_SCOPE("ClPropertyList::lay_out");
    static_assert(sizeof(Property) <= sizeof(uint64_t));
    const uint32_t values = pairs_ * 2u + 1u;
    if (pairs_ == 0u || out == nullptr || capacity < values) return 0u;
    for (uint32_t index = 0u; index < pairs_; ++index) {
        __builtin_memcpy(&out[index * 2u], &names_[index], sizeof(Property));
        __builtin_memcpy(&out[index * 2u + 1u], &values_[index], sizeof(Property));
    }
    out[values - 1u] = 0;
    return values;
}

}  // namespace av::clicd

#endif  // AV_CLICD_CL_PROPERTY_LIST_HPP
