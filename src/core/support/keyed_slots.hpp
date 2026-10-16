/* Entries by an integer key whose count has no fixed bound - the objects a
 * guest named, the handles it holds, the ids it shares - each in a slot of
 * its own: the storage under every keyed table, host and guest.
 *
 * An entry lives in a chunk that never moves, so its address and its slot
 * number hold from insert to erase: a caller keeps a pointer to it, or keeps
 * a side table by slot. Chunks double in size as they are added and are
 * given back only by clear; an erased slot goes on a free list and is the
 * next one used.
 *
 * Keys come from outside and are any value, 0 and all-ones included. A key
 * finds its slot through an open-addressed index of (key, slot) pairs, mixed
 * first - a guest's keys are small and sequential, or strided, and would
 * otherwise fill one run - and kept at most half full, doubling as it fills.
 * An erase shifts the entries after it back along their probe instead of
 * leaving a tombstone, so a guest that makes and erases keys for ever never
 * lengthens a probe or grows the index.
 *
 * Entries are copied in bytewise, so Entry is trivially copyable. The table
 * takes no lock; its owner serializes it. Inserting allocates, so insert and
 * reserve are legal up to APC; finding and erasing never allocate. */
#ifndef AV_KEYED_SLOTS_HPP
#define AV_KEYED_SLOTS_HPP

#include "core/platform/block_source.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct KeyedSlotsProbe;

template <typename Key, typename Entry>
class KeyedSlots {
    static_assert(Key(0) < Key(-1) && sizeof(Key) <= sizeof(uint64_t), "a key is an unsigned integer");
    static_assert(__is_trivially_copyable(Entry), "KeyedSlots copies its entries bytewise");

    struct Row {
        Entry    entry_;
        Key      key_;
        uint32_t next_free_;
        bool     live_;
    };

    struct Bucket {
        Key      key_;
        uint32_t slot_;
    };

public:
    static constexpr uint32_t no_slot = 0xFFFFFFFFu;
    /* The first chunk's rows; chunk n holds first_rows << n, so slots stay
     * dense from 0 and a slot finds its chunk by its high bit. */
    static constexpr uint32_t first_rows = 16u;
    /* Chunks, and so slots, a table may have: a chunk's bytes are counted by
     * a size_t on the 32-bit guest builds as on the host, and no slot reaches
     * no_slot. */
    static constexpr uint32_t most_chunks =
        (~size_t{0} / 4u / sizeof(Row)) / first_rows >= (size_t{1} << 26u)
            ? 27u
            : uint32_t(64 - __builtin_clzll(static_cast<unsigned long long>((~size_t{0} / 4u / sizeof(Row)) /
                                                                           first_rows)));
    static constexpr uint32_t most_slots = first_rows * ((uint32_t{1} << most_chunks) - 1u);
    /* The first index's buckets, and the least a rebuilt one has. */
    static constexpr uint32_t first_buckets = 16u;
    /* An index's buckets at most, by the same rule as a chunk's rows. */
    static constexpr uint32_t most_buckets =
        ~size_t{0} / 4u / sizeof(Bucket) >= size_t{0x80000000u}
            ? 0x80000000u
            : uint32_t{1} << (63 - __builtin_clzll(static_cast<unsigned long long>(~size_t{0} / 4u / sizeof(Bucket))));
    /* Entries a table may hold: slots for them, and an index at most half full. */
    static constexpr uint32_t most_entries = most_slots < most_buckets / 2u ? most_slots : most_buckets / 2u;

    KeyedSlots(BlockSource &blocks, Pool pool);
    ~KeyedSlots();
    KeyedSlots(const KeyedSlots &) = delete;
    KeyedSlots &operator=(const KeyedSlots &) = delete;
    KeyedSlots(KeyedSlots &&) = delete;
    KeyedSlots &operator=(KeyedSlots &&) = delete;

    /* `entry` under `key`, in the slot handed back: conflict when `key` is
     * already in one, no_memory when the table cannot grow. Nothing changes
     * unless ok. */
    [[nodiscard]] Status insert(Key key, const Entry &entry, uint32_t &slot);
    /* Room for `count` entries, so inserts up to it cannot fail: what a caller
     * does before work it cannot undo. */
    [[nodiscard]] Status reserve(uint32_t count);
    /* The entry `key` is in, or null. */
    Entry               *find(Key key);
    const Entry         *find(Key key) const;
    /* The slot `key` is in, or no_slot. */
    uint32_t             slot_of(Key key) const;
    /* The entry in `slot`, or null for any slot no entry is in - a slot from
     * outside included. */
    Entry               *at(uint32_t slot);
    const Entry         *at(uint32_t slot) const;
    /* The key in `slot`; 0 for a slot no entry is in. */
    Key                  key_at(uint32_t slot) const;
    /* `key`'s entry into `entry` and its slot freed; false, `entry` untouched,
     * when `key` is in none. */
    bool                 take(Key key, Entry &entry);
    /* `key`'s slot freed; false when it was in none. */
    bool                 erase(Key key);
    /* The entry in `slot` gone; nothing for a slot no entry is in. */
    void                 erase_slot(uint32_t slot);
    /* The first slot at or after `from` an entry is in; no_slot for none. */
    uint32_t             next_live(uint32_t from) const;
    /* Every entry to `visit(key, entry)`, in slot order. */
    template <typename Visitor>
    void                 for_each(Visitor visit);
    template <typename Visitor>
    void                 for_each(Visitor visit) const;
    /* Every entry `matches(key, entry)` answers true for gone - `matches` may
     * let go of what the entry holds first; how many went. */
    template <typename Predicate>
    uint32_t             erase_if(Predicate matches);
    /* Every entry gone, and every chunk and the index given back. */
    void                 clear();
    uint32_t             count() const;
    /* Slots ever handed out since the last clear: every live slot is below it. */
    uint32_t             slots() const;
    uint32_t             buckets() const;

    bool invariants_hold() const;

private:
    uint32_t chunk_of(uint32_t slot) const;
    Row     *row(uint32_t slot) const;
    uint32_t home_of(Key key) const;
    /* The bucket `key` is in, or the empty one its probe ends at. */
    uint32_t bucket_of(Key key) const;
    /* The bucket at `hole` emptied, the buckets after it on their probes
     * moved back into it. */
    void     unindex(uint32_t hole);
    /* An index of `buckets` with every entry in it; the old one given back. */
    [[nodiscard]] Status rebuild(uint32_t buckets);
    /* Rows enough that `count` more slots can be handed out. */
    [[nodiscard]] Status make_rows(uint32_t count);
    /* Room in the index for `count` entries at most half full. */
    [[nodiscard]] Status make_buckets(uint32_t count);
    /* Rows past slots_ in the chunks already made. */
    uint32_t spare_rows() const;
    void     free_row(uint32_t slot);

    BlockSource &blocks_;
    Pool         pool_;
    Row         *chunks_[most_chunks];
    uint32_t     made_chunks_;
    Bucket      *index_;
    uint32_t     buckets_;
    uint32_t     count_;
    uint32_t     slots_;
    uint32_t     free_head_;
    uint32_t     free_count_;

    friend struct KeyedSlotsProbe;
};

template <typename Key, typename Entry>
KeyedSlots<Key, Entry>::KeyedSlots(BlockSource &blocks, Pool pool)
    : blocks_(blocks), pool_(pool), chunks_{}, made_chunks_(0u), index_(nullptr), buckets_(0u), count_(0u),
      slots_(0u), free_head_(no_slot), free_count_(0u)
{
    AV_SCOPE("KeyedSlots::KeyedSlots");
}

template <typename Key, typename Entry>
KeyedSlots<Key, Entry>::~KeyedSlots()
{
    AV_SCOPE("KeyedSlots::~KeyedSlots");
    clear();
}

template <typename Key, typename Entry>
uint32_t KeyedSlots<Key, Entry>::chunk_of(uint32_t slot) const
{
    AV_SCOPE("KeyedSlots::chunk_of");
    return static_cast<uint32_t>(31 - __builtin_clz(slot / first_rows + 1u));
}

template <typename Key, typename Entry>
typename KeyedSlots<Key, Entry>::Row *KeyedSlots<Key, Entry>::row(uint32_t slot) const
{
    AV_SCOPE("KeyedSlots::row");
    const uint32_t chunk = chunk_of(slot);
    return &chunks_[chunk][slot - first_rows * ((uint32_t{1} << chunk) - 1u)];
}

/* splitmix64's finaliser. */
template <typename Key, typename Entry>
uint32_t KeyedSlots<Key, Entry>::home_of(Key key) const
{
    AV_SCOPE("KeyedSlots::home_of");
    uint64_t mixed = static_cast<uint64_t>(key);
    mixed ^= mixed >> 30u;
    mixed *= 0xBF58476D1CE4E5B9u;
    mixed ^= mixed >> 27u;
    mixed *= 0x94D049BB133111EBu;
    mixed ^= mixed >> 31u;
    return static_cast<uint32_t>(mixed) & (buckets_ - 1u);
}

template <typename Key, typename Entry>
uint32_t KeyedSlots<Key, Entry>::bucket_of(Key key) const
{
    AV_SCOPE("KeyedSlots::bucket_of");
    uint32_t bucket = home_of(key);
    while (index_[bucket].slot_ != no_slot && index_[bucket].key_ != key) bucket = (bucket + 1u) & (buckets_ - 1u);
    return bucket;
}

template <typename Key, typename Entry>
void KeyedSlots<Key, Entry>::unindex(uint32_t hole)
{
    AV_SCOPE("KeyedSlots::unindex");
    const uint32_t mask = buckets_ - 1u;
    for (uint32_t next = (hole + 1u) & mask; index_[next].slot_ != no_slot; next = (next + 1u) & mask) {
        /* A pair moves back into the hole only if the hole is on its probe:
         * between its home and where it is now. */
        const uint32_t home = home_of(index_[next].key_);
        if (((next - home) & mask) < ((next - hole) & mask)) continue;
        index_[hole] = index_[next];
        hole = next;
    }
    index_[hole].slot_ = no_slot;
}

template <typename Key, typename Entry>
Status KeyedSlots<Key, Entry>::rebuild(uint32_t buckets)
{
    AV_SCOPE("KeyedSlots::rebuild");
    auto *const rebuilt = static_cast<Bucket *>(blocks_.allocate(sizeof(Bucket) * size_t{buckets}, pool_));
    if (rebuilt == nullptr) return Status::no_memory;
    for (uint32_t bucket = 0u; bucket < buckets; bucket++) rebuilt[bucket].slot_ = no_slot;
    Bucket *const  previous = index_;
    const uint32_t previous_buckets = buckets_;
    index_ = rebuilt;
    buckets_ = buckets;
    for (uint32_t bucket = 0u; bucket < previous_buckets; bucket++)
        if (previous[bucket].slot_ != no_slot) index_[bucket_of(previous[bucket].key_)] = previous[bucket];
    if (previous != nullptr) blocks_.release(previous);
    return Status::ok;
}

template <typename Key, typename Entry>
Status KeyedSlots<Key, Entry>::make_buckets(uint32_t count)
{
    AV_SCOPE("KeyedSlots::make_buckets");
    if (2u * count <= buckets_) return Status::ok;
    uint32_t wanted = buckets_ < first_buckets ? first_buckets : buckets_;
    while (wanted < 2u * count) wanted *= 2u;
    return rebuild(wanted);
}

template <typename Key, typename Entry>
uint32_t KeyedSlots<Key, Entry>::spare_rows() const
{
    AV_SCOPE("KeyedSlots::spare_rows");
    return first_rows * ((uint32_t{1} << made_chunks_) - 1u) - slots_;
}

template <typename Key, typename Entry>
Status KeyedSlots<Key, Entry>::make_rows(uint32_t count)
{
    AV_SCOPE("KeyedSlots::make_rows");
    /* Never past most_chunks: no table holds more than most_entries. */
    while (free_count_ + spare_rows() < count) {
        const size_t rows = size_t{first_rows} << made_chunks_;
        auto *const  chunk = static_cast<Row *>(blocks_.allocate(sizeof(Row) * rows, pool_));
        if (chunk == nullptr) return Status::no_memory;
        for (size_t index = 0u; index < rows; index++) chunk[index].live_ = false;
        chunks_[made_chunks_++] = chunk;
    }
    return Status::ok;
}

template <typename Key, typename Entry>
Status KeyedSlots<Key, Entry>::insert(Key key, const Entry &entry, uint32_t &slot)
{
    AV_SCOPE("KeyedSlots::insert");
    blocks_.require_irql_at_most(Irql::apc, "KeyedSlots::insert");
    if (slot_of(key) != no_slot) return Status::conflict;
    if (count_ == most_entries) return Status::no_memory;
    const Status indexed = make_buckets(count_ + 1u);
    if (indexed != Status::ok) return indexed;
    const Status rowed = make_rows(1u);
    if (rowed != Status::ok) return rowed;
    if (free_head_ != no_slot) {
        slot = free_head_;
        free_head_ = row(slot)->next_free_;
        free_count_--;
    } else {
        slot = slots_++;
    }
    Row *const target = row(slot);
    target->entry_ = entry;
    target->key_ = key;
    target->live_ = true;
    index_[bucket_of(key)] = Bucket{key, slot};
    count_++;
    return Status::ok;
}

template <typename Key, typename Entry>
Status KeyedSlots<Key, Entry>::reserve(uint32_t count)
{
    AV_SCOPE("KeyedSlots::reserve");
    blocks_.require_irql_at_most(Irql::apc, "KeyedSlots::reserve");
    if (count <= count_) return Status::ok;
    if (count > most_entries) return Status::no_memory;
    const Status indexed = make_buckets(count);
    if (indexed != Status::ok) return indexed;
    return make_rows(count - count_);
}

template <typename Key, typename Entry>
uint32_t KeyedSlots<Key, Entry>::slot_of(Key key) const
{
    AV_SCOPE("KeyedSlots::slot_of");
    if (index_ == nullptr) return no_slot;
    return index_[bucket_of(key)].slot_;
}

template <typename Key, typename Entry>
Entry *KeyedSlots<Key, Entry>::find(Key key)
{
    AV_SCOPE("KeyedSlots::find");
    blocks_.require_irql_at_most(Irql::dispatch, "KeyedSlots::find");
    return at(slot_of(key));
}

template <typename Key, typename Entry>
const Entry *KeyedSlots<Key, Entry>::find(Key key) const
{
    AV_SCOPE("KeyedSlots::find");
    blocks_.require_irql_at_most(Irql::dispatch, "KeyedSlots::find");
    return at(slot_of(key));
}

template <typename Key, typename Entry>
Entry *KeyedSlots<Key, Entry>::at(uint32_t slot)
{
    AV_SCOPE("KeyedSlots::at");
    if (slot >= slots_) return nullptr;
    Row *const found = row(slot);
    return found->live_ ? &found->entry_ : nullptr;
}

template <typename Key, typename Entry>
const Entry *KeyedSlots<Key, Entry>::at(uint32_t slot) const
{
    AV_SCOPE("KeyedSlots::at");
    if (slot >= slots_) return nullptr;
    const Row *const found = row(slot);
    return found->live_ ? &found->entry_ : nullptr;
}

template <typename Key, typename Entry>
Key KeyedSlots<Key, Entry>::key_at(uint32_t slot) const
{
    AV_SCOPE("KeyedSlots::key_at");
    if (slot >= slots_) return Key(0);
    const Row *const found = row(slot);
    return found->live_ ? found->key_ : Key(0);
}

template <typename Key, typename Entry>
void KeyedSlots<Key, Entry>::free_row(uint32_t slot)
{
    AV_SCOPE("KeyedSlots::free_row");
    Row *const freed = row(slot);
    unindex(bucket_of(freed->key_));
    freed->live_ = false;
    freed->next_free_ = free_head_;
    free_head_ = slot;
    free_count_++;
    count_--;
}

template <typename Key, typename Entry>
bool KeyedSlots<Key, Entry>::take(Key key, Entry &entry)
{
    AV_SCOPE("KeyedSlots::take");
    blocks_.require_irql_at_most(Irql::dispatch, "KeyedSlots::take");
    const uint32_t slot = slot_of(key);
    if (slot == no_slot) return false;
    entry = row(slot)->entry_;
    free_row(slot);
    return true;
}

template <typename Key, typename Entry>
bool KeyedSlots<Key, Entry>::erase(Key key)
{
    AV_SCOPE("KeyedSlots::erase");
    blocks_.require_irql_at_most(Irql::dispatch, "KeyedSlots::erase");
    const uint32_t slot = slot_of(key);
    if (slot == no_slot) return false;
    free_row(slot);
    return true;
}

template <typename Key, typename Entry>
void KeyedSlots<Key, Entry>::erase_slot(uint32_t slot)
{
    AV_SCOPE("KeyedSlots::erase_slot");
    blocks_.require_irql_at_most(Irql::dispatch, "KeyedSlots::erase_slot");
    if (at(slot) != nullptr) free_row(slot);
}

template <typename Key, typename Entry>
uint32_t KeyedSlots<Key, Entry>::next_live(uint32_t from) const
{
    AV_SCOPE("KeyedSlots::next_live");
    for (uint32_t slot = from; slot < slots_; slot++)
        if (row(slot)->live_) return slot;
    return no_slot;
}

template <typename Key, typename Entry>
template <typename Visitor>
void KeyedSlots<Key, Entry>::for_each(Visitor visit)
{
    AV_SCOPE("KeyedSlots::for_each");
    for (uint32_t slot = next_live(0u); slot != no_slot; slot = next_live(slot + 1u))
        visit(row(slot)->key_, row(slot)->entry_);
}

template <typename Key, typename Entry>
template <typename Visitor>
void KeyedSlots<Key, Entry>::for_each(Visitor visit) const
{
    AV_SCOPE("KeyedSlots::for_each");
    for (uint32_t slot = next_live(0u); slot != no_slot; slot = next_live(slot + 1u))
        visit(row(slot)->key_, static_cast<const Entry &>(row(slot)->entry_));
}

template <typename Key, typename Entry>
template <typename Predicate>
uint32_t KeyedSlots<Key, Entry>::erase_if(Predicate matches)
{
    AV_SCOPE("KeyedSlots::erase_if");
    uint32_t gone = 0u;
    for (uint32_t slot = next_live(0u); slot != no_slot; slot = next_live(slot + 1u)) {
        if (!matches(row(slot)->key_, row(slot)->entry_)) continue;
        free_row(slot);
        gone++;
    }
    return gone;
}

template <typename Key, typename Entry>
void KeyedSlots<Key, Entry>::clear()
{
    AV_SCOPE("KeyedSlots::clear");
    blocks_.require_irql_at_most(Irql::apc, "KeyedSlots::clear");
    for (uint32_t chunk = 0u; chunk < made_chunks_; chunk++) blocks_.release(chunks_[chunk]);
    if (index_ != nullptr) blocks_.release(index_);
    made_chunks_ = 0u;
    index_ = nullptr;
    buckets_ = 0u;
    count_ = 0u;
    slots_ = 0u;
    free_head_ = no_slot;
    free_count_ = 0u;
}

template <typename Key, typename Entry>
uint32_t KeyedSlots<Key, Entry>::count() const
{
    AV_SCOPE("KeyedSlots::count");
    return count_;
}

template <typename Key, typename Entry>
uint32_t KeyedSlots<Key, Entry>::slots() const
{
    AV_SCOPE("KeyedSlots::slots");
    return slots_;
}

template <typename Key, typename Entry>
uint32_t KeyedSlots<Key, Entry>::buckets() const
{
    AV_SCOPE("KeyedSlots::buckets");
    return buckets_;
}

template <typename Key, typename Entry>
bool KeyedSlots<Key, Entry>::invariants_hold() const
{
    AV_SCOPE("KeyedSlots::invariants_hold");
    if (slots_ > first_rows * ((uint32_t{1} << made_chunks_) - 1u) || 2u * count_ > buckets_ ||
        (buckets_ & (buckets_ - 1u)) != 0u || (index_ == nullptr) != (buckets_ == 0u))
        return false;
    uint32_t live = 0u;
    for (uint32_t slot = next_live(0u); slot != no_slot; slot = next_live(slot + 1u)) {
        live++;
        if (slot_of(row(slot)->key_) != slot) return false;
    }
    uint32_t freed = 0u;
    for (uint32_t slot = free_head_; slot != no_slot && freed <= free_count_; slot = row(slot)->next_free_) {
        if (slot >= slots_ || row(slot)->live_) return false;
        freed++;
    }
    uint32_t indexed = 0u;
    for (uint32_t bucket = 0u; bucket < buckets_; bucket++) indexed += index_[bucket].slot_ != no_slot;
    return live == count_ && freed == free_count_ && indexed == count_ && live + freed == slots_;
}

}  // namespace av

#endif  // AV_KEYED_SLOTS_HPP
