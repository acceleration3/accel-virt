#include "executor/d3d/d3d_object_table.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

#include <sys/mman.h>

namespace av {

namespace {

/* Knuth's multiplicative hash: guest ids are small and sequential, and a table
 * probed from id % capacity would cluster every id of one batch together. */
constexpr uint32_t golden_ratio = 2654435769u;

}  // namespace

D3dObjectTable::D3dObjectTable()
    : table_(nullptr), objects_(nullptr), viewed_(nullptr), ids_(nullptr), view_slots_(nullptr), view_at_(nullptr),
      kinds_(nullptr), tombstones_(nullptr), views_(0u), count_(0u)
{
    AV_SCOPE("D3dObjectTable::D3dObjectTable");
}

D3dObjectTable::~D3dObjectTable()
{
    AV_SCOPE("D3dObjectTable::~D3dObjectTable");
    if (table_ != nullptr) munmap(table_, table_bytes);
}

/* Anonymous and unreserved: pages are zero, and count only once touched. */
bool D3dObjectTable::reserve()
{
    AV_SCOPE("D3dObjectTable::reserve");
    if (table_ != nullptr) return true;
    void *const mapped = mmap(nullptr, table_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
                              -1, 0);
    if (mapped == MAP_FAILED) return false;
    table_ = mapped;
    objects_ = static_cast<IUnknown **>(mapped);
    viewed_ = objects_ + capacity;
    ids_ = reinterpret_cast<uint32_t *>(viewed_ + capacity);
    view_slots_ = ids_ + capacity;
    view_at_ = view_slots_ + capacity;
    kinds_ = reinterpret_cast<D3dObjectKind *>(view_at_ + capacity);
    tombstones_ = reinterpret_cast<bool *>(kinds_ + capacity);
    return true;
}

uint32_t D3dObjectTable::home_of(uint32_t id) const
{
    AV_SCOPE("D3dObjectTable::home_of");
    return (id * golden_ratio) % capacity;
}

uint32_t D3dObjectTable::slot_of(uint32_t id) const
{
    AV_SCOPE("D3dObjectTable::slot_of");
    if (table_ == nullptr) return capacity;
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
    if (count_ >= capacity / 4u * 3u || slot_of(id) != capacity || !reserve()) return false;
    uint32_t slot = home_of(id);
    while (kinds_[slot] != D3dObjectKind::none) slot = (slot + 1u) % capacity;
    set(slot, id, kind, false, object);
    count_++;
    return true;
}

bool D3dObjectTable::keep(uint32_t id, D3dObjectKind kind, IUnknown *made)
{
    AV_SCOPE("D3dObjectTable::keep");
    if (made == nullptr) return false;
    if (insert(id, kind, made)) return true;
    made->Release();
    return false;
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

IUnknown *D3dObjectTable::find_view(uint32_t id) const
{
    AV_SCOPE("D3dObjectTable::find_view");
    const uint32_t slot = id == 0u ? capacity : slot_of(id);
    return slot == capacity || !view_kind(kinds_[slot]) ? nullptr : objects_[slot];
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
    if (table_ == nullptr) return capacity;
    for (uint32_t slot = from; slot < capacity; slot++)
        if (kinds_[slot] != D3dObjectKind::none) return slot;
    return capacity;
}

IUnknown *D3dObjectTable::viewed(uint32_t slot) const
{
    AV_SCOPE("D3dObjectTable::viewed");
    return slot < capacity && table_ != nullptr ? viewed_[slot] : nullptr;
}

void D3dObjectTable::note_viewed(uint32_t slot, IUnknown *resource)
{
    AV_SCOPE("D3dObjectTable::note_viewed");
    if (slot < capacity && table_ != nullptr) viewed_[slot] = resource;
}

IUnknown *D3dObjectTable::at(uint32_t slot, uint32_t &id, D3dObjectKind &kind) const
{
    AV_SCOPE("D3dObjectTable::at");
    const bool used = slot < capacity && table_ != nullptr && kinds_[slot] != D3dObjectKind::none;
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

void D3dObjectTable::remove_kinds(D3dObjectKind first, D3dObjectKind last)
{
    AV_SCOPE("D3dObjectTable::remove_kinds");
    for (uint32_t slot = next_used(0u); slot < capacity; slot = next_used(slot + 1u))
        if (kinds_[slot] >= first && kinds_[slot] <= last) remove(ids_[slot]);
}

uint32_t D3dObjectTable::count_kinds(D3dObjectKind first, D3dObjectKind last) const
{
    AV_SCOPE("D3dObjectTable::count_kinds");
    uint32_t kept = 0u;
    for (uint32_t slot = next_used(0u); slot < capacity; slot = next_used(slot + 1u))
        if (kinds_[slot] >= first && kinds_[slot] <= last) kept++;
    return kept;
}

/* The pages go back zeroed, so nothing is left to reset slot by slot. */
void D3dObjectTable::clear()
{
    AV_SCOPE("D3dObjectTable::clear");
    for (uint32_t slot = next_used(0u); slot < capacity; slot = next_used(slot + 1u)) objects_[slot]->Release();
    if (table_ != nullptr) static_cast<void>(madvise(table_, table_bytes, MADV_DONTNEED));
    views_ = 0u;
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
    if (table_ == nullptr) return count_ == 0u && views_ == 0u;
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
