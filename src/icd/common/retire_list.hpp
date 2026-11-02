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

    [[nodiscard]] Status add(Entry entry);
    bool                 take_last(Entry &entry);
    template <typename Matches>
    bool                 take_if(Matches matches, Entry &entry);
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
template <typename Matches>
bool RetireList<Entry>::take_if(Matches matches, Entry &entry)
{
    AV_SCOPE("RetireList::take_if");
    for (uint32_t index = 0u; index < entries_.size(); index++) {
        if (!matches(entries_.at(index))) continue;
        entry = entries_.at(index);
        entries_.remove_at(index);
        return true;
    }
    return false;
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

}

#endif
