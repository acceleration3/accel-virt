/* Values by an integer id whose count has no fixed bound - the objects a guest
 * named, the handles it holds, the ids it shares - in an open-addressed table
 * that grows as it fills, so a lookup is one short probe whatever the count.
 *
 * Ids come from outside and are any value, 0 and all-ones included: an
 * entry's state, not a reserved id, says whether it is empty, live or removed.
 * A removal leaves a tombstone that later probes walk past; the table is
 * rebuilt - tombstones dropped, doubled only when a quarter of it is live -
 * once live entries and tombstones reach half of it, so a probe always ends
 * at an empty entry and a guest that makes and removes ids for ever never
 * grows it. Ids are mixed before they pick an entry: a guest's are small and
 * sequential, or strided, and would otherwise fill one run of the table.
 *
 * A value's address is good only until the next insert or reserve, which may
 * move every entry; a caller whose value is an object keeps the object in an
 * ObjectPool and its pointer here. Values are copied bytewise, so Value is
 * trivially copyable.
 *
 * The map takes no lock; its owner serializes it. Growing allocates, so
 * insert and reserve are legal up to APC; finding is legal anywhere. */
#ifndef AV_ID_MAP_HPP
#define AV_ID_MAP_HPP

#include "core/platform/block_source.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct IdMapProbe;

template <typename Key, typename Value>
class IdMap {
    static_assert(Key(0) < Key(-1) && sizeof(Key) <= sizeof(uint64_t), "an id is an unsigned integer");
    static_assert(__is_trivially_copyable(Value), "IdMap moves its values bytewise");

    enum class State : uint8_t { empty, live, removed };

    struct Entry {
        Key   key_;
        State state_;
        Value value_;
    };

public:
    /* The first table's entries, and the least a rebuilt one has. */
    static constexpr uint32_t first_capacity = 16u;
    /* The most entries a table may have: a power of two whose bytes a size_t
     * still counts. A quarter of it can be live after a rebuild. */
    static constexpr uint32_t most_entries =
        size_t{0x40000000u} <= ~size_t{0} / sizeof(Entry)
            ? 0x40000000u
            : uint32_t{1} << (63 - __builtin_clzll(static_cast<unsigned long long>(~size_t{0} / sizeof(Entry))));

    IdMap(BlockSource &blocks, Pool pool);
    ~IdMap();
    IdMap(const IdMap &) = delete;
    IdMap &operator=(const IdMap &) = delete;
    IdMap(IdMap &&) = delete;
    IdMap &operator=(IdMap &&) = delete;

    /* The value `key` names, or null. */
    Value                *find(Key key);
    const Value          *find(Key key) const;
    /* `value` under `key`: conflict when `key` already names one, no_memory
     * when the table cannot grow. Nothing changes unless ok. */
    [[nodiscard]] Status  insert(Key key, const Value &value);
    /* Room for `count` live entries, so inserts up to it cannot fail: what a
     * caller does before work it cannot undo. */
    [[nodiscard]] Status  reserve(uint32_t count);
    /* `key`'s value into `value` and the entry gone; false, `value`
     * untouched, when `key` names nothing. */
    bool                  take(Key key, Value &value);
    /* `key` names nothing more; false when it named nothing. */
    bool                  remove(Key key);
    /* Every live entry to `visit(key, value)`, in table order. */
    template <typename Visitor>
    void                  for_each(Visitor visit);
    template <typename Visitor>
    void                  for_each(Visitor visit) const;
    /* Every entry `matches(key, value)` answers true for gone - `matches`
     * may let go of what the value holds first; how many went. */
    template <typename Predicate>
    uint32_t              remove_if(Predicate matches);
    /* Every entry gone and the table given back. */
    void                  clear();
    uint32_t              count() const;
    uint32_t              capacity() const;

    bool invariants_hold() const;

private:
    uint32_t start_of(Key key) const;
    /* The live entry `key` is in, or null. */
    Entry   *live_entry(Key key) const;
    /* The first entry `key` could go in: a tombstone or an empty one. */
    Entry   *free_entry(Key key) const;
    /* `entry` a tombstone. */
    void     retire(Entry &entry);
    /* Every live entry moved to a fresh table with room for `count` live
     * ones at a quarter full; tombstones are left behind. */
    [[nodiscard]] Status rebuild(uint32_t count);

    BlockSource &blocks_;
    Pool         pool_;
    Entry       *entries_;
    uint32_t     capacity_;
    uint32_t     count_;
    uint32_t     removed_;

    friend struct IdMapProbe;
};

template <typename Key, typename Value>
IdMap<Key, Value>::IdMap(BlockSource &blocks, Pool pool)
    : blocks_(blocks), pool_(pool), entries_(nullptr), capacity_(0u), count_(0u), removed_(0u)
{
    AV_SCOPE("IdMap::IdMap");
}

template <typename Key, typename Value>
IdMap<Key, Value>::~IdMap()
{
    AV_SCOPE("IdMap::~IdMap");
    clear();
}

/* splitmix64's finaliser. */
template <typename Key, typename Value>
uint32_t IdMap<Key, Value>::start_of(Key key) const
{
    AV_SCOPE("IdMap::start_of");
    uint64_t mixed = static_cast<uint64_t>(key);
    mixed ^= mixed >> 30u;
    mixed *= 0xBF58476D1CE4E5B9u;
    mixed ^= mixed >> 27u;
    mixed *= 0x94D049BB133111EBu;
    mixed ^= mixed >> 31u;
    return static_cast<uint32_t>(mixed) & (capacity_ - 1u);
}

template <typename Key, typename Value>
typename IdMap<Key, Value>::Entry *IdMap<Key, Value>::live_entry(Key key) const
{
    AV_SCOPE("IdMap::live_entry");
    if (entries_ == nullptr) return nullptr;
    for (uint32_t index = start_of(key);; index = (index + 1u) & (capacity_ - 1u)) {
        Entry &entry = entries_[index];
        if (entry.state_ == State::empty) return nullptr;
        if (entry.state_ == State::live && entry.key_ == key) return &entry;
    }
}

template <typename Key, typename Value>
typename IdMap<Key, Value>::Entry *IdMap<Key, Value>::free_entry(Key key) const
{
    AV_SCOPE("IdMap::free_entry");
    for (uint32_t index = start_of(key);; index = (index + 1u) & (capacity_ - 1u))
        if (entries_[index].state_ != State::live) return &entries_[index];
}

template <typename Key, typename Value>
Status IdMap<Key, Value>::rebuild(uint32_t count)
{
    AV_SCOPE("IdMap::rebuild");
    if (count > most_entries / 4u) return Status::no_memory;
    uint32_t wanted = first_capacity;
    while (wanted < 4u * count) wanted *= 2u;
    auto *const rebuilt = static_cast<Entry *>(blocks_.allocate(sizeof(Entry) * size_t{wanted}, pool_));
    if (rebuilt == nullptr) return Status::no_memory;
    for (uint32_t index = 0u; index < wanted; index++) rebuilt[index].state_ = State::empty;
    Entry *const   previous = entries_;
    const uint32_t previous_capacity = capacity_;
    entries_ = rebuilt;
    capacity_ = wanted;
    removed_ = 0u;
    for (uint32_t index = 0u; index < previous_capacity; index++)
        if (previous[index].state_ == State::live) *free_entry(previous[index].key_) = previous[index];
    if (previous != nullptr) blocks_.release(previous);
    return Status::ok;
}

template <typename Key, typename Value>
Value *IdMap<Key, Value>::find(Key key)
{
    AV_SCOPE("IdMap::find");
    blocks_.require_irql_at_most(Irql::dispatch, "IdMap::find");
    Entry *const entry = live_entry(key);
    return entry != nullptr ? &entry->value_ : nullptr;
}

template <typename Key, typename Value>
const Value *IdMap<Key, Value>::find(Key key) const
{
    AV_SCOPE("IdMap::find");
    blocks_.require_irql_at_most(Irql::dispatch, "IdMap::find");
    const Entry *const entry = live_entry(key);
    return entry != nullptr ? &entry->value_ : nullptr;
}

template <typename Key, typename Value>
Status IdMap<Key, Value>::insert(Key key, const Value &value)
{
    AV_SCOPE("IdMap::insert");
    blocks_.require_irql_at_most(Irql::apc, "IdMap::insert");
    if (live_entry(key) != nullptr) return Status::conflict;
    if (2u * (count_ + removed_ + 1u) > capacity_) {
        const Status rebuilt = rebuild(count_ + 1u);
        if (rebuilt != Status::ok) return rebuilt;
    }
    Entry *const entry = free_entry(key);
    if (entry->state_ == State::removed) removed_--;
    entry->key_ = key;
    entry->state_ = State::live;
    entry->value_ = value;
    count_++;
    return Status::ok;
}

template <typename Key, typename Value>
Status IdMap<Key, Value>::reserve(uint32_t count)
{
    AV_SCOPE("IdMap::reserve");
    blocks_.require_irql_at_most(Irql::apc, "IdMap::reserve");
    if (count <= count_) return Status::ok;
    /* Inserts up to `count` add at most count - count_ to what the table
     * holds, tombstones included; rebuilt at a quarter, they never reach half. */
    if (2u * (count_ + removed_) + 2u * (count - count_) <= capacity_) return Status::ok;
    return rebuild(count);
}

template <typename Key, typename Value>
bool IdMap<Key, Value>::take(Key key, Value &value)
{
    AV_SCOPE("IdMap::take");
    blocks_.require_irql_at_most(Irql::dispatch, "IdMap::take");
    Entry *const entry = live_entry(key);
    if (entry == nullptr) return false;
    value = entry->value_;
    retire(*entry);
    return true;
}

template <typename Key, typename Value>
bool IdMap<Key, Value>::remove(Key key)
{
    AV_SCOPE("IdMap::remove");
    blocks_.require_irql_at_most(Irql::dispatch, "IdMap::remove");
    Entry *const entry = live_entry(key);
    if (entry == nullptr) return false;
    retire(*entry);
    return true;
}

template <typename Key, typename Value>
void IdMap<Key, Value>::retire(Entry &entry)
{
    AV_SCOPE("IdMap::retire");
    entry.state_ = State::removed;
    count_--;
    removed_++;
}

template <typename Key, typename Value>
template <typename Visitor>
void IdMap<Key, Value>::for_each(Visitor visit)
{
    AV_SCOPE("IdMap::for_each");
    for (uint32_t index = 0u; index < capacity_; index++)
        if (entries_[index].state_ == State::live) visit(entries_[index].key_, entries_[index].value_);
}

template <typename Key, typename Value>
template <typename Visitor>
void IdMap<Key, Value>::for_each(Visitor visit) const
{
    AV_SCOPE("IdMap::for_each");
    for (uint32_t index = 0u; index < capacity_; index++)
        if (entries_[index].state_ == State::live)
            visit(entries_[index].key_, static_cast<const Value &>(entries_[index].value_));
}

template <typename Key, typename Value>
template <typename Predicate>
uint32_t IdMap<Key, Value>::remove_if(Predicate matches)
{
    AV_SCOPE("IdMap::remove_if");
    uint32_t gone = 0u;
    for (uint32_t index = 0u; index < capacity_; index++) {
        Entry &entry = entries_[index];
        if (entry.state_ != State::live || !matches(entry.key_, entry.value_)) continue;
        retire(entry);
        gone++;
    }
    return gone;
}

template <typename Key, typename Value>
void IdMap<Key, Value>::clear()
{
    AV_SCOPE("IdMap::clear");
    blocks_.require_irql_at_most(Irql::apc, "IdMap::clear");
    if (entries_ != nullptr) blocks_.release(entries_);
    entries_ = nullptr;
    capacity_ = 0u;
    count_ = 0u;
    removed_ = 0u;
}

template <typename Key, typename Value>
uint32_t IdMap<Key, Value>::count() const
{
    AV_SCOPE("IdMap::count");
    return count_;
}

template <typename Key, typename Value>
uint32_t IdMap<Key, Value>::capacity() const
{
    AV_SCOPE("IdMap::capacity");
    return capacity_;
}

template <typename Key, typename Value>
bool IdMap<Key, Value>::invariants_hold() const
{
    AV_SCOPE("IdMap::invariants_hold");
    if (entries_ == nullptr) return capacity_ == 0u && count_ == 0u && removed_ == 0u;
    if ((capacity_ & (capacity_ - 1u)) != 0u || capacity_ < first_capacity || 2u * (count_ + removed_) > capacity_)
        return false;
    uint32_t live = 0u;
    uint32_t removed = 0u;
    for (uint32_t index = 0u; index < capacity_; index++) {
        const Entry &entry = entries_[index];
        if (entry.state_ == State::removed) removed++;
        if (entry.state_ != State::live) continue;
        live++;
        if (live_entry(entry.key_) != &entry) return false;
    }
    return live == count_ && removed == removed_;
}

}  // namespace av

#endif  // AV_ID_MAP_HPP
