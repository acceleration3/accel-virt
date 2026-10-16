#include "executor/d3d/d3d_object_table.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

static_assert(D3dObjectTable::no_slot == KeyedSlots<uint32_t, uint32_t>::no_slot, "one sentinel for no slot");

namespace {

bool view_kind(D3dObjectKind kind)
{
    AV_SCOPE("view_kind");
    return kind == D3dObjectKind::shader_resource_view || kind == D3dObjectKind::render_target_view ||
           kind == D3dObjectKind::depth_stencil_view || kind == D3dObjectKind::unordered_access_view;
}

}  // namespace

D3dObjectTable::D3dObjectTable(BlockSource &blocks) : objects_(blocks, Pool::paged), views_(blocks, Pool::paged)
{
    AV_SCOPE("D3dObjectTable::D3dObjectTable");
}

D3dObjectTable::~D3dObjectTable()
{
    AV_SCOPE("D3dObjectTable::~D3dObjectTable");
}

/* A view's place in views_ is made first, so the insert that follows is the
 * last thing that can fail and nothing has to be undone. */
Status D3dObjectTable::insert(uint32_t id, D3dObjectKind kind, IUnknown *object)
{
    AV_SCOPE("D3dObjectTable::insert");
    if (id == 0u || kind == D3dObjectKind::none || object == nullptr) return Status::invalid_parameter;
    if (objects_.slot_of(id) != no_slot) return Status::conflict;
    const bool view = view_kind(kind);
    if (view) {
        const Status room = views_.reserve(views_.size() + 1u);
        if (room != Status::ok) return room;
    }
    uint32_t     slot = no_slot;
    const Status kept = objects_.insert(id, Entry{object, nullptr, views_.size(), kind}, slot);
    if (kept != Status::ok || !view) return kept;
    return views_.append(slot);
}

Status D3dObjectTable::keep(uint32_t id, D3dObjectKind kind, IUnknown *made)
{
    AV_SCOPE("D3dObjectTable::keep");
    if (made == nullptr) return Status::invalid_parameter;
    const Status kept = insert(id, kind, made);
    if (kept != Status::ok) made->Release();
    return kept;
}

IUnknown *D3dObjectTable::find(uint32_t id, D3dObjectKind kind) const
{
    AV_SCOPE("D3dObjectTable::find");
    const Entry *const entry = id == 0u ? nullptr : objects_.find(id);
    return entry == nullptr || entry->kind_ != kind ? nullptr : entry->object_;
}

IUnknown *D3dObjectTable::find_any(uint32_t id, D3dObjectKind &kind) const
{
    AV_SCOPE("D3dObjectTable::find_any");
    const Entry *const entry = id == 0u ? nullptr : objects_.find(id);
    kind = entry == nullptr ? D3dObjectKind::none : entry->kind_;
    return entry == nullptr ? nullptr : entry->object_;
}

IUnknown *D3dObjectTable::find_view(uint32_t id) const
{
    AV_SCOPE("D3dObjectTable::find_view");
    const Entry *const entry = id == 0u ? nullptr : objects_.find(id);
    return entry == nullptr || !view_kind(entry->kind_) ? nullptr : entry->object_;
}

IUnknown *D3dObjectTable::exchange(uint32_t id, D3dObjectKind kind, IUnknown *object)
{
    AV_SCOPE("D3dObjectTable::exchange");
    Entry *const entry = id == 0u || object == nullptr ? nullptr : objects_.find(id);
    if (entry == nullptr || entry->kind_ != kind) return nullptr;
    IUnknown *const old = entry->object_;
    entry->object_ = object;
    entry->viewed_ = nullptr;
    return old;
}

uint32_t D3dObjectTable::next_used(uint32_t from) const
{
    AV_SCOPE("D3dObjectTable::next_used");
    return objects_.next_live(from);
}

uint32_t D3dObjectTable::views() const
{
    AV_SCOPE("D3dObjectTable::views");
    return views_.size();
}

uint32_t D3dObjectTable::view_slot(uint32_t index) const
{
    AV_SCOPE("D3dObjectTable::view_slot");
    return index < views_.size() ? views_.at(index) : no_slot;
}

IUnknown *D3dObjectTable::viewed(uint32_t slot) const
{
    AV_SCOPE("D3dObjectTable::viewed");
    const Entry *const entry = objects_.at(slot);
    return entry != nullptr ? entry->viewed_ : nullptr;
}

void D3dObjectTable::note_viewed(uint32_t slot, IUnknown *resource)
{
    AV_SCOPE("D3dObjectTable::note_viewed");
    Entry *const entry = objects_.at(slot);
    if (entry != nullptr) entry->viewed_ = resource;
}

IUnknown *D3dObjectTable::at(uint32_t slot, uint32_t &id, D3dObjectKind &kind) const
{
    AV_SCOPE("D3dObjectTable::at");
    const Entry *const entry = objects_.at(slot);
    id = entry != nullptr ? objects_.key_at(slot) : 0u;
    kind = entry != nullptr ? entry->kind_ : D3dObjectKind::none;
    return entry != nullptr ? entry->object_ : nullptr;
}

void D3dObjectTable::forget(uint32_t slot, const Entry &entry)
{
    AV_SCOPE("D3dObjectTable::forget");
    if (view_kind(entry.kind_)) {
        const uint32_t moved = views_.at(views_.size() - 1u);
        views_.remove_swap(entry.view_at_);
        if (moved != slot) objects_.at(moved)->view_at_ = entry.view_at_;
    }
    objects_.erase_slot(slot);
}

void D3dObjectTable::remove(uint32_t id)
{
    AV_SCOPE("D3dObjectTable::remove");
    const uint32_t slot = id == 0u ? no_slot : objects_.slot_of(id);
    if (slot == no_slot) return;
    const Entry entry = *objects_.at(slot);
    entry.object_->Release();
    forget(slot, entry);
}

void D3dObjectTable::remove_kinds(D3dObjectKind first, D3dObjectKind last)
{
    AV_SCOPE("D3dObjectTable::remove_kinds");
    for (uint32_t slot = next_used(0u); slot != no_slot; slot = next_used(slot + 1u)) {
        const Entry entry = *objects_.at(slot);
        if (entry.kind_ < first || entry.kind_ > last) continue;
        entry.object_->Release();
        forget(slot, entry);
    }
}

uint32_t D3dObjectTable::count_kinds(D3dObjectKind first, D3dObjectKind last) const
{
    AV_SCOPE("D3dObjectTable::count_kinds");
    uint32_t kept = 0u;
    objects_.for_each([&kept, first, last](uint32_t, const Entry &entry) {
        if (entry.kind_ >= first && entry.kind_ <= last) kept++;
    });
    return kept;
}

void D3dObjectTable::clear()
{
    AV_SCOPE("D3dObjectTable::clear");
    objects_.for_each([](uint32_t, Entry &entry) { entry.object_->Release(); });
    objects_.clear();
    views_.clear();
}

uint32_t D3dObjectTable::count() const
{
    AV_SCOPE("D3dObjectTable::count");
    return objects_.count();
}

/* Every entry holds an object of a kind; every view is listed once, where
 * its entry says, and nothing else is. */
bool D3dObjectTable::invariants_hold() const
{
    AV_SCOPE("D3dObjectTable::invariants_hold");
    if (!objects_.invariants_hold() || !views_.invariants_hold()) return false;
    uint32_t listed = 0u;
    bool     holds = true;
    objects_.for_each([&](uint32_t id, const Entry &entry) {
        holds = holds && id != 0u && entry.object_ != nullptr && entry.kind_ != D3dObjectKind::none;
        if (!view_kind(entry.kind_)) return;
        listed++;
        holds = holds && entry.view_at_ < views_.size() && views_.at(entry.view_at_) == objects_.slot_of(id);
    });
    return holds && listed == views_.size();
}

}  // namespace av
