#ifndef AV_CLICD_CL_PROPERTY_LIST_HPP
#define AV_CLICD_CL_PROPERTY_LIST_HPP

#include "core/trace/function_scope.hpp"

#include <CL/cl.h>

#include <stdint.h>

namespace av::clicd {

struct ClPropertyListProbe;

class ClPropertyList {
public:
    static constexpr uint32_t max_pairs = 32u;

    ClPropertyList();
    ClPropertyList(const ClPropertyList &) = delete;
    ClPropertyList &operator=(const ClPropertyList &) = delete;

    template <typename Property>
    [[nodiscard]] bool read(const Property *list);

    uint32_t pairs() const;
    uint64_t name_at(uint32_t index) const;
    uint64_t value_at(uint32_t index) const;
    bool     names(uint64_t name, uint64_t &value) const;
    bool     repeated() const;
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

}

#endif
