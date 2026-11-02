#include "executor/d3d/d3d_object_table.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

namespace {

bool view_kind(D3dObjectKind kind)
{
    AV_SCOPE("view_kind");
    return kind == D3dObjectKind::shader_resource_view || kind == D3dObjectKind::render_target_view ||
           kind == D3dObjectKind::depth_stencil_view || kind == D3dObjectKind::unordered_access_view;
}

}

D3dObjectTable::D3dObjectTable(BlockSource &blocks) : objects_(blocks), views_(blocks, Pool::paged)
{
    AV_SCOPE("D3dObjectTable::D3dObjectTable");
    static_assert(no_slot == HostObjectRegistry<uint32_t, D3dObjectKind, Entry>::no_slot, "one sentinel for no slot");
}

D3dObjectTable::~D3dObjectTable()
{
    AV_SCOPE("D3dObjectTable::~D3dObjectTable");
}

Status D3dObjectTable::insert(uint32_t id, D3dObjectKind kind, IUnknown *object)
{
    AV_SCOPE("D3dObjectTable::insert");
    if (id == 0u || kind == D3dObjectKind::none || object == nullptr) return Status::invalid_parameter;
    if (objects_.holds(id)) return Status::conflict;
    const bool view = view_kind(kind);
    if (view) {
        const Status room = views_.reserve(views_.size() + 1u);
        if (room != Status::ok) return room;
    }
    const Status kept = objects_.remember(id, kind, 0u, Entry{object, nullptr, views_.size()});
    if (kept != Status::ok || !view) return kept;
    return views_.append(objects_.slot_of(id));
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
    const Entry *const entry = objects_.find(id, kind);
    return entry != nullptr ? entry->object_ : nullptr;
}

IUnknown *D3dObjectTable::find_any(uint32_t id, D3dObjectKind &kind) const
{
    AV_SCOPE("D3dObjectTable::find_any");
    kind = D3dObjectKind::none;
    const Entry *const entry = objects_.find_any(id, kind);
    return entry != nullptr ? entry->object_ : nullptr;
}

IUnknown *D3dObjectTable::find_view(uint32_t id) const
{
    AV_SCOPE("D3dObjectTable::find_view");
    D3dObjectKind      kind = D3dObjectKind::none;
    const Entry *const entry = objects_.find_any(id, kind);
    return entry != nullptr && view_kind(kind) ? entry->object_ : nullptr;
}

IUnknown *D3dObjectTable::exchange(uint32_t id, D3dObjectKind kind, IUnknown *object)
{
    AV_SCOPE("D3dObjectTable::exchange");
    Entry *const entry = object != nullptr ? objects_.find(id, kind) : nullptr;
    if (entry == nullptr) return nullptr;
    IUnknown *const old = entry->object_;
    entry->object_ = object;
    entry->viewed_ = nullptr;
    return old;
}

uint32_t D3dObjectTable::next_used(uint32_t from) const
{
    AV_SCOPE("D3dObjectTable::next_used");
    return objects_.next_held(from);
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
    uint32_t           id = 0u;
    D3dObjectKind      kind = D3dObjectKind::none;
    const Entry *const entry = objects_.at(slot, id, kind);
    return entry != nullptr ? entry->viewed_ : nullptr;
}

void D3dObjectTable::note_viewed(uint32_t slot, IUnknown *resource)
{
    AV_SCOPE("D3dObjectTable::note_viewed");
    uint32_t      id = 0u;
    D3dObjectKind kind = D3dObjectKind::none;
    Entry *const  entry = objects_.at(slot, id, kind);
    if (entry != nullptr) entry->viewed_ = resource;
}

IUnknown *D3dObjectTable::at(uint32_t slot, uint32_t &id, D3dObjectKind &kind) const
{
    AV_SCOPE("D3dObjectTable::at");
    kind = D3dObjectKind::none;
    const Entry *const entry = objects_.at(slot, id, kind);
    return entry != nullptr ? entry->object_ : nullptr;
}

void D3dObjectTable::let_go(D3dObjectKind kind, const Entry &entry)
{
    AV_SCOPE("D3dObjectTable::let_go");
    entry.object_->Release();
    if (!view_kind(kind)) return;
    const uint32_t moved = views_.at(views_.size() - 1u);
    views_.remove_swap(entry.view_at_);
    uint32_t      id = 0u;
    D3dObjectKind moved_kind = D3dObjectKind::none;
    Entry *const  listed = objects_.at(moved, id, moved_kind);
    if (listed != &entry) listed->view_at_ = entry.view_at_;
}

void D3dObjectTable::remove(uint32_t id)
{
    AV_SCOPE("D3dObjectTable::remove");
    D3dObjectKind      kind = D3dObjectKind::none;
    const Entry *const entry = objects_.find_any(id, kind);
    if (entry == nullptr) return;
    let_go(kind, *entry);
    objects_.forget(id);
}

void D3dObjectTable::remove_kinds(D3dObjectKind first, D3dObjectKind last)
{
    AV_SCOPE("D3dObjectTable::remove_kinds");
    objects_.release_kinds(first, last,
                           [this](uint32_t, D3dObjectKind kind, Entry &entry, uint32_t) { let_go(kind, entry); });
}

uint32_t D3dObjectTable::count_kinds(D3dObjectKind first, D3dObjectKind last) const
{
    AV_SCOPE("D3dObjectTable::count_kinds");
    return objects_.count_kinds(first, last);
}

void D3dObjectTable::release_all()
{
    AV_SCOPE("D3dObjectTable::release_all");
    remove_kinds(D3dObjectKind::resource, D3dObjectKind::query9);
    objects_.forget_all();
    views_.clear();
}

uint32_t D3dObjectTable::count() const
{
    AV_SCOPE("D3dObjectTable::count");
    return objects_.count();
}

bool D3dObjectTable::invariants_hold() const
{
    AV_SCOPE("D3dObjectTable::invariants_hold");
    if (!objects_.invariants_hold() || !views_.invariants_hold()) return false;
    uint32_t listed = 0u;
    for (uint32_t slot = objects_.next_held(0u); slot != no_slot; slot = objects_.next_held(slot + 1u)) {
        uint32_t           id = 0u;
        D3dObjectKind      kind = D3dObjectKind::none;
        const Entry *const entry = objects_.at(slot, id, kind);
        if (entry->object_ == nullptr || kind == D3dObjectKind::none) return false;
        if (!view_kind(kind)) continue;
        listed++;
        if (entry->view_at_ >= views_.size() || views_.at(entry->view_at_) != slot) return false;
    }
    return listed == views_.size();
}

}
