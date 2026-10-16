/* Hands out non-zero ids in increasing order to any thread: Direct3D's guest
 * object ids, OpenGL's sync tokens and the names its context gives memory
 * objects and semaphores.
 *
 * Zero is never one, since every wire this serves reads 0 as "no object". The
 * ids climb, because a host that bound one refuses an id not newer than it
 * (an OpenGL sync token), and a run of them is contiguous, because an
 * application is handed the run whole. Past the largest Id the mint starts
 * again at 1: a 32-bit id is the D3D wire's width, and the table an id is
 * remembered in refuses one still held rather than this mint guessing which
 * are. */
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

    /* One id. */
    Id   next();
    /* The first of `count` consecutive ids, none of them 0; `count` of 0 is
     * taken as 1. */
    Id   run(Id count);
    /* Whether `id` has been handed out since the mint last started again. */
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
        /* A run that would reach past the largest id, or the 0 after it,
         * starts again at 1. */
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

}  // namespace av::icd

#endif  // AV_ICD_ID_MINT_HPP
