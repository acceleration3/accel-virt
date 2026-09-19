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
 * The three spellings - cl_context_properties (intptr_t), cl_queue_properties
 * and cl_mem_properties (cl_bitfield) - are one type on the only target this
 * ICD ships for, which the static assertion in the .cpp is. A 32-bit ICD would
 * need the narrower walk too (feature 061). */
#ifndef AV_CLICD_CL_PROPERTY_LIST_HPP
#define AV_CLICD_CL_PROPERTY_LIST_HPP

#include <CL/cl.h>

#include <stdint.h>

namespace av::clicd {

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
     * not, which is refused. A second read replaces the first. */
    [[nodiscard]] bool read(const uint64_t *list);

    uint32_t pairs() const;
    uint64_t name_at(uint32_t index) const;
    uint64_t value_at(uint32_t index) const;
    /* The value the last pair with this name carried, as the API says a
     * repeated property is an error rather than a last-one-wins - which is
     * what `repeated()` is for. */
    bool     names(uint64_t name, uint64_t &value) const;
    bool     repeated() const;

    bool invariants_hold() const;

private:
    uint64_t names_[max_pairs];
    uint64_t values_[max_pairs];
    uint32_t pairs_;
    bool     repeated_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_PROPERTY_LIST_HPP
