#include "device/executor/d3d_object_table.hpp"

#include "core/trace/function_scope.hpp"
#include "device/shim/d3d_headers.hpp"

namespace av {

namespace {

/* Knuth's multiplicative hash: guest ids are small and sequential, and a table
 * probed from id % capacity would cluster every id of one batch together. */
constexpr uint32_t golden_ratio = 2654435769u;

}  // namespace

D3dObjectTable::D3dObjectTable() : ids_{}, kinds_{}, tombstones_{}, objects_{}, viewed_{}, view_slots_{}, view_at_{}, views_(0u), count_(0u)
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
        if (ids_[slot] == id && kinds_[slot] != D3dObjectKind::none) return slot;
        if (kinds_[slot] == D3dObjectKind::none && !tombstones_[slot]) return capacity;
        slot = (slot + 1u) % capacity;
    }
    return capacity;
}

namespace {

bool view_kind(D3dObjectKind kind)
{
    AV_SCOPE("view_kind");
    return kind == D3dObjectKind::shader_resource_view || kind == D3dObjectKind::render_target_view ||
           kind == D3dObjectKind::depth_stencil_view || kind == D3dObjectKind::unordered_access_view;
}

}  // namespace

void D3dObjectTable::set(uint32_t slot, uint32_t id, D3dObjectKind kind, bool tombstone, IUnknown *object)
{
    AV_SCOPE("D3dObjectTable::set");
    const bool was_view = view_kind(kinds_[slot]);
    const bool is_view = view_kind(kind);
    if (was_view && !is_view) {
        const uint32_t at = view_at_[slot];
        view_slots_[at] = view_slots_[--views_];
        view_at_[view_slots_[at]] = at;
    }
    if (!was_view && is_view) {
        view_at_[slot] = views_;
        view_slots_[views_++] = slot;
    }
    ids_[slot] = id;
    kinds_[slot] = kind;
    tombstones_[slot] = tombstone;
    objects_[slot] = object;
    viewed_[slot] = nullptr;
}

bool D3dObjectTable::insert(uint32_t id, D3dObjectKind kind, IUnknown *object)
{
    AV_SCOPE("D3dObjectTable::insert");
    if (id == 0u || kind == D3dObjectKind::none || object == nullptr) return false;
    /* Three quarters full is where linear probing stops being cheap; past it a
     * guest is refused rather than made to walk the table on every lookup. */
    if (count_ >= capacity / 4u * 3u || slot_of(id) != capacity) return false;
    uint32_t slot = home_of(id);
    while (kinds_[slot] != D3dObjectKind::none) slot = (slot + 1u) % capacity;
    set(slot, id, kind, false, object);
    count_++;
    return true;
}

IUnknown *D3dObjectTable::find(uint32_t id, D3dObjectKind kind) const
{
    AV_SCOPE("D3dObjectTable::find");
    const uint32_t slot = id == 0u ? capacity : slot_of(id);
    if (slot == capacity || kinds_[slot] != kind) return nullptr;
    return objects_[slot];
}

IUnknown *D3dObjectTable::find_any(uint32_t id, D3dObjectKind &kind) const
{
    AV_SCOPE("D3dObjectTable::find_any");
    const uint32_t slot = id == 0u ? capacity : slot_of(id);
    kind = slot == capacity ? D3dObjectKind::none : kinds_[slot];
    return slot == capacity ? nullptr : objects_[slot];
}

IUnknown *D3dObjectTable::exchange(uint32_t id, D3dObjectKind kind, IUnknown *object)
{
    AV_SCOPE("D3dObjectTable::exchange");
    const uint32_t slot = id == 0u || object == nullptr ? capacity : slot_of(id);
    if (slot == capacity || kinds_[slot] != kind) return nullptr;
    IUnknown *const old = objects_[slot];
    objects_[slot] = object;
    viewed_[slot] = nullptr;
    return old;
}

uint32_t D3dObjectTable::views() const
{
    AV_SCOPE("D3dObjectTable::views");
    return views_;
}

uint32_t D3dObjectTable::view_slot(uint32_t index) const
{
    AV_SCOPE("D3dObjectTable::view_slot");
    return index < views_ ? view_slots_[index] : capacity;
}

uint32_t D3dObjectTable::next_used(uint32_t from) const
{
    AV_SCOPE("D3dObjectTable::next_used");
    for (uint32_t slot = from; slot < capacity; slot++)
        if (kinds_[slot] != D3dObjectKind::none) return slot;
    return capacity;
}

IUnknown *D3dObjectTable::viewed(uint32_t slot) const
{
    AV_SCOPE("D3dObjectTable::viewed");
    return slot < capacity ? viewed_[slot] : nullptr;
}

void D3dObjectTable::note_viewed(uint32_t slot, IUnknown *resource)
{
    AV_SCOPE("D3dObjectTable::note_viewed");
    if (slot < capacity) viewed_[slot] = resource;
}

IUnknown *D3dObjectTable::at(uint32_t slot, uint32_t &id, D3dObjectKind &kind) const
{
    AV_SCOPE("D3dObjectTable::at");
    const bool used = slot < capacity && kinds_[slot] != D3dObjectKind::none;
    id = used ? ids_[slot] : 0u;
    kind = used ? kinds_[slot] : D3dObjectKind::none;
    return used ? objects_[slot] : nullptr;
}

void D3dObjectTable::remove(uint32_t id)
{
    AV_SCOPE("D3dObjectTable::remove");
    const uint32_t slot = id == 0u ? capacity : slot_of(id);
    if (slot == capacity) return;
    objects_[slot]->Release();
    set(slot, 0u, D3dObjectKind::none, true, nullptr);
    count_--;
}

void D3dObjectTable::clear()
{
    AV_SCOPE("D3dObjectTable::clear");
    for (uint32_t slot = 0u; slot < capacity; slot++) {
        if (kinds_[slot] != D3dObjectKind::none) objects_[slot]->Release();
        set(slot, 0u, D3dObjectKind::none, false, nullptr);
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
    for (uint32_t slot = 0u; slot < capacity; slot++) {
        if (kinds_[slot] == D3dObjectKind::none) {
            if (objects_[slot] != nullptr) return false;
            continue;
        }
        if (objects_[slot] == nullptr || ids_[slot] == 0u || tombstones_[slot]) return false;
        live++;
    }
    return live == count_;
}

}  // namespace av
