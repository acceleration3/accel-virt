/* The host objects one session's guest may name, by the number the guest
 * names each with: what every backend's decoder asks before it hands a
 * number to the host's driver as an object.
 *
 * A number from the guest is an attacker's (constitution V). The host's
 * driver dereferences whatever it is handed - a number a guest invented, an
 * object of one kind named where another belongs, one already destroyed - so
 * a backend accepts a number only where it made an object of the kind the
 * command wants, and a number that names none is a command the host refuses
 * (feature 044). Who chose the number differs - Vulkan's and OpenCL's are the
 * driver's own, answered to the guest; Direct3D's are the guest's, named in
 * the batch that creates the object - and the bookkeeping does not.
 *
 * Each entry is an object of a kind, with the references the guest holds on
 * it, what it was made through, and whatever else its API keeps with it
 * (Detail). An object made through another ends with it: a driver that
 * destroys a Vulkan device frees the device's buffers, and a buffer still
 * held here would be a freed pointer the guest could name. References are
 * counted where the API counts them - an OpenCL application that retains a
 * context and releases it once has not given it up - and a count saturates
 * rather than wraps, after which it no longer says how many and is never
 * counted down to a forget the driver may not have done.
 *
 * What the guest never destroyed is the session's to release as it ends, in
 * an order its API decides - dependents before what they depend on - through
 * whatever its API releases with. The registry calls the release; what it
 * does is the API's.
 *
 * The entries are KeyedSlots, so there is no count past which an object is
 * refused, and an entry keeps its slot while it is held. Detail is copied
 * bytewise. The registry takes no lock: a session runs one command at a
 * time on its own worker. */
#ifndef AV_HOST_OBJECT_REGISTRY_HPP
#define AV_HOST_OBJECT_REGISTRY_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"

#include <stdint.h>

namespace av {

class BlockSource;
struct HostObjectRegistryProbe;

template <typename Key, typename Kind, typename Detail>
class HostObjectRegistry {
    struct Entry {
        Detail   detail_;
        /* The slot of what it was made through; no_slot for nothing. */
        uint32_t parent_;
        /* How many entries name this one as their parent, so forgetting a
         * leaf - which most objects are - looks at nothing else. */
        uint32_t children_;
        uint32_t references_;
        Kind     kind_;
    };

public:
    static constexpr uint32_t no_slot = KeyedSlots<Key, Entry>::no_slot;
    /* A count that has stopped counting. */
    static constexpr uint32_t saturated = 0xFFFFFFFFu;

    explicit HostObjectRegistry(BlockSource &blocks);
    HostObjectRegistry(const HostObjectRegistry &) = delete;
    HostObjectRegistry &operator=(const HostObjectRegistry &) = delete;
    HostObjectRegistry(HostObjectRegistry &&) = delete;
    HostObjectRegistry &operator=(HostObjectRegistry &&) = delete;

    /* Room for `objects` more, so remembering that many cannot fail for want
     * of memory: asked before the driver makes them, so a guest that wants
     * more than the host can hold is told it is out of memory rather than
     * handed an object this host would refuse. */
    [[nodiscard]] Status make_room(uint64_t objects);
    /* `key` held as `kind` with one reference and `detail`, made through
     * `parent` - nothing when 0 or a key not held. invalid_parameter for key
     * 0, which no object is; conflict for a key already held, as whatever
     * kind, with nothing changed; no_memory when no room was made and none
     * can be. */
    [[nodiscard]] Status remember(Key key, Kind kind, Key parent, const Detail &detail);
    bool                 holds(Key key) const;
    /* What `key` was made through; 0 for a key not held, or made through
     * nothing. */
    Key                  parent_of(Key key) const;
    /* What `key` is held with if it is held as `kind`; null otherwise - for
     * 0, a key never held, and one held as another kind. */
    Detail              *find(Key key, Kind kind);
    const Detail        *find(Key key, Kind kind) const;
    /* What `key` is held with and its kind, whatever it is; null for a key
     * not held. */
    Detail              *find_any(Key key, Kind &kind);
    const Detail        *find_any(Key key, Kind &kind) const;
    /* One more reference on `key` held as `kind`; nothing otherwise. */
    void                 retain(Key key, Kind kind);
    /* One reference fewer; the last forgets `key` as forget does. True when
     * it was the last. Nothing for a key not held as `kind`, or whose count
     * has saturated. */
    bool                 release(Key key, Kind kind);
    /* `key` and everything made through it, at any depth, forgotten; nothing
     * for a key not held. */
    void                 forget(Key key);
    /* Everything made through `key`, at any depth, forgotten, and not `key`. */
    void                 forget_children(Key key);
    /* Every entry of a kind from `first` to `last` to `let_go(key, kind,
     * detail, times)` - `times` the references the guest holds, once for a
     * saturated count - and then forgotten as forget does. `let_go` releases
     * the host's object and leaves this registry alone. */
    template <typename Release>
    void                 release_kinds(Kind first, Kind last, Release let_go);
    /* Every entry of each kind in `order`, in that order, to `let_go` as
     * release_kinds hands them; then everything forgotten, entries of kinds
     * not in `order` unreleased, and the memory given back. */
    template <typename Release>
    void                 release_all(const Kind *order, uint32_t kinds, Release let_go);
    /* Everything forgotten unreleased, and the memory given back. */
    void                 forget_all();

    /* The slot `key` is in; no_slot for none. */
    uint32_t             slot_of(Key key) const;
    /* The first slot at or after `from` an entry is in; no_slot for none. */
    uint32_t             next_held(uint32_t from) const;
    /* What the entry in `slot` is held with, its key and its kind; null, key
     * 0, for a slot holding nothing. */
    Detail              *at(uint32_t slot, Key &key, Kind &kind);
    const Detail        *at(uint32_t slot, Key &key, Kind &kind) const;
    uint32_t             count() const;
    /* How many entries are of a kind from `first` to `last`. */
    uint32_t             count_kinds(Kind first, Kind last) const;

    /* No key is 0 and no count 0; every parent is a live slot other than its
     * own, and the children each claims are the entries naming it. */
    bool invariants_hold() const;

private:
    void forget_slot(uint32_t slot);
    /* Forgets every entry whose parent is gone, until none is. */
    void sweep_orphans();
    /* The times a release hands back for `references`. */
    uint32_t times_of(uint32_t references) const;

    KeyedSlots<Key, Entry> entries_;

    friend struct HostObjectRegistryProbe;
};

template <typename Key, typename Kind, typename Detail>
HostObjectRegistry<Key, Kind, Detail>::HostObjectRegistry(BlockSource &blocks) : entries_(blocks, Pool::paged)
{
    AV_SCOPE("HostObjectRegistry::HostObjectRegistry");
}

template <typename Key, typename Kind, typename Detail>
Status HostObjectRegistry<Key, Kind, Detail>::make_room(uint64_t objects)
{
    AV_SCOPE("HostObjectRegistry::make_room");
    const uint64_t wanted = uint64_t{entries_.count()} + objects;
    if (wanted > KeyedSlots<Key, Entry>::most_entries) return Status::no_memory;
    return entries_.reserve(static_cast<uint32_t>(wanted));
}

template <typename Key, typename Kind, typename Detail>
Status HostObjectRegistry<Key, Kind, Detail>::remember(Key key, Kind kind, Key parent, const Detail &detail)
{
    AV_SCOPE("HostObjectRegistry::remember");
    if (key == Key(0)) return Status::invalid_parameter;
    const uint32_t parent_slot = parent != Key(0) ? entries_.slot_of(parent) : no_slot;
    uint32_t       slot = no_slot;
    const Status   inserted = entries_.insert(key, Entry{detail, parent_slot, 0u, 1u, kind}, slot);
    if (inserted != Status::ok) return inserted;
    if (parent_slot != no_slot) entries_.at(parent_slot)->children_++;
    return Status::ok;
}

template <typename Key, typename Kind, typename Detail>
bool HostObjectRegistry<Key, Kind, Detail>::holds(Key key) const
{
    AV_SCOPE("HostObjectRegistry::holds");
    return key != Key(0) && entries_.find(key) != nullptr;
}

template <typename Key, typename Kind, typename Detail>
Key HostObjectRegistry<Key, Kind, Detail>::parent_of(Key key) const
{
    AV_SCOPE("HostObjectRegistry::parent_of");
    const Entry *const entry = key != Key(0) ? entries_.find(key) : nullptr;
    return entry != nullptr && entry->parent_ != no_slot ? entries_.key_at(entry->parent_) : Key(0);
}

template <typename Key, typename Kind, typename Detail>
Detail *HostObjectRegistry<Key, Kind, Detail>::find(Key key, Kind kind)
{
    AV_SCOPE("HostObjectRegistry::find");
    Entry *const entry = key != Key(0) ? entries_.find(key) : nullptr;
    return entry != nullptr && entry->kind_ == kind ? &entry->detail_ : nullptr;
}

template <typename Key, typename Kind, typename Detail>
const Detail *HostObjectRegistry<Key, Kind, Detail>::find(Key key, Kind kind) const
{
    AV_SCOPE("HostObjectRegistry::find");
    const Entry *const entry = key != Key(0) ? entries_.find(key) : nullptr;
    return entry != nullptr && entry->kind_ == kind ? &entry->detail_ : nullptr;
}

template <typename Key, typename Kind, typename Detail>
Detail *HostObjectRegistry<Key, Kind, Detail>::find_any(Key key, Kind &kind)
{
    AV_SCOPE("HostObjectRegistry::find_any");
    Entry *const entry = key != Key(0) ? entries_.find(key) : nullptr;
    if (entry == nullptr) return nullptr;
    kind = entry->kind_;
    return &entry->detail_;
}

template <typename Key, typename Kind, typename Detail>
const Detail *HostObjectRegistry<Key, Kind, Detail>::find_any(Key key, Kind &kind) const
{
    AV_SCOPE("HostObjectRegistry::find_any");
    const Entry *const entry = key != Key(0) ? entries_.find(key) : nullptr;
    if (entry == nullptr) return nullptr;
    kind = entry->kind_;
    return &entry->detail_;
}

template <typename Key, typename Kind, typename Detail>
void HostObjectRegistry<Key, Kind, Detail>::retain(Key key, Kind kind)
{
    AV_SCOPE("HostObjectRegistry::retain");
    Entry *const entry = key != Key(0) ? entries_.find(key) : nullptr;
    /* Saturated rather than wrapped: a count that wrapped to zero would be an
     * entry no release could take out. */
    if (entry != nullptr && entry->kind_ == kind && entry->references_ != saturated) entry->references_++;
}

template <typename Key, typename Kind, typename Detail>
bool HostObjectRegistry<Key, Kind, Detail>::release(Key key, Kind kind)
{
    AV_SCOPE("HostObjectRegistry::release");
    const uint32_t slot = key != Key(0) ? entries_.slot_of(key) : no_slot;
    Entry *const   entry = entries_.at(slot);
    if (entry == nullptr || entry->kind_ != kind || entry->references_ == saturated) return false;
    if (--entry->references_ != 0u) return false;
    const bool had_children = entry->children_ != 0u;
    forget_slot(slot);
    if (had_children) sweep_orphans();
    return true;
}

template <typename Key, typename Kind, typename Detail>
void HostObjectRegistry<Key, Kind, Detail>::forget(Key key)
{
    AV_SCOPE("HostObjectRegistry::forget");
    const uint32_t slot = key != Key(0) ? entries_.slot_of(key) : no_slot;
    if (slot == no_slot) return;
    const bool had_children = entries_.at(slot)->children_ != 0u;
    forget_slot(slot);
    if (had_children) sweep_orphans();
}

template <typename Key, typename Kind, typename Detail>
void HostObjectRegistry<Key, Kind, Detail>::forget_children(Key key)
{
    AV_SCOPE("HostObjectRegistry::forget_children");
    const uint32_t slot = key != Key(0) ? entries_.slot_of(key) : no_slot;
    if (slot == no_slot || entries_.at(slot)->children_ == 0u) return;
    bool grandchildren = false;
    for (uint32_t child = entries_.next_live(0u); child != no_slot && entries_.at(slot)->children_ != 0u;
         child = entries_.next_live(child + 1u)) {
        const Entry &made = *entries_.at(child);
        if (made.parent_ != slot) continue;
        grandchildren = grandchildren || made.children_ != 0u;
        forget_slot(child);
    }
    if (grandchildren) sweep_orphans();
}

template <typename Key, typename Kind, typename Detail>
template <typename Release>
void HostObjectRegistry<Key, Kind, Detail>::release_kinds(Kind first, Kind last, Release let_go)
{
    AV_SCOPE("HostObjectRegistry::release_kinds");
    bool orphans = false;
    for (uint32_t slot = entries_.next_live(0u); slot != no_slot; slot = entries_.next_live(slot + 1u)) {
        Entry &entry = *entries_.at(slot);
        if (entry.kind_ < first || entry.kind_ > last) continue;
        let_go(entries_.key_at(slot), entry.kind_, entry.detail_, times_of(entry.references_));
        orphans = orphans || entry.children_ != 0u;
        forget_slot(slot);
    }
    if (orphans) sweep_orphans();
}

template <typename Key, typename Kind, typename Detail>
template <typename Release>
void HostObjectRegistry<Key, Kind, Detail>::release_all(const Kind *order, uint32_t kinds, Release let_go)
{
    AV_SCOPE("HostObjectRegistry::release_all");
    for (uint32_t index = 0u; index < kinds; index++) {
        entries_.for_each([&](Key key, Entry &entry) {
            if (entry.kind_ == order[index]) let_go(key, entry.kind_, entry.detail_, times_of(entry.references_));
        });
    }
    forget_all();
}

template <typename Key, typename Kind, typename Detail>
void HostObjectRegistry<Key, Kind, Detail>::forget_all()
{
    AV_SCOPE("HostObjectRegistry::forget_all");
    entries_.clear();
}

template <typename Key, typename Kind, typename Detail>
uint32_t HostObjectRegistry<Key, Kind, Detail>::slot_of(Key key) const
{
    AV_SCOPE("HostObjectRegistry::slot_of");
    return key != Key(0) ? entries_.slot_of(key) : no_slot;
}

template <typename Key, typename Kind, typename Detail>
uint32_t HostObjectRegistry<Key, Kind, Detail>::next_held(uint32_t from) const
{
    AV_SCOPE("HostObjectRegistry::next_held");
    return entries_.next_live(from);
}

template <typename Key, typename Kind, typename Detail>
Detail *HostObjectRegistry<Key, Kind, Detail>::at(uint32_t slot, Key &key, Kind &kind)
{
    AV_SCOPE("HostObjectRegistry::at");
    Entry *const entry = entries_.at(slot);
    key = entries_.key_at(slot);
    if (entry == nullptr) return nullptr;
    kind = entry->kind_;
    return &entry->detail_;
}

template <typename Key, typename Kind, typename Detail>
const Detail *HostObjectRegistry<Key, Kind, Detail>::at(uint32_t slot, Key &key, Kind &kind) const
{
    AV_SCOPE("HostObjectRegistry::at");
    const Entry *const entry = entries_.at(slot);
    key = entries_.key_at(slot);
    if (entry == nullptr) return nullptr;
    kind = entry->kind_;
    return &entry->detail_;
}

template <typename Key, typename Kind, typename Detail>
uint32_t HostObjectRegistry<Key, Kind, Detail>::count() const
{
    AV_SCOPE("HostObjectRegistry::count");
    return entries_.count();
}

template <typename Key, typename Kind, typename Detail>
uint32_t HostObjectRegistry<Key, Kind, Detail>::count_kinds(Kind first, Kind last) const
{
    AV_SCOPE("HostObjectRegistry::count_kinds");
    uint32_t counted = 0u;
    entries_.for_each([&counted, first, last](Key, const Entry &entry) {
        if (entry.kind_ >= first && entry.kind_ <= last) counted++;
    });
    return counted;
}

template <typename Key, typename Kind, typename Detail>
void HostObjectRegistry<Key, Kind, Detail>::forget_slot(uint32_t slot)
{
    AV_SCOPE("HostObjectRegistry::forget_slot");
    const uint32_t parent = entries_.at(slot)->parent_;
    if (parent != no_slot && entries_.at(parent) != nullptr) entries_.at(parent)->children_--;
    entries_.erase_slot(slot);
}

template <typename Key, typename Kind, typename Detail>
void HostObjectRegistry<Key, Kind, Detail>::sweep_orphans()
{
    AV_SCOPE("HostObjectRegistry::sweep_orphans");
    bool forgot = true;
    while (forgot) {
        forgot = false;
        for (uint32_t slot = entries_.next_live(0u); slot != no_slot; slot = entries_.next_live(slot + 1u)) {
            const uint32_t parent = entries_.at(slot)->parent_;
            if (parent == no_slot || entries_.at(parent) != nullptr) continue;
            forget_slot(slot);
            forgot = true;
        }
    }
}

template <typename Key, typename Kind, typename Detail>
uint32_t HostObjectRegistry<Key, Kind, Detail>::times_of(uint32_t references) const
{
    AV_SCOPE("HostObjectRegistry::times_of");
    return references == saturated ? 1u : references;
}

template <typename Key, typename Kind, typename Detail>
bool HostObjectRegistry<Key, Kind, Detail>::invariants_hold() const
{
    AV_SCOPE("HostObjectRegistry::invariants_hold");
    if (!entries_.invariants_hold()) return false;
    uint64_t made_through = 0u;
    uint64_t claimed = 0u;
    for (uint32_t slot = entries_.next_live(0u); slot != no_slot; slot = entries_.next_live(slot + 1u)) {
        const Entry &held = *entries_.at(slot);
        if (entries_.key_at(slot) == Key(0) || held.references_ == 0u) return false;
        claimed += held.children_;
        if (held.parent_ == no_slot) continue;
        if (held.parent_ == slot || entries_.at(held.parent_) == nullptr) return false;
        made_through++;
    }
    return claimed == made_through;
}

}  // namespace av

#endif  // AV_HOST_OBJECT_REGISTRY_HPP
