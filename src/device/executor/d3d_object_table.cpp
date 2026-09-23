#include "device/executor/d3d_object_table.hpp"

#include "core/trace/function_scope.hpp"
#include "device/shim/d3d_headers.hpp"

namespace av {

namespace {

/* Knuth's multiplicative hash: guest ids are small and sequential, and a table
 * probed from id % capacity would cluster every id of one batch together. */
constexpr uint32_t golden_ratio = 2654435769u;

}  // namespace

D3dObjectTable::D3dObjectTable() : entries_{}, count_(0u)
{
    AV_SCOPE("D3dObjectTable::D3dObjectTable");
}

uint32_t D3dObjectTable::home_of(uint32_t id) const
{
    AV_SCOPE("D3dObjectTable::home_of");
    return (id * golden_ratio) % capacity;
}

uint32_t D3dObjectTable::slot_of(uint32_t id) const
{
    AV_SCOPE("D3dObjectTable::slot_of");
    uint32_t slot = home_of(id);
    for (uint32_t probe = 0u; probe < capacity; probe++) {
        const Entry &entry = entries_[slot];
        if (entry.id == id && entry.kind != D3dObjectKind::none) return slot;
        if (entry.kind == D3dObjectKind::none && !entry.tombstone) return capacity;
        slot = (slot + 1u) % capacity;
    }
    return capacity;
}

bool D3dObjectTable::insert(uint32_t id, D3dObjectKind kind, IUnknown *object)
{
    AV_SCOPE("D3dObjectTable::insert");
    if (id == 0u || kind == D3dObjectKind::none || object == nullptr) return false;
    /* Three quarters full is where linear probing stops being cheap; past it a
     * guest is refused rather than made to walk the table on every lookup. */
    if (count_ >= capacity / 4u * 3u || slot_of(id) != capacity) return false;
    uint32_t slot = home_of(id);
    while (entries_[slot].kind != D3dObjectKind::none) slot = (slot + 1u) % capacity;
    entries_[slot] = Entry{id, kind, false, object};
    count_++;
    return true;
}

IUnknown *D3dObjectTable::find(uint32_t id, D3dObjectKind kind) const
{
    AV_SCOPE("D3dObjectTable::find");
    const uint32_t slot = id == 0u ? capacity : slot_of(id);
    if (slot == capacity || entries_[slot].kind != kind) return nullptr;
    return entries_[slot].object;
}

IUnknown *D3dObjectTable::find_any(uint32_t id, D3dObjectKind &kind) const
{
    AV_SCOPE("D3dObjectTable::find_any");
    const uint32_t slot = id == 0u ? capacity : slot_of(id);
    kind = slot == capacity ? D3dObjectKind::none : entries_[slot].kind;
    return slot == capacity ? nullptr : entries_[slot].object;
}

void D3dObjectTable::remove(uint32_t id)
{
    AV_SCOPE("D3dObjectTable::remove");
    const uint32_t slot = id == 0u ? capacity : slot_of(id);
    if (slot == capacity) return;
    entries_[slot].object->Release();
    entries_[slot] = Entry{0u, D3dObjectKind::none, true, nullptr};
    count_--;
}

void D3dObjectTable::clear()
{
    AV_SCOPE("D3dObjectTable::clear");
    for (Entry &entry : entries_) {
        if (entry.kind != D3dObjectKind::none) entry.object->Release();
        entry = Entry{0u, D3dObjectKind::none, false, nullptr};
    }
    count_ = 0u;
}

uint32_t D3dObjectTable::count() const
{
    AV_SCOPE("D3dObjectTable::count");
    return count_;
}

bool D3dObjectTable::invariants_hold() const
{
    AV_SCOPE("D3dObjectTable::invariants_hold");
    uint32_t live = 0u;
    for (const Entry &entry : entries_) {
        if (entry.kind == D3dObjectKind::none) {
            if (entry.object != nullptr) return false;
            continue;
        }
        if (entry.object == nullptr || entry.id == 0u || entry.tombstone) return false;
        live++;
    }
    return live == count_;
}

}  // namespace av
