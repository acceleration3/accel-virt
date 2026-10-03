/* Answers to info queries that cannot change, kept so asking again costs no
 * round trip.
 *
 * A device's properties and a buffer's type, flags, size and offset are fixed
 * for its life, and an application may ask for them on every operation:
 * Affinity asks each tile's buffer and the device hundreds of times a second
 * while a layer moves, from every worker thread at once, and each answer the
 * host gave was a round trip under the lock all of them share. Here the first
 * answer is kept and every later one is the guest's own.
 *
 * Entries are only ever added, so a reader takes no lock: a writer claims a
 * slot, fills it and only then marks it ready, and a reader skips any slot not
 * yet ready. Two threads keeping the same answer at once keep it twice, which
 * costs a slot and answers the same. A table that is full keeps nothing more,
 * and an answer longer than `Bytes` is never kept: both are asked of the host
 * every time, as before. */
#ifndef AV_CLICD_CL_INFO_ANSWERS_HPP
#define AV_CLICD_CL_INFO_ANSWERS_HPP

#include "core/trace/function_scope.hpp"
#include "icd/opencl/cl_info_request.hpp"

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av::clicd {

template <uint32_t Entries, size_t Bytes>
class ClInfoAnswers {
public:
    ClInfoAnswers();
    ClInfoAnswers(const ClInfoAnswers &) = delete;
    ClInfoAnswers &operator=(const ClInfoAnswers &) = delete;

    /* The answer kept for `name` of `object` given to `asked`, as the API
     * gives one, with `result` what the call returns; false when none is kept. */
    [[nodiscard]] bool answer(uint64_t object, uint32_t name, const ClInfoRequest &asked, cl_int &result) const;
    /* Keeps the `bytes` at `value` as that answer: nothing for one longer than
     * `Bytes` or a table already full. */
    void               keep(uint64_t object, uint32_t name, const void *value, size_t bytes);
    uint32_t           kept() const;

private:
    struct Entry {
        uint64_t object_;
        uint32_t name_;
        uint32_t bytes_;
        bool     ready_;
        uint8_t  value_[Bytes];
    };

    Entry    entries_[Entries];
    uint32_t claimed_;
};

template <uint32_t Entries, size_t Bytes>
ClInfoAnswers<Entries, Bytes>::ClInfoAnswers() : entries_(), claimed_(0u)
{
    AV_SCOPE("ClInfoAnswers::ClInfoAnswers");
}

template <uint32_t Entries, size_t Bytes>
bool ClInfoAnswers<Entries, Bytes>::answer(uint64_t object, uint32_t name, const ClInfoRequest &asked,
                                           cl_int &result) const
{
    AV_SCOPE("ClInfoAnswers::answer");
    const uint32_t claimed = __atomic_load_n(&claimed_, __ATOMIC_ACQUIRE);
    for (uint32_t index = 0u; index < claimed; ++index) {
        const Entry &entry = entries_[index];
        if (!__atomic_load_n(&entry.ready_, __ATOMIC_ACQUIRE)) continue;
        if (entry.object_ != object || entry.name_ != name) continue;
        result = asked.give(entry.value_, entry.bytes_);
        return true;
    }
    return false;
}

template <uint32_t Entries, size_t Bytes>
void ClInfoAnswers<Entries, Bytes>::keep(uint64_t object, uint32_t name, const void *value, size_t bytes)
{
    AV_SCOPE("ClInfoAnswers::keep");
    if (bytes > Bytes) return;
    /* Claimed one at a time, so a table that is full stays exactly full and
     * the count never runs past it however many keep after. */
    uint32_t slot = __atomic_load_n(&claimed_, __ATOMIC_ACQUIRE);
    do {
        if (slot >= Entries) return;
    } while (!__atomic_compare_exchange_n(&claimed_, &slot, slot + 1u, true, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE));
    Entry &entry = entries_[slot];
    entry.object_ = object;
    entry.name_ = name;
    entry.bytes_ = static_cast<uint32_t>(bytes);
    __builtin_memcpy(entry.value_, value, bytes);
    __atomic_store_n(&entry.ready_, true, __ATOMIC_RELEASE);
}

template <uint32_t Entries, size_t Bytes>
uint32_t ClInfoAnswers<Entries, Bytes>::kept() const
{
    AV_SCOPE("ClInfoAnswers::kept");
    return __atomic_load_n(&claimed_, __ATOMIC_ACQUIRE);
}

}  // namespace av::clicd

#endif  // AV_CLICD_CL_INFO_ANSWERS_HPP
