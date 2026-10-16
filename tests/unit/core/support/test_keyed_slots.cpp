#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>

namespace av {

struct KeyedSlotsProbe {
    template <typename Key, typename Entry>
    static std::uint32_t home_of(const KeyedSlots<Key, Entry> &table, Key key) { return table.home_of(key); }

    template <typename Key, typename Entry>
    static std::uint32_t chunks(const KeyedSlots<Key, Entry> &table) { return table.made_chunks_; }

    template <typename Key, typename Entry>
    static void corrupt_count(KeyedSlots<Key, Entry> &table, std::uint32_t count) { table.count_ = count; }

    template <typename Key, typename Entry>
    static void corrupt_slots(KeyedSlots<Key, Entry> &table, std::uint32_t slots) { table.slots_ = slots; }

    template <typename Key, typename Entry>
    static void corrupt_buckets(KeyedSlots<Key, Entry> &table, std::uint32_t buckets) { table.buckets_ = buckets; }

    template <typename Key, typename Entry>
    static void corrupt_free_count(KeyedSlots<Key, Entry> &table, std::uint32_t count) { table.free_count_ = count; }

    template <typename Key, typename Entry>
    static void corrupt_free_head(KeyedSlots<Key, Entry> &table, std::uint32_t slot) { table.free_head_ = slot; }

    template <typename Key, typename Entry>
    static void revive(KeyedSlots<Key, Entry> &table, std::uint32_t slot) { table.row(slot)->live_ = true; }

    template <typename Key, typename Entry>
    static void rekey(KeyedSlots<Key, Entry> &table, std::uint32_t slot, Key key) { table.row(slot)->key_ = key; }

    template <typename Key, typename Entry>
    static void link_free(KeyedSlots<Key, Entry> &table, std::uint32_t slot, std::uint32_t next)
    {
        table.row(slot)->next_free_ = next;
    }

    /* `key` indexed to `slot` with no entry put there, or (no_slot) unindexed. */
    template <typename Key, typename Entry>
    static void index_quietly(KeyedSlots<Key, Entry> &table, Key key, std::uint32_t slot)
    {
        table.index_[table.bucket_of(key)] = typename KeyedSlots<Key, Entry>::Bucket{key, slot};
    }

    template <typename Key, typename Entry>
    static void drop_index(KeyedSlots<Key, Entry> &table)
    {
        table.blocks_.release(table.index_);
        table.index_ = nullptr;
    }
};

}  // namespace av

using av::Irql;
using av::KeyedSlots;
using av::KeyedSlotsProbe;
using av::MockPlatform;
using av::Pool;
using av::Status;
using av::Violation;

namespace {

struct Object {
    std::uint64_t address;
    std::uint32_t kind;
};

using Objects = KeyedSlots<std::uint32_t, Object>;

bool names(const Objects &table, std::uint32_t key, std::uint64_t address)
{
    const Object *const object = table.find(key);
    return object != nullptr && object->address == address;
}

std::uint32_t put(Objects &table, std::uint32_t key, std::uint64_t address)
{
    std::uint32_t slot = Objects::no_slot;
    return table.insert(key, Object{address, 0u}, slot) == Status::ok ? slot : Objects::no_slot;
}

/* `count` keys after `after` whose probe starts where `key`'s does: ids a
 * hostile guest would pick to share one run. */
void colliding(const Objects &table, std::uint32_t key, std::uint32_t after, std::uint32_t *keys, std::uint32_t count)
{
    std::uint32_t found = 0u;
    for (std::uint32_t candidate = after + 1u; found < count; candidate++)
        if (KeyedSlotsProbe::home_of(table, candidate) == KeyedSlotsProbe::home_of(table, key)) keys[found++] = candidate;
}

}  // namespace

TEST(keyed_slots_insert, starts_with_nothing_and_hands_out_slot_zero_first)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  CHECK(table.invariants_hold());
  CHECK(table.find(1u) == nullptr);
  CHECK_EQ(table.slot_of(1u), Objects::no_slot);
  CHECK_EQ(platform.live_allocations(), 0u);
  std::uint32_t slot = 99u;
  CHECK_STATUS(table.insert(1u, Object{0x1000u, 3u}, slot), Status::ok);
  CHECK(table.invariants_hold());
  CHECK_EQ(slot, 0u);
  CHECK_EQ(table.count(), 1u);
  CHECK_EQ(table.slots(), 1u);
  CHECK_EQ(table.buckets(), Objects::first_buckets);
  CHECK(names(table, 1u, 0x1000u));
  CHECK_EQ(table.find(1u)->kind, 3u);
  CHECK(table.at(0u) == table.find(1u));
  CHECK_EQ(table.key_at(0u), 1u);
  CHECK_EQ(table.slot_of(1u), 0u);
  CHECK_EQ(platform.live_allocations(), 2u); }

TEST(keyed_slots_insert, takes_every_key_a_guest_can_name_including_zero_and_all_ones)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  CHECK_EQ(put(table, 0u, 10u), 0u);
  CHECK_EQ(put(table, 0xFFFFFFFFu, 20u), 1u);
  CHECK_EQ(put(table, 0x80000000u, 30u), 2u);
  CHECK(table.invariants_hold());
  CHECK(names(table, 0u, 10u));
  CHECK(names(table, 0xFFFFFFFFu, 20u));
  CHECK(names(table, 0x80000000u, 30u));
  KeyedSlots<std::uint64_t, std::uint32_t> wide(platform, Pool::paged);
  std::uint32_t slot = 0u;
  CHECK_STATUS(wide.insert(0xFFFFFFFFFFFFFFFFu, 1u, slot), Status::ok);
  CHECK_STATUS(wide.insert(0xFFFFFFFF00000000u, 2u, slot), Status::ok);
  CHECK_STATUS(wide.insert(0x00000000FFFFFFFFu, 3u, slot), Status::ok);
  CHECK_EQ(*wide.find(0xFFFFFFFFFFFFFFFFu), 1u);
  CHECK_EQ(*wide.find(0xFFFFFFFF00000000u), 2u);
  CHECK_EQ(*wide.find(0x00000000FFFFFFFFu), 3u);
  CHECK(wide.find(0u) == nullptr);
  CHECK_EQ(wide.key_at(2u), 0x00000000FFFFFFFFu);
  CHECK(wide.invariants_hold()); }

TEST(keyed_slots_insert, refuses_a_key_already_in_a_slot_and_keeps_its_entry)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  CHECK_EQ(put(table, 7u, 1u), 0u);
  std::uint32_t slot = 55u;
  CHECK_STATUS(table.insert(7u, Object{2u, 2u}, slot), Status::conflict);
  CHECK_EQ(slot, 55u);
  CHECK(names(table, 7u, 1u));
  CHECK_EQ(table.count(), 1u);
  CHECK(table.invariants_hold()); }

TEST(keyed_slots_insert, grows_well_past_any_old_table_with_every_entry_where_it_was_put)
{ MockPlatform platform;
  Objects table(platform, Pool::non_paged);
  const Object *first = nullptr;
  for (std::uint32_t key = 1u; key <= 300000u; key++) {
      CHECK_EQ(put(table, key, key * 16u), key - 1u);
      if (key == 1u) first = table.find(1u);
      if ((key & (key - 1u)) == 0u) CHECK(table.invariants_hold());
  }
  CHECK_EQ(table.count(), 300000u);
  CHECK_EQ(table.slots(), 300000u);
  CHECK_EQ(table.buckets(), 1048576u);
  CHECK_EQ(KeyedSlotsProbe::chunks(table), 15u);
  CHECK(table.find(1u) == first);
  for (std::uint32_t key = 1u; key <= 300000u; key++) {
      CHECK(names(table, key, key * 16u));
      CHECK_EQ(table.slot_of(key), key - 1u);
  }
  CHECK(table.find(300001u) == nullptr);
  CHECK(table.invariants_hold()); }

TEST(keyed_slots_insert, a_failed_index_growth_changes_nothing)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  for (std::uint32_t key = 1u; key <= Objects::first_buckets / 2u; key++) CHECK_EQ(put(table, key, key), key - 1u);
  platform.fail_allocation_at(1u);
  std::uint32_t slot = 77u;
  CHECK_STATUS(table.insert(100u, Object{100u, 0u}, slot), Status::no_memory);
  CHECK_EQ(slot, 77u);
  CHECK(table.invariants_hold());
  CHECK_EQ(table.count(), Objects::first_buckets / 2u);
  CHECK_EQ(table.buckets(), Objects::first_buckets);
  CHECK(table.find(100u) == nullptr);
  for (std::uint32_t key = 1u; key <= Objects::first_buckets / 2u; key++) CHECK(names(table, key, key));
  CHECK_EQ(put(table, 100u, 100u), Objects::first_buckets / 2u);
  CHECK(names(table, 100u, 100u)); }

TEST(keyed_slots_insert, a_failed_chunk_changes_nothing)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  for (std::uint32_t key = 0u; key < 3u * Objects::first_rows; key++) CHECK_EQ(put(table, key, key), key);
  CHECK_EQ(KeyedSlotsProbe::chunks(table), 2u);
  CHECK_EQ(table.buckets(), 128u);
  platform.fail_allocation_at(1u);
  std::uint32_t slot = 77u;
  CHECK_STATUS(table.insert(500u, Object{500u, 0u}, slot), Status::no_memory);
  CHECK_EQ(slot, 77u);
  CHECK_EQ(table.count(), 3u * Objects::first_rows);
  CHECK(table.find(500u) == nullptr);
  CHECK(table.invariants_hold());
  CHECK_EQ(put(table, 500u, 500u), 3u * Objects::first_rows);
  CHECK_EQ(KeyedSlotsProbe::chunks(table), 3u); }

TEST(keyed_slots_insert, the_first_allocation_failing_leaves_an_empty_table)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  platform.fail_allocation_at(1u);
  CHECK_EQ(put(table, 1u, 1u), Objects::no_slot);
  CHECK(table.invariants_hold());
  CHECK_EQ(table.count(), 0u);
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(keyed_slots_insert, refuses_past_the_most_entries_a_table_holds)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  CHECK_EQ(put(table, 1u, 1u), 0u);
  KeyedSlotsProbe::corrupt_count(table, Objects::most_entries);
  std::uint32_t slot = 5u;
  CHECK_STATUS(table.insert(2u, Object{2u, 0u}, slot), Status::no_memory);
  CHECK_EQ(slot, 5u);
  KeyedSlotsProbe::corrupt_count(table, 1u);
  CHECK(table.find(2u) == nullptr);
  CHECK(table.invariants_hold()); }

TEST(keyed_slots_insert, is_legal_up_to_apc)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  platform.set_irql(Irql::dispatch);
  static_cast<void>(put(table, 1u, 1u));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  while (platform.claim(Violation::allocation_at_dispatch)) {}
  table.clear(); }

TEST(keyed_slots_find, finding_and_erasing_are_legal_at_dispatch)
{ MockPlatform platform;
  Objects table(platform, Pool::non_paged);
  CHECK_EQ(put(table, 5u, 5u), 0u);
  CHECK_EQ(put(table, 6u, 6u), 1u);
  CHECK_EQ(put(table, 7u, 7u), 2u);
  platform.set_irql(Irql::dispatch);
  const Objects &view = table;
  CHECK(view.find(5u) != nullptr);
  CHECK(view.find(9u) == nullptr);
  CHECK(table.erase(5u));
  Object taken{};
  CHECK(table.take(6u, taken));
  table.erase_slot(2u);
  platform.set_irql(Irql::passive);
  CHECK_EQ(table.count(), 0u);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(keyed_slots_at, answers_null_for_any_slot_no_entry_is_in)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  CHECK(table.at(0u) == nullptr);
  CHECK_EQ(table.key_at(0u), 0u);
  CHECK_EQ(put(table, 9u, 90u), 0u);
  CHECK_EQ(put(table, 10u, 100u), 1u);
  CHECK(table.erase(9u));
  const Objects &view = table;
  CHECK(table.at(0u) == nullptr);
  CHECK(view.at(0u) == nullptr);
  CHECK_EQ(table.key_at(0u), 0u);
  CHECK(view.at(1u) != nullptr);
  CHECK_EQ(view.at(1u)->address, 100u);
  CHECK(table.at(2u) == nullptr);
  CHECK(view.at(Objects::first_rows) == nullptr);
  CHECK(table.at(Objects::no_slot) == nullptr);
  CHECK_EQ(table.key_at(0xFFFFFFF0u), 0u);
  table.erase_slot(Objects::no_slot);
  table.erase_slot(0u);
  CHECK_EQ(table.count(), 1u);
  CHECK(table.invariants_hold()); }

TEST(keyed_slots_erase, frees_the_slot_for_the_next_insert_and_keeps_others_in_place)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  for (std::uint32_t key = 0u; key < 5u; key++) CHECK_EQ(put(table, key, key), key);
  const Object *const kept = table.find(4u);
  CHECK(table.erase(1u));
  CHECK(table.erase(3u));
  CHECK(!table.erase(3u));
  CHECK(!table.erase(77u));
  CHECK(table.invariants_hold());
  CHECK_EQ(put(table, 50u, 50u), 3u);
  CHECK_EQ(put(table, 51u, 51u), 1u);
  CHECK_EQ(put(table, 52u, 52u), 5u);
  CHECK(table.find(4u) == kept);
  CHECK(names(table, 50u, 50u));
  CHECK(table.find(1u) == nullptr);
  CHECK_EQ(table.slots(), 6u);
  CHECK(table.invariants_hold()); }

TEST(keyed_slots_erase, on_an_empty_table_finds_nothing)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  CHECK(!table.erase(0u));
  Object kept{4u, 4u};
  CHECK(!table.take(0u, kept));
  CHECK_EQ(kept.address, 4u);
  CHECK(table.invariants_hold()); }

TEST(keyed_slots_erase, shifts_colliding_keys_back_so_each_is_still_found)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  CHECK_EQ(put(table, 1u, 1u), 0u);
  std::uint32_t keys[5] = {};
  colliding(table, 1u, 1u, keys, 5u);
  for (std::uint32_t key : keys) CHECK(put(table, key, key) != Objects::no_slot);
  CHECK_EQ(table.buckets(), Objects::first_buckets);
  CHECK(table.erase(keys[0]));
  CHECK(table.invariants_hold());
  CHECK(table.erase(1u));
  CHECK(table.invariants_hold());
  for (std::uint32_t index = 1u; index < 5u; index++) CHECK(names(table, keys[index], keys[index]));
  CHECK(table.find(keys[0]) == nullptr);
  CHECK(table.find(1u) == nullptr);
  CHECK(table.erase(keys[4]));
  CHECK(table.erase(keys[2]));
  CHECK(names(table, keys[1], keys[1]));
  CHECK(names(table, keys[3], keys[3]));
  CHECK(table.invariants_hold()); }

TEST(keyed_slots_erase, keys_made_and_erased_for_ever_never_grow_anything)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  CHECK_EQ(put(table, 0xFFFFFFFFu, 1u), 0u);
  for (std::uint32_t key = 1u; key <= 100000u; key++) {
      CHECK_EQ(put(table, key, key), 1u);
      CHECK(table.erase(key));
      if (key % 4096u == 0u) CHECK(table.invariants_hold());
  }
  CHECK_EQ(table.buckets(), Objects::first_buckets);
  CHECK_EQ(table.slots(), 2u);
  CHECK_EQ(KeyedSlotsProbe::chunks(table), 1u);
  CHECK(names(table, 0xFFFFFFFFu, 1u));
  CHECK(table.invariants_hold()); }

TEST(keyed_slots_take, hands_the_entry_back_and_frees_its_slot)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  std::uint32_t slot = 0u;
  CHECK_STATUS(table.insert(3u, Object{0x30u, 2u}, slot), Status::ok);
  Object taken{0u, 0u};
  CHECK(table.take(3u, taken));
  CHECK_EQ(taken.address, 0x30u);
  CHECK_EQ(taken.kind, 2u);
  CHECK(table.find(3u) == nullptr);
  CHECK(table.at(slot) == nullptr);
  Object untouched{5u, 5u};
  CHECK(!table.take(3u, untouched));
  CHECK_EQ(untouched.address, 5u);
  CHECK(table.invariants_hold()); }

TEST(keyed_slots_hostile, strided_keys_past_every_rebuild_are_all_found)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  for (std::uint32_t round = 0u; round < 20000u; round++) CHECK(put(table, round * 0x10000u, round) != Objects::no_slot);
  for (std::uint32_t round = 0u; round < 20000u; round++) CHECK(names(table, round * 0x10000u, round));
  for (std::uint32_t round = 0u; round < 20000u; round += 3u) CHECK(table.erase(round * 0x10000u));
  for (std::uint32_t round = 0u; round < 20000u; round++)
      CHECK(round % 3u == 0u ? table.find(round * 0x10000u) == nullptr : names(table, round * 0x10000u, round));
  CHECK(table.invariants_hold()); }

TEST(keyed_slots_reserve, makes_room_so_inserts_up_to_it_allocate_nothing)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  CHECK_EQ(put(table, 1u, 1u), 0u);
  CHECK_STATUS(table.reserve(1000u), Status::ok);
  CHECK(table.invariants_hold());
  const std::uint32_t allocations = platform.live_allocations();
  platform.fail_allocation_at(1u);
  for (std::uint32_t key = 2u; key <= 1000u; key++) CHECK(put(table, key, key) != Objects::no_slot);
  CHECK_EQ(platform.live_allocations(), allocations);
  std::uint32_t key = 1001u;
  while (put(table, key, key) != Objects::no_slot) key++;
  CHECK(table.find(key) == nullptr);
  CHECK(table.count() >= 1000u);
  CHECK(table.invariants_hold()); }

TEST(keyed_slots_reserve, counts_freed_slots_as_room)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  for (std::uint32_t key = 0u; key < Objects::first_rows; key++) CHECK_EQ(put(table, key, key), key);
  for (std::uint32_t key = 0u; key < 4u; key++) CHECK(table.erase(key));
  platform.fail_allocation_at(1u);
  CHECK_STATUS(table.reserve(Objects::first_rows), Status::ok);
  CHECK_STATUS(table.reserve(Objects::first_rows + 1u), Status::no_memory);
  CHECK_EQ(KeyedSlotsProbe::chunks(table), 1u);
  CHECK(table.invariants_hold()); }

TEST(keyed_slots_reserve, asks_for_nothing_when_the_room_is_already_there_and_reports_a_failed_index)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  CHECK_STATUS(table.reserve(0u), Status::ok);
  CHECK_EQ(platform.live_allocations(), 0u);
  CHECK_EQ(put(table, 1u, 1u), 0u);
  CHECK_EQ(put(table, 2u, 2u), 1u);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(table.reserve(1u), Status::ok);
  CHECK_STATUS(table.reserve(Objects::first_buckets / 2u), Status::ok);
  CHECK_STATUS(table.reserve(Objects::first_buckets / 2u + 1u), Status::no_memory);
  CHECK_EQ(table.buckets(), Objects::first_buckets);
  CHECK(names(table, 2u, 2u));
  CHECK(table.invariants_hold()); }

TEST(keyed_slots_reserve, refuses_more_than_a_table_may_hold)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  CHECK_STATUS(table.reserve(Objects::most_entries + 1u), Status::no_memory);
  CHECK_STATUS(table.reserve(0xFFFFFFFFu), Status::no_memory);
  CHECK_EQ(platform.live_allocations(), 0u);
  CHECK(table.invariants_hold()); }

TEST(keyed_slots_limits, bound_slots_and_buckets_by_what_a_size_t_counts)
{ CHECK_EQ(Objects::most_chunks, 27u);
  CHECK_EQ(Objects::most_slots, 16u * ((1u << 27u) - 1u));
  CHECK_EQ(Objects::most_buckets, 0x80000000u);
  CHECK_EQ(Objects::most_entries, 0x40000000u);
  CHECK(Objects::most_slots < Objects::no_slot); }

TEST(keyed_slots_next_live, walks_live_slots_in_order)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  CHECK_EQ(table.next_live(0u), Objects::no_slot);
  for (std::uint32_t key = 0u; key < 40u; key++) CHECK_EQ(put(table, key + 100u, key), key);
  for (std::uint32_t key = 0u; key < 40u; key += 2u) CHECK(table.erase(key + 100u));
  std::uint32_t visited = 0u;
  for (std::uint32_t slot = table.next_live(0u); slot != Objects::no_slot; slot = table.next_live(slot + 1u)) {
      CHECK_EQ(slot % 2u, 1u);
      CHECK_EQ(table.key_at(slot), slot + 100u);
      visited++;
  }
  CHECK_EQ(visited, 20u);
  CHECK_EQ(table.next_live(40u), Objects::no_slot);
  CHECK_EQ(table.next_live(Objects::no_slot), Objects::no_slot); }

TEST(keyed_slots_for_each, visits_every_entry_once_in_slot_order_and_may_change_them)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  for (std::uint32_t key = 1u; key <= 50u; key++) CHECK(put(table, key, key) != Objects::no_slot);
  CHECK(table.erase(25u));
  std::uint64_t sum = 0u;
  std::uint32_t visits = 0u;
  std::uint32_t previous = 0u;
  bool          ordered = true;
  table.for_each([&](std::uint32_t key, Object &object) {
      ordered = ordered && key > previous;
      previous = key;
      sum += key;
      visits++;
      object.kind = key + 1u;
  });
  CHECK(ordered);
  CHECK_EQ(visits, 49u);
  CHECK_EQ(sum, 50u * 51u / 2u - 25u);
  const Objects &view = table;
  std::uint32_t mismatched = 0u;
  view.for_each([&](std::uint32_t key, const Object &object) { mismatched += object.kind != key + 1u; });
  CHECK_EQ(mismatched, 0u);
  Objects empty(platform, Pool::paged);
  empty.for_each([&](std::uint32_t, Object &) { visits++; });
  CHECK_EQ(visits, 49u); }

TEST(keyed_slots_erase_if, erases_what_matches_and_lets_the_predicate_see_each_entry)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  for (std::uint32_t key = 0u; key < 64u; key++) {
      std::uint32_t slot = 0u;
      CHECK_STATUS(table.insert(key, Object{key, key % 3u}, slot), Status::ok);
  }
  std::uint64_t released = 0u;
  const std::uint32_t gone = table.erase_if([&](std::uint32_t, Object &object) {
      if (object.kind != 0u) return false;
      released += object.address;
      return true;
  });
  CHECK_EQ(gone, 22u);
  CHECK_EQ(table.count(), 42u);
  CHECK_EQ(released, 21u * 22u / 2u * 3u);
  for (std::uint32_t key = 0u; key < 64u; key++) CHECK((table.find(key) == nullptr) == (key % 3u == 0u));
  CHECK(table.invariants_hold()); }

TEST(keyed_slots_clear, gives_everything_back_and_forgets_every_key)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  for (std::uint32_t key = 0u; key < 100u; key++) CHECK(put(table, key, key) != Objects::no_slot);
  CHECK(table.erase(50u));
  table.clear();
  CHECK(table.invariants_hold());
  CHECK_EQ(table.count(), 0u);
  CHECK_EQ(table.slots(), 0u);
  CHECK_EQ(platform.live_allocations(), 0u);
  CHECK(table.find(5u) == nullptr);
  CHECK_EQ(put(table, 5u, 6u), 0u);
  CHECK(names(table, 5u, 6u)); }

TEST(keyed_slots_destructor, gives_everything_back)
{ MockPlatform platform;
  { Objects table(platform, Pool::paged);
    for (std::uint32_t key = 0u; key < 40u; key++) CHECK(put(table, key, key) != Objects::no_slot); }
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(keyed_slots_invariants, fail_for_wrong_counts_bad_sizes_a_broken_free_list_or_an_unindexed_entry)
{ MockPlatform platform;
  Objects table(platform, Pool::paged);
  KeyedSlotsProbe::corrupt_buckets(table, 16u);
  CHECK(!table.invariants_hold());
  KeyedSlotsProbe::corrupt_buckets(table, 0u);
  KeyedSlotsProbe::corrupt_slots(table, 1u);
  CHECK(!table.invariants_hold());
  KeyedSlotsProbe::corrupt_slots(table, 0u);
  for (std::uint32_t key = 0u; key < 4u; key++) CHECK_EQ(put(table, key, key), key);
  CHECK(table.erase(3u));
  CHECK(table.invariants_hold());
  KeyedSlotsProbe::corrupt_count(table, 4u);
  CHECK(!table.invariants_hold());
  KeyedSlotsProbe::corrupt_count(table, 9u);
  CHECK(!table.invariants_hold());
  KeyedSlotsProbe::corrupt_count(table, 3u);
  KeyedSlotsProbe::corrupt_buckets(table, 12u);
  CHECK(!table.invariants_hold());
  KeyedSlotsProbe::corrupt_buckets(table, 16u);
  KeyedSlotsProbe::corrupt_free_count(table, 2u);
  CHECK(!table.invariants_hold());
  KeyedSlotsProbe::corrupt_free_count(table, 0u);
  CHECK(!table.invariants_hold());
  KeyedSlotsProbe::corrupt_free_count(table, 1u);
  KeyedSlotsProbe::corrupt_free_head(table, 2u);
  CHECK(!table.invariants_hold());
  KeyedSlotsProbe::corrupt_free_head(table, 9u);
  CHECK(!table.invariants_hold());
  KeyedSlotsProbe::corrupt_free_head(table, 3u);
  CHECK(table.invariants_hold());
  KeyedSlotsProbe::link_free(table, 3u, 3u);
  CHECK(!table.invariants_hold());
  KeyedSlotsProbe::link_free(table, 3u, Objects::no_slot);
  KeyedSlotsProbe::corrupt_free_head(table, Objects::no_slot);
  KeyedSlotsProbe::corrupt_free_count(table, 0u);
  CHECK(!table.invariants_hold());
  KeyedSlotsProbe::corrupt_free_head(table, 3u);
  KeyedSlotsProbe::corrupt_free_count(table, 1u);
  CHECK(table.invariants_hold());
  KeyedSlotsProbe::rekey(table, 1u, 77u);
  CHECK(!table.invariants_hold());
  KeyedSlotsProbe::rekey(table, 1u, 1u);
  KeyedSlotsProbe::corrupt_slots(table, 3u);
  CHECK(!table.invariants_hold());
  KeyedSlotsProbe::corrupt_slots(table, Objects::first_rows + 1u);
  CHECK(!table.invariants_hold());
  KeyedSlotsProbe::corrupt_slots(table, 4u);
  KeyedSlotsProbe::revive(table, 3u);
  CHECK(!table.invariants_hold());
  table.clear();
  CHECK_EQ(put(table, 1u, 1u), 0u);
  CHECK_EQ(put(table, 2u, 2u), 1u);
  CHECK(table.erase(2u));
  KeyedSlotsProbe::index_quietly(table, 99u, 1u);
  CHECK(!table.invariants_hold());
  KeyedSlotsProbe::index_quietly(table, 99u, Objects::no_slot);
  CHECK(table.invariants_hold());
  KeyedSlotsProbe::drop_index(table);
  CHECK(!table.invariants_hold());
  KeyedSlotsProbe::corrupt_buckets(table, 0u);
  KeyedSlotsProbe::corrupt_count(table, 0u);
  KeyedSlotsProbe::corrupt_slots(table, 0u);
  KeyedSlotsProbe::corrupt_free_head(table, Objects::no_slot);
  KeyedSlotsProbe::corrupt_free_count(table, 0u);
  CHECK(table.invariants_hold()); }
