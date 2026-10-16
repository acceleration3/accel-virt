#include "core/support/id_map.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>

namespace av {

struct IdMapProbe {
    template <typename Key, typename Value>
    static std::uint32_t start_of(const IdMap<Key, Value> &map, Key key) { return map.start_of(key); }

    template <typename Key, typename Value>
    static std::uint32_t tombstones(const IdMap<Key, Value> &map) { return map.removed_; }

    template <typename Key, typename Value>
    static void corrupt_count(IdMap<Key, Value> &map, std::uint32_t count) { map.count_ = count; }

    template <typename Key, typename Value>
    static void corrupt_tombstones(IdMap<Key, Value> &map, std::uint32_t removed) { map.removed_ = removed; }

    template <typename Key, typename Value>
    static void corrupt_capacity(IdMap<Key, Value> &map, std::uint32_t capacity) { map.capacity_ = capacity; }

    /* The live entry for `key` moved one entry along its probe, past an empty
     * one, where a lookup cannot reach it. */
    template <typename Key, typename Value>
    static bool strand(IdMap<Key, Value> &map, Key key)
    {
        using Entry = typename IdMap<Key, Value>::Entry;
        Entry *const entry = map.live_entry(key);
        if (entry == nullptr) return false;
        const std::uint32_t index = static_cast<std::uint32_t>(entry - map.entries_);
        Entry &next = map.entries_[(index + 1u) & (map.capacity_ - 1u)];
        if (next.state_ != IdMap<Key, Value>::State::empty) return false;
        next = *entry;
        entry->state_ = IdMap<Key, Value>::State::empty;
        return true;
    }
};

}  // namespace av

using av::IdMap;
using av::IdMapProbe;
using av::Irql;
using av::MockPlatform;
using av::Pool;
using av::Status;
using av::Violation;

namespace {

struct Object {
    std::uint64_t address;
    std::uint32_t kind;
};

using Objects = IdMap<std::uint32_t, Object>;

/* The first `count` keys after `after` whose probe starts where `key`'s does in
 * a table of `map`'s size: ids a hostile guest would pick to share one run. */
std::uint32_t colliding(const Objects &map, std::uint32_t key, std::uint32_t after, std::uint32_t *keys,
                        std::uint32_t count)
{
    std::uint32_t found = 0u;
    for (std::uint32_t candidate = after + 1u; found < count; candidate++)
        if (IdMapProbe::start_of(map, candidate) == IdMapProbe::start_of(map, key)) keys[found++] = candidate;
    return found;
}

bool names(const Objects &map, std::uint32_t key, std::uint64_t address)
{
    const Object *const object = map.find(key);
    return object != nullptr && object->address == address;
}

}  // namespace

TEST(id_map_insert, starts_with_no_table_and_takes_one_at_the_first_id)
{ MockPlatform platform;
  Objects map(platform, Pool::paged);
  CHECK(map.invariants_hold());
  CHECK(map.find(1u) == nullptr);
  CHECK_EQ(platform.live_allocations(), 0u);
  CHECK_STATUS(map.insert(1u, Object{0x1000u, 3u}), Status::ok);
  CHECK(map.invariants_hold());
  CHECK_EQ(map.count(), 1u);
  CHECK_EQ(map.capacity(), Objects::first_capacity);
  CHECK(names(map, 1u, 0x1000u));
  CHECK_EQ(map.find(1u)->kind, 3u);
  CHECK(map.find(2u) == nullptr); }

TEST(id_map_insert, takes_every_id_a_guest_can_name_including_zero_and_all_ones)
{ MockPlatform platform;
  Objects map(platform, Pool::paged);
  CHECK_STATUS(map.insert(0u, Object{10u, 0u}), Status::ok);
  CHECK_STATUS(map.insert(0xFFFFFFFFu, Object{20u, 0u}), Status::ok);
  CHECK_STATUS(map.insert(0x80000000u, Object{30u, 0u}), Status::ok);
  CHECK(map.invariants_hold());
  CHECK(names(map, 0u, 10u));
  CHECK(names(map, 0xFFFFFFFFu, 20u));
  CHECK(names(map, 0x80000000u, 30u));
  IdMap<std::uint64_t, std::uint32_t> wide(platform, Pool::paged);
  CHECK_STATUS(wide.insert(0xFFFFFFFFFFFFFFFFu, 1u), Status::ok);
  CHECK_STATUS(wide.insert(0xFFFFFFFF00000000u, 2u), Status::ok);
  CHECK_STATUS(wide.insert(0x00000000FFFFFFFFu, 3u), Status::ok);
  CHECK_EQ(*wide.find(0xFFFFFFFFFFFFFFFFu), 1u);
  CHECK_EQ(*wide.find(0xFFFFFFFF00000000u), 2u);
  CHECK_EQ(*wide.find(0x00000000FFFFFFFFu), 3u);
  CHECK(wide.find(0u) == nullptr);
  CHECK(wide.invariants_hold()); }

TEST(id_map_insert, refuses_an_id_already_in_use_and_keeps_its_value)
{ MockPlatform platform;
  Objects map(platform, Pool::paged);
  CHECK_STATUS(map.insert(7u, Object{1u, 1u}), Status::ok);
  CHECK_STATUS(map.insert(7u, Object{2u, 2u}), Status::conflict);
  CHECK(names(map, 7u, 1u));
  CHECK_EQ(map.count(), 1u);
  CHECK(map.invariants_hold()); }

TEST(id_map_insert, grows_well_past_any_old_table_with_every_id_still_found)
{ MockPlatform platform;
  Objects map(platform, Pool::non_paged);
  for (std::uint32_t key = 1u; key <= 300000u; key++) {
      CHECK_STATUS(map.insert(key, Object{key * 16u, key & 7u}), Status::ok);
      if ((key & (key - 1u)) == 0u) CHECK(map.invariants_hold());
  }
  CHECK_EQ(map.count(), 300000u);
  CHECK_EQ(map.capacity(), 1048576u);
  CHECK_EQ(platform.live_allocations(), 1u);
  for (std::uint32_t key = 1u; key <= 300000u; key++) CHECK(names(map, key, key * 16u));
  CHECK(map.find(300001u) == nullptr);
  CHECK(map.invariants_hold()); }

TEST(id_map_insert, a_failed_growth_changes_nothing)
{ MockPlatform platform;
  Objects map(platform, Pool::paged);
  for (std::uint32_t key = 1u; key <= Objects::first_capacity / 2u; key++)
      CHECK_STATUS(map.insert(key, Object{key, 0u}), Status::ok);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(map.insert(100u, Object{100u, 0u}), Status::no_memory);
  CHECK(map.invariants_hold());
  CHECK_EQ(map.count(), Objects::first_capacity / 2u);
  CHECK_EQ(map.capacity(), Objects::first_capacity);
  CHECK(map.find(100u) == nullptr);
  for (std::uint32_t key = 1u; key <= Objects::first_capacity / 2u; key++) CHECK(names(map, key, key));
  CHECK_STATUS(map.insert(100u, Object{100u, 0u}), Status::ok);
  CHECK(names(map, 100u, 100u)); }

TEST(id_map_insert, the_first_table_failing_leaves_an_empty_map)
{ MockPlatform platform;
  Objects map(platform, Pool::paged);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(map.insert(1u, Object{1u, 0u}), Status::no_memory);
  CHECK(map.invariants_hold());
  CHECK_EQ(map.count(), 0u);
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(id_map_insert, is_legal_up_to_apc)
{ MockPlatform platform;
  Objects map(platform, Pool::paged);
  platform.set_irql(Irql::dispatch);
  static_cast<void>(map.insert(1u, Object{1u, 0u}));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  while (platform.claim(Violation::allocation_at_dispatch)) {}
  map.clear(); }

TEST(id_map_find, is_legal_at_dispatch)
{ MockPlatform platform;
  Objects map(platform, Pool::non_paged);
  CHECK_STATUS(map.insert(5u, Object{5u, 0u}), Status::ok);
  platform.set_irql(Irql::dispatch);
  const Objects &view = map;
  CHECK(view.find(5u) != nullptr);
  CHECK(view.find(6u) == nullptr);
  CHECK(map.remove(5u));
  platform.set_irql(Irql::passive);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }

TEST(id_map_find, walks_past_colliding_ids_and_tombstones_to_the_one_asked_for)
{ MockPlatform platform;
  Objects map(platform, Pool::paged);
  CHECK_STATUS(map.insert(1u, Object{1u, 0u}), Status::ok);
  std::uint32_t keys[5] = {};
  CHECK_EQ(colliding(map, 1u, 1u, keys, 5u), 5u);
  for (std::uint32_t key : keys) CHECK_STATUS(map.insert(key, Object{key, 0u}), Status::ok);
  CHECK_EQ(map.capacity(), Objects::first_capacity);
  CHECK(map.remove(keys[0]));
  CHECK(map.remove(keys[2]));
  CHECK(map.invariants_hold());
  CHECK(names(map, 1u, 1u));
  CHECK(names(map, keys[1], keys[1]));
  CHECK(names(map, keys[3], keys[3]));
  CHECK(names(map, keys[4], keys[4]));
  CHECK(map.find(keys[0]) == nullptr);
  CHECK(map.find(keys[2]) == nullptr);
  CHECK_STATUS(map.insert(keys[2], Object{77u, 0u}), Status::ok);
  CHECK_EQ(IdMapProbe::tombstones(map), 1u);
  CHECK(names(map, keys[2], 77u));
  CHECK(map.invariants_hold()); }

TEST(id_map_remove, a_removed_id_is_named_again_with_its_new_value)
{ MockPlatform platform;
  Objects map(platform, Pool::paged);
  CHECK_STATUS(map.insert(9u, Object{1u, 0u}), Status::ok);
  CHECK(map.remove(9u));
  CHECK(!map.remove(9u));
  CHECK(!map.remove(10u));
  CHECK(map.find(9u) == nullptr);
  CHECK_EQ(map.count(), 0u);
  CHECK(map.invariants_hold());
  CHECK_STATUS(map.insert(9u, Object{2u, 0u}), Status::ok);
  CHECK(names(map, 9u, 2u)); }

TEST(id_map_remove, on_an_empty_map_finds_nothing)
{ MockPlatform platform;
  Objects map(platform, Pool::paged);
  CHECK(!map.remove(0u));
  Object kept{4u, 4u};
  CHECK(!map.take(0u, kept));
  CHECK_EQ(kept.address, 4u);
  CHECK(map.invariants_hold()); }

TEST(id_map_take, hands_the_value_back_and_leaves_the_id_unnamed)
{ MockPlatform platform;
  Objects map(platform, Pool::paged);
  CHECK_STATUS(map.insert(3u, Object{0x30u, 2u}), Status::ok);
  Object taken{0u, 0u};
  CHECK(map.take(3u, taken));
  CHECK_EQ(taken.address, 0x30u);
  CHECK_EQ(taken.kind, 2u);
  CHECK(map.find(3u) == nullptr);
  Object untouched{5u, 5u};
  CHECK(!map.take(3u, untouched));
  CHECK_EQ(untouched.address, 5u);
  CHECK(map.invariants_hold()); }

TEST(id_map_tombstones, ids_made_and_removed_for_ever_never_grow_the_table)
{ MockPlatform platform;
  Objects map(platform, Pool::paged);
  CHECK_STATUS(map.insert(0xFFFFFFFFu, Object{1u, 0u}), Status::ok);
  for (std::uint32_t key = 1u; key <= 100000u; key++) {
      CHECK_STATUS(map.insert(key, Object{key, 0u}), Status::ok);
      CHECK(map.remove(key));
      if (key % 4096u == 0u) CHECK(map.invariants_hold());
  }
  CHECK_EQ(map.capacity(), Objects::first_capacity);
  CHECK_EQ(map.count(), 1u);
  CHECK(names(map, 0xFFFFFFFFu, 1u));
  CHECK(map.invariants_hold()); }

TEST(id_map_tombstones, a_rebuild_across_tombstones_keeps_every_live_id)
{ MockPlatform platform;
  Objects map(platform, Pool::paged);
  for (std::uint32_t key = 0u; key < 4096u; key++) CHECK_STATUS(map.insert(key, Object{key, 0u}), Status::ok);
  for (std::uint32_t key = 0u; key < 4096u; key += 2u) CHECK(map.remove(key));
  CHECK_EQ(IdMapProbe::tombstones(map), 2048u);
  std::uint32_t key = 4096u;
  while (IdMapProbe::tombstones(map) != 0u) {
      CHECK_STATUS(map.insert(key, Object{key, 1u}), Status::ok);
      key++;
  }
  CHECK(map.invariants_hold());
  for (std::uint32_t odd = 1u; odd < 4096u; odd += 2u) CHECK(names(map, odd, odd));
  for (std::uint32_t even = 0u; even < 4096u; even += 2u) CHECK(map.find(even) == nullptr);
  for (std::uint32_t added = 4096u; added < key; added++) CHECK(names(map, added, added)); }

TEST(id_map_tombstones, a_rebuild_with_few_live_ids_takes_only_the_room_they_need)
{ MockPlatform platform;
  Objects map(platform, Pool::paged);
  for (std::uint32_t key = 0u; key < 10000u; key++) CHECK_STATUS(map.insert(key, Object{key, 0u}), Status::ok);
  const std::uint32_t full = map.capacity();
  CHECK_EQ(map.remove_if([](std::uint32_t key, Object &) { return key >= 10u; }), 9990u);
  CHECK_EQ(map.capacity(), full);
  for (std::uint32_t key = 20000u; key < 120000u; key++) {
      CHECK_STATUS(map.insert(key, Object{0u, 0u}), Status::ok);
      CHECK(map.remove(key));
  }
  CHECK_EQ(map.capacity(), 64u);
  for (std::uint32_t kept = 0u; kept < 10u; kept++) CHECK(names(map, kept, kept));
  CHECK(map.invariants_hold()); }

TEST(id_map_hostile, colliding_ids_past_every_rebuild_are_all_found)
{ MockPlatform platform;
  Objects map(platform, Pool::paged);
  for (std::uint32_t round = 0u; round < 2000u; round++) {
      const std::uint32_t key = round * 0x10000u;
      CHECK_STATUS(map.insert(key, Object{round, 0u}), Status::ok);
  }
  for (std::uint32_t round = 0u; round < 2000u; round++) CHECK(names(map, round * 0x10000u, round));
  for (std::uint32_t round = 0u; round < 2000u; round += 3u) CHECK(map.remove(round * 0x10000u));
  for (std::uint32_t round = 0u; round < 2000u; round++)
      CHECK(round % 3u == 0u ? map.find(round * 0x10000u) == nullptr : names(map, round * 0x10000u, round));
  CHECK(map.invariants_hold()); }

TEST(id_map_reserve, makes_room_so_inserts_up_to_it_allocate_nothing)
{ MockPlatform platform;
  Objects map(platform, Pool::paged);
  CHECK_STATUS(map.insert(1u, Object{1u, 0u}), Status::ok);
  CHECK_STATUS(map.reserve(1000u), Status::ok);
  CHECK(map.invariants_hold());
  platform.fail_allocation_at(1u);
  for (std::uint32_t key = 2u; key <= 1000u; key++) CHECK_STATUS(map.insert(key, Object{key, 0u}), Status::ok);
  CHECK_EQ(platform.live_allocations(), 1u);
  std::uint32_t key = 1001u;
  while (map.insert(key, Object{key, 0u}) == Status::ok) key++;
  CHECK_EQ(map.count(), map.capacity() / 2u);
  CHECK(map.find(key) == nullptr);
  CHECK(map.invariants_hold()); }

TEST(id_map_reserve, asks_for_nothing_when_the_room_is_already_there)
{ MockPlatform platform;
  Objects map(platform, Pool::paged);
  CHECK_STATUS(map.reserve(0u), Status::ok);
  CHECK_EQ(platform.live_allocations(), 0u);
  CHECK_STATUS(map.insert(1u, Object{1u, 0u}), Status::ok);
  CHECK_STATUS(map.insert(2u, Object{2u, 0u}), Status::ok);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(map.reserve(1u), Status::ok);
  CHECK_STATUS(map.reserve(Objects::first_capacity / 2u), Status::ok);
  CHECK_STATUS(map.reserve(Objects::first_capacity / 2u + 1u), Status::no_memory);
  CHECK_EQ(map.capacity(), Objects::first_capacity);
  CHECK(names(map, 2u, 2u));
  CHECK(map.invariants_hold()); }

TEST(id_map_reserve, refuses_more_than_a_table_may_hold)
{ MockPlatform platform;
  Objects map(platform, Pool::paged);
  CHECK_STATUS(map.reserve(Objects::most_entries / 4u + 1u), Status::no_memory);
  CHECK_STATUS(map.reserve(0xFFFFFFFFu), Status::no_memory);
  CHECK_EQ(platform.live_allocations(), 0u);
  CHECK(map.invariants_hold()); }

TEST(id_map_for_each, visits_every_live_id_once_and_may_change_values)
{ MockPlatform platform;
  Objects map(platform, Pool::paged);
  for (std::uint32_t key = 1u; key <= 50u; key++) CHECK_STATUS(map.insert(key, Object{key, 0u}), Status::ok);
  CHECK(map.remove(25u));
  std::uint64_t sum = 0u;
  std::uint32_t visits = 0u;
  map.for_each([&](std::uint32_t key, Object &object) {
      sum += key;
      visits++;
      object.kind = key + 1u;
  });
  CHECK_EQ(visits, 49u);
  CHECK_EQ(sum, 50u * 51u / 2u - 25u);
  const Objects &view = map;
  std::uint32_t mismatched = 0u;
  view.for_each([&](std::uint32_t key, const Object &object) { mismatched += object.kind != key + 1u; });
  CHECK_EQ(mismatched, 0u);
  Objects empty(platform, Pool::paged);
  empty.for_each([&](std::uint32_t, Object &) { visits++; });
  CHECK_EQ(visits, 49u); }

TEST(id_map_remove_if, removes_what_matches_and_lets_the_predicate_see_each_value)
{ MockPlatform platform;
  Objects map(platform, Pool::paged);
  for (std::uint32_t key = 0u; key < 64u; key++) CHECK_STATUS(map.insert(key, Object{key, key % 3u}), Status::ok);
  std::uint64_t released = 0u;
  const std::uint32_t gone = map.remove_if([&](std::uint32_t, Object &object) {
      if (object.kind != 0u) return false;
      released += object.address;
      return true;
  });
  CHECK_EQ(gone, 22u);
  CHECK_EQ(map.count(), 42u);
  CHECK_EQ(released, 0u + 3u + 6u + 9u + 12u + 15u + 18u + 21u + 24u + 27u + 30u + 33u + 36u + 39u + 42u + 45u + 48u +
                         51u + 54u + 57u + 60u + 63u);
  for (std::uint32_t key = 0u; key < 64u; key++) CHECK((map.find(key) == nullptr) == (key % 3u == 0u));
  CHECK(map.invariants_hold()); }

TEST(id_map_clear, gives_the_table_back_and_forgets_every_id)
{ MockPlatform platform;
  Objects map(platform, Pool::paged);
  for (std::uint32_t key = 0u; key < 100u; key++) CHECK_STATUS(map.insert(key, Object{key, 0u}), Status::ok);
  map.clear();
  CHECK(map.invariants_hold());
  CHECK_EQ(map.count(), 0u);
  CHECK_EQ(platform.live_allocations(), 0u);
  CHECK(map.find(5u) == nullptr);
  CHECK_STATUS(map.insert(5u, Object{6u, 0u}), Status::ok);
  CHECK(names(map, 5u, 6u)); }

TEST(id_map_destructor, gives_the_table_back)
{ MockPlatform platform;
  { Objects map(platform, Pool::paged);
    CHECK_STATUS(map.insert(1u, Object{1u, 0u}), Status::ok); }
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(id_map_invariants, fail_for_wrong_counts_a_bad_size_or_a_stranded_entry)
{ MockPlatform platform;
  Objects map(platform, Pool::paged);
  IdMapProbe::corrupt_count(map, 1u);
  CHECK(!map.invariants_hold());
  IdMapProbe::corrupt_count(map, 0u);
  IdMapProbe::corrupt_tombstones(map, 1u);
  CHECK(!map.invariants_hold());
  IdMapProbe::corrupt_tombstones(map, 0u);
  IdMapProbe::corrupt_capacity(map, 16u);
  CHECK(!map.invariants_hold());
  IdMapProbe::corrupt_capacity(map, 0u);
  for (std::uint32_t key = 0u; key < 4u; key++) CHECK_STATUS(map.insert(key, Object{key, 0u}), Status::ok);
  CHECK(map.remove(3u));
  CHECK(map.invariants_hold());
  IdMapProbe::corrupt_count(map, 4u);
  CHECK(!map.invariants_hold());
  IdMapProbe::corrupt_count(map, 3u);
  IdMapProbe::corrupt_tombstones(map, 0u);
  CHECK(!map.invariants_hold());
  IdMapProbe::corrupt_tombstones(map, 1u);
  IdMapProbe::corrupt_capacity(map, 8u);
  CHECK(!map.invariants_hold());
  IdMapProbe::corrupt_capacity(map, 12u);
  CHECK(!map.invariants_hold());
  IdMapProbe::corrupt_capacity(map, 16u);
  IdMapProbe::corrupt_tombstones(map, 6u);
  CHECK(!map.invariants_hold());
  IdMapProbe::corrupt_tombstones(map, 1u);
  CHECK(map.invariants_hold());
  std::uint32_t stranded = 0u;
  for (std::uint32_t key = 0u; key < 3u && stranded == 0u; key++)
      if (IdMapProbe::strand(map, key)) stranded = 1u;
  CHECK_EQ(stranded, 1u);
  CHECK(!map.invariants_hold()); }
