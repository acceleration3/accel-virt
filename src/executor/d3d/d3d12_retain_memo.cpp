#include "executor/d3d/d3d12_retain_memo.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

D3d12RetainMemo::D3d12RetainMemo() : entries_{}
{
    AV_SCOPE("D3d12RetainMemo::D3d12RetainMemo");
}

uint32_t D3d12RetainMemo::way_of(const IUnknown *object) const
{
    AV_SCOPE("D3d12RetainMemo::way_of");
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(object) >> 4u) % ways;
}

bool D3d12RetainMemo::covers(uint32_t list, const IUnknown *object, const D3d12BlockHeap *pin) const
{
    AV_SCOPE("D3d12RetainMemo::covers");
    const Entry &entry = entries_[way_of(object)];
    return entry.object_ == object && entry.list_ == list && (pin == nullptr || entry.pinned_);
}

bool D3d12RetainMemo::whole(uint32_t list, const IUnknown *object) const
{
    AV_SCOPE("D3d12RetainMemo::whole");
    const Entry &entry = entries_[way_of(object)];
    return entry.object_ == object && entry.list_ == list && entry.whole_;
}

void D3d12RetainMemo::note(uint32_t list, const IUnknown *object, bool pinned)
{
    AV_SCOPE("D3d12RetainMemo::note");
    Entry &entry = entries_[way_of(object)];
    if (entry.object_ == object && entry.list_ == list) {
        entry.pinned_ = entry.pinned_ || pinned;
        return;
    }
    entry = Entry{list, object, pinned, false};
}

void D3d12RetainMemo::note_whole(uint32_t list, const IUnknown *object)
{
    AV_SCOPE("D3d12RetainMemo::note_whole");
    Entry &entry = entries_[way_of(object)];
    entry = Entry{list, object, true, true};
}

void D3d12RetainMemo::clear()
{
    AV_SCOPE("D3d12RetainMemo::clear");
    for (Entry &entry : entries_) entry = Entry{};
}

}
