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
 * Entries are only ever added, so a reader takes no lock (ClaimedRows): a
 * reader passes any entry not yet written. Two threads keeping the same
 * answer at once keep it twice, which costs an entry and answers the same.
 * As many are kept as there are answers; one longer than `Bytes`, or one
 * there is no memory for, is never kept and is asked of the host every time,
 * as before. */
#ifndef AV_CLICD_CL_INFO_ANSWERS_HPP
#define AV_CLICD_CL_INFO_ANSWERS_HPP

#include "core/support/claimed_rows.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/opencl/cl_info_request.hpp"

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av {
class BlockSource;
}  // namespace av

namespace av::clicd {

template <size_t Bytes>
class ClInfoAnswers {
public:
    explicit ClInfoAnswers(BlockSource &memory);
    ClInfoAnswers(const ClInfoAnswers &) = delete;
    ClInfoAnswers &operator=(const ClInfoAnswers &) = delete;

    /* The answer kept for `name` of `object` given to `asked`, as the API
     * gives one, with `result` what the call returns; false when none is kept. */
    [[nodiscard]] bool answer(uint64_t object, uint32_t name, const ClInfoRequest &asked, cl_int &result) const;
    /* Keeps the `bytes` at `value` as that answer: nothing for one longer than
     * `Bytes` or one there is no memory for. */
    void               keep(uint64_t object, uint32_t name, const void *value, size_t bytes);
    uint32_t           kept() const;

private:
    struct Entry {
        uint64_t object_;
        uint32_t name_;
        uint32_t bytes_;
        uint8_t  value_[Bytes];
    };

    ClaimedRows<Entry> entries_;
};

template <size_t Bytes>
ClInfoAnswers<Bytes>::ClInfoAnswers(BlockSource &memory) : entries_(memory, Pool::paged)
{
    AV_SCOPE("ClInfoAnswers::ClInfoAnswers");
}

template <size_t Bytes>
bool ClInfoAnswers<Bytes>::answer(uint64_t object, uint32_t name, const ClInfoRequest &asked, cl_int &result) const
{
    AV_SCOPE("ClInfoAnswers::answer");
    const Entry *const entry =
        entries_.find_if([object, name](const Entry &kept) { return kept.object_ == object && kept.name_ == name; });
    if (entry == nullptr) return false;
    result = asked.give(entry->value_, entry->bytes_);
    return true;
}

template <size_t Bytes>
void ClInfoAnswers<Bytes>::keep(uint64_t object, uint32_t name, const void *value, size_t bytes)
{
    AV_SCOPE("ClInfoAnswers::keep");
    if (bytes > Bytes) return;
    Entry entry{};
    entry.object_ = object;
    entry.name_ = name;
    entry.bytes_ = static_cast<uint32_t>(bytes);
    __builtin_memcpy(entry.value_, value, bytes);
    static_cast<void>(entries_.append(entry));
}

template <size_t Bytes>
uint32_t ClInfoAnswers<Bytes>::kept() const
{
    AV_SCOPE("ClInfoAnswers::kept");
    return entries_.claimed();
}

}  // namespace av::clicd

#endif  // AV_CLICD_CL_INFO_ANSWERS_HPP
