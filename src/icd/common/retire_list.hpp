/* Memory a host API's object was made over, let go of by the application
 * while work the host queued may still read or write it (docs/host-memory.md,
 * "Application memory"): kept here until the API knows that work is done,
 * then handed back. An OpenCL buffer can be released with kernels over it
 * still queued, and its arena block or the pages it was lent are not another
 * buffer's until those kernels finish. The list grows with what is retired;
 * its owner serializes it. */
#ifndef AV_ICD_RETIRE_LIST_HPP
#define AV_ICD_RETIRE_LIST_HPP

#include "core/platform/block_source.hpp"
#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"

#include <stdint.h>

namespace av::icd {

template <typename Entry>
class RetireList {
public:
    RetireList(BlockSource &memory, Pool pool);
    RetireList(const RetireList &) = delete;
    RetireList &operator=(const RetireList &) = delete;

    /* Kept until reclaimed: no_memory, keeping nothing, when the list cannot
     * grow - the caller then waits for the host's work itself. */
    [[nodiscard]] Status add(Entry entry);
    /* The newest entry out of the list into `entry`; false when it is empty.
     * For an owner whose give-back takes the lock the list is under. */
    bool                 take_last(Entry &entry);
    /* Every entry handed to `give_back`, oldest first, and the list emptied. */
    template <typename GiveBack>
    void                 reclaim(GiveBack give_back);
    uint32_t             count() const;

private:
    GrowableArray<Entry> entries_;
};

template <typename Entry>
RetireList<Entry>::RetireList(BlockSource &memory, Pool pool) : entries_(memory, pool)
{
    AV_SCOPE("RetireList::RetireList");
}

template <typename Entry>
Status RetireList<Entry>::add(Entry entry)
{
    AV_SCOPE("RetireList::add");
    return entries_.append(entry);
}

template <typename Entry>
bool RetireList<Entry>::take_last(Entry &entry)
{
    AV_SCOPE("RetireList::take_last");
    if (entries_.size() == 0u) return false;
    entry = entries_.at(entries_.size() - 1u);
    entries_.truncate(entries_.size() - 1u);
    return true;
}

template <typename Entry>
template <typename GiveBack>
void RetireList<Entry>::reclaim(GiveBack give_back)
{
    AV_SCOPE("RetireList::reclaim");
    for (const Entry &entry : entries_) give_back(entry);
    entries_.clear();
}

template <typename Entry>
uint32_t RetireList<Entry>::count() const
{
    AV_SCOPE("RetireList::count");
    return entries_.size();
}

}  // namespace av::icd

#endif  // AV_ICD_RETIRE_LIST_HPP
