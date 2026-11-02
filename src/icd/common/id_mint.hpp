#ifndef AV_ICD_ID_MINT_HPP
#define AV_ICD_ID_MINT_HPP

#include "core/trace/function_scope.hpp"

#include <stdint.h>

namespace av::icd {

template <typename Id>
class IdMint {
    static_assert(Id(0) < Id(-1), "an id is an unsigned integer");

public:
    IdMint();
    IdMint(const IdMint &) = delete;
    IdMint &operator=(const IdMint &) = delete;

    Id   next();
    Id   run(Id count);
    bool issued(Id id) const;

private:
    Id latest_;
};

template <typename Id>
IdMint<Id>::IdMint() : latest_(Id(0))
{
    AV_SCOPE("IdMint::IdMint");
}

template <typename Id>
Id IdMint<Id>::next()
{
    AV_SCOPE("IdMint::next");
    return run(Id(1));
}

template <typename Id>
Id IdMint<Id>::run(Id count)
{
    AV_SCOPE("IdMint::run");
    if (count == Id(0)) count = Id(1);
    Id seen = __atomic_load_n(&latest_, __ATOMIC_RELAXED);
    for (;;) {
        const Id first = Id(-1) - seen < count ? Id(1) : Id(seen + 1u);
        const Id last = Id(first + (count - 1u));
        if (__atomic_compare_exchange_n(&latest_, &seen, last, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED))
            return first;
    }
}

template <typename Id>
bool IdMint<Id>::issued(Id id) const
{
    AV_SCOPE("IdMint::issued");
    return id != Id(0) && id <= __atomic_load_n(&latest_, __ATOMIC_RELAXED);
}

}

#endif
