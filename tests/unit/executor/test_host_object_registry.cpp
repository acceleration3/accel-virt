#include "executor/host_object_registry.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

namespace {

enum class Kind : std::uint32_t { context = 1u, queue = 2u, mem = 3u, event = 4u };

struct Detail {
    int tag;
};

using Registry = av::HostObjectRegistry<std::uint64_t, Kind, Detail>;

}

namespace av {

struct HostObjectRegistryProbe {
    static void set_references(Registry &registry, std::uint64_t key, std::uint32_t references)
    {
        registry.entries_.find(key)->references_ = references;
    }
    static void set_parent_to_itself(Registry &registry, std::uint64_t key)
    {
        registry.entries_.find(key)->parent_ = registry.entries_.slot_of(key);
    }
};

}

using av::HostObjectRegistryProbe;
using av::MockPlatform;
using av::Status;

TEST(host_object_registry, a_made_object_is_found_by_its_key_and_kind)
{ MockPlatform platform;
  Registry     registry(platform);
  CHECK(registry.remember(0x10u, Kind::mem, 0u, Detail{7}) == Status::ok);
  CHECK(registry.holds(0x10u));
  CHECK(registry.find(0x10u, Kind::mem) != nullptr);
  CHECK_EQ(registry.find(0x10u, Kind::mem)->tag, 7);
  CHECK(registry.find(0x10u, Kind::event) == nullptr);
  CHECK(registry.find(0x11u, Kind::mem) == nullptr);
  Kind kind = Kind::context;
  CHECK(registry.find_any(0x10u, kind) != nullptr);
  CHECK(kind == Kind::mem);
  CHECK(registry.find_any(0x11u, kind) == nullptr);
  CHECK_EQ(registry.count(), 1u);
  CHECK(registry.invariants_hold()); }

TEST(host_object_registry, zero_is_never_a_key)
{ MockPlatform platform;
  Registry     registry(platform);
  CHECK(registry.remember(0u, Kind::mem, 0u, Detail{}) == Status::invalid_parameter);
  CHECK(!registry.holds(0u));
  CHECK(registry.find(0u, Kind::mem) == nullptr);
  CHECK(registry.parent_of(0u) == 0u);
  CHECK_EQ(registry.slot_of(0u), Registry::no_slot);
  registry.retain(0u, Kind::mem);
  CHECK(!registry.release(0u, Kind::mem));
  registry.forget(0u);
  CHECK_EQ(registry.count(), 0u);
  CHECK(registry.invariants_hold()); }

TEST(host_object_registry, a_key_already_held_is_refused)
{ MockPlatform platform;
  Registry     registry(platform);
  CHECK(registry.remember(0x10u, Kind::mem, 0u, Detail{1}) == Status::ok);
  CHECK(registry.remember(0x10u, Kind::event, 0u, Detail{2}) == Status::conflict);
  CHECK_EQ(registry.find(0x10u, Kind::mem)->tag, 1);
  CHECK_EQ(registry.count(), 1u);
  CHECK(registry.invariants_hold()); }

TEST(host_object_registry, the_last_release_forgets_it_and_a_retain_defers_that)
{ MockPlatform platform;
  Registry     registry(platform);
  CHECK(registry.remember(0x10u, Kind::context, 0u, Detail{}) == Status::ok);
  registry.retain(0x10u, Kind::context);
  CHECK(!registry.release(0x10u, Kind::context));
  CHECK(registry.holds(0x10u));
  CHECK(registry.invariants_hold());
  CHECK(registry.release(0x10u, Kind::context));
  CHECK(!registry.holds(0x10u));
  CHECK_EQ(registry.count(), 0u);
  CHECK(registry.invariants_hold()); }

TEST(host_object_registry, retain_and_release_of_an_unknown_or_mistyped_object_change_nothing)
{ MockPlatform platform;
  Registry     registry(platform);
  CHECK(registry.remember(0x10u, Kind::mem, 0u, Detail{}) == Status::ok);
  CHECK(!registry.release(0x10u, Kind::event));
  registry.retain(0x10u, Kind::event);
  CHECK(!registry.release(0x20u, Kind::mem));
  registry.retain(0x20u, Kind::mem);
  CHECK(registry.release(0x10u, Kind::mem));
  CHECK_EQ(registry.count(), 0u);
  CHECK(registry.invariants_hold()); }

TEST(host_object_registry, a_saturated_reference_count_stays_held)
{ MockPlatform platform;
  Registry     registry(platform);
  CHECK(registry.remember(0x10u, Kind::mem, 0u, Detail{}) == Status::ok);
  HostObjectRegistryProbe::set_references(registry, 0x10u, Registry::saturated);
  registry.retain(0x10u, Kind::mem);
  CHECK(!registry.release(0x10u, Kind::mem));
  CHECK(registry.holds(0x10u));
  CHECK(registry.invariants_hold()); }

TEST(host_object_registry, forgetting_a_parent_forgets_what_was_made_through_it)
{ MockPlatform platform;
  Registry     registry(platform);
  CHECK(registry.remember(0x10u, Kind::context, 0u, Detail{}) == Status::ok);
  CHECK(registry.remember(0x20u, Kind::queue, 0x10u, Detail{}) == Status::ok);
  CHECK(registry.remember(0x30u, Kind::event, 0x20u, Detail{}) == Status::ok);
  CHECK(registry.remember(0x40u, Kind::mem, 0u, Detail{}) == Status::ok);
  CHECK(registry.parent_of(0x30u) == 0x20u);
  CHECK(registry.parent_of(0x10u) == 0u);
  CHECK(registry.invariants_hold());
  registry.forget(0x10u);
  CHECK(!registry.holds(0x10u));
  CHECK(!registry.holds(0x20u));
  CHECK(!registry.holds(0x30u));
  CHECK(registry.holds(0x40u));
  CHECK_EQ(registry.count(), 1u);
  CHECK(registry.invariants_hold()); }

TEST(host_object_registry, the_last_release_of_a_parent_sweeps_its_children)
{ MockPlatform platform;
  Registry     registry(platform);
  CHECK(registry.remember(0x10u, Kind::context, 0u, Detail{}) == Status::ok);
  CHECK(registry.remember(0x20u, Kind::queue, 0x10u, Detail{}) == Status::ok);
  CHECK(registry.release(0x10u, Kind::context));
  CHECK_EQ(registry.count(), 0u);
  CHECK(registry.invariants_hold()); }

TEST(host_object_registry, forgetting_the_children_leaves_the_parent)
{ MockPlatform platform;
  Registry     registry(platform);
  CHECK(registry.remember(0x10u, Kind::context, 0u, Detail{}) == Status::ok);
  CHECK(registry.remember(0x20u, Kind::queue, 0x10u, Detail{}) == Status::ok);
  CHECK(registry.remember(0x30u, Kind::event, 0x20u, Detail{}) == Status::ok);
  CHECK(registry.remember(0x40u, Kind::mem, 0x10u, Detail{}) == Status::ok);
  registry.forget_children(0x10u);
  CHECK(registry.holds(0x10u));
  CHECK_EQ(registry.count(), 1u);
  CHECK(registry.invariants_hold());
  registry.forget_children(0x10u);
  registry.forget_children(0x99u);
  CHECK_EQ(registry.count(), 1u); }

TEST(host_object_registry, children_are_walked_through_their_parent_alone_after_others_come_and_go)
{ MockPlatform platform;
  Registry     registry(platform);
  CHECK(registry.remember(0x10u, Kind::context, 0u, Detail{}) == Status::ok);
  CHECK(registry.remember(0x11u, Kind::context, 0u, Detail{}) == Status::ok);
  for (std::uint64_t key = 0x100u; key < 0x140u; key++)
      CHECK(registry.remember(key, Kind::mem, key % 2u == 0u ? 0x10u : 0x11u, Detail{}) == Status::ok);
  for (std::uint64_t key = 0x100u; key < 0x140u; key += 4u) CHECK(registry.release(key, Kind::mem));
  CHECK(registry.remember(0x200u, Kind::mem, 0x10u, Detail{}) == Status::ok);
  std::uint64_t seen = 0u;
  std::uint32_t walked = 0u;
  registry.for_each_child(0x10u, [&](std::uint64_t key) {
      CHECK_EQ(registry.parent_of(key), 0x10u);
      seen += key;
      walked++;
  });
  std::uint64_t wanted = 0x200u;
  std::uint32_t expected = 1u;
  for (std::uint64_t key = 0x100u; key < 0x140u; key += 2u)
      if (key % 4u != 0u) {
          wanted += key;
          expected++;
      }
  CHECK_EQ(walked, expected);
  CHECK_EQ(seen, wanted);
  CHECK(registry.invariants_hold()); }

TEST(host_object_registry, forgetting_a_parent_takes_its_whole_subtree_and_nothing_else)
{ MockPlatform platform;
  Registry     registry(platform);
  CHECK(registry.remember(1u, Kind::context, 0u, Detail{}) == Status::ok);
  CHECK(registry.remember(2u, Kind::queue, 1u, Detail{}) == Status::ok);
  CHECK(registry.remember(3u, Kind::mem, 2u, Detail{}) == Status::ok);
  CHECK(registry.remember(4u, Kind::mem, 2u, Detail{}) == Status::ok);
  CHECK(registry.remember(5u, Kind::queue, 1u, Detail{}) == Status::ok);
  CHECK(registry.remember(6u, Kind::context, 0u, Detail{}) == Status::ok);
  CHECK(registry.remember(7u, Kind::mem, 6u, Detail{}) == Status::ok);
  registry.forget(2u);
  CHECK(!registry.holds(2u));
  CHECK(!registry.holds(3u));
  CHECK(!registry.holds(4u));
  CHECK(registry.holds(5u));
  CHECK(registry.holds(7u));
  CHECK_EQ(registry.count(), 4u);
  std::uint32_t kept = 0u;
  registry.for_each_child(1u, [&](std::uint64_t key) {
      CHECK_EQ(key, 5u);
      kept++;
  });
  CHECK_EQ(kept, 1u);
  CHECK(registry.invariants_hold()); }

TEST(host_object_registry, a_parent_that_is_not_held_makes_an_object_with_none)
{ MockPlatform platform;
  Registry     registry(platform);
  CHECK(registry.remember(0x20u, Kind::queue, 0x10u, Detail{}) == Status::ok);
  CHECK(registry.parent_of(0x20u) == 0u);
  CHECK(registry.invariants_hold()); }

TEST(host_object_registry, holds_ten_thousand_objects_and_each_is_found)
{ MockPlatform platform;
  Registry     registry(platform);
  constexpr std::uint64_t many = 10000u;
  for (std::uint64_t key = 1u; key <= many; key++)
      CHECK(registry.remember(key, Kind::mem, 0u, Detail{static_cast<int>(key)}) == Status::ok);
  CHECK_EQ(registry.count(), many);
  for (std::uint64_t key = 1u; key <= many; key++) CHECK_EQ(registry.find(key, Kind::mem)->tag, static_cast<int>(key));
  for (std::uint64_t key = 2u; key <= many; key += 2u) CHECK(registry.release(key, Kind::mem));
  CHECK_EQ(registry.count(), many / 2u);
  CHECK(registry.invariants_hold()); }

TEST(host_object_registry, room_made_ahead_keeps_remembering_working_when_allocation_fails)
{ MockPlatform platform;
  Registry     registry(platform);
  CHECK(registry.remember(1u, Kind::context, 0u, Detail{}) == Status::ok);
  CHECK(registry.make_room(100u) == Status::ok);
  platform.fail_allocation_at(1u);
  for (std::uint64_t key = 2u; key <= 101u; key++) CHECK(registry.remember(key, Kind::event, 0u, Detail{}) == Status::ok);
  std::uint64_t next = 102u;
  while (registry.remember(next, Kind::event, 0u, Detail{}) == Status::ok) next++;
  CHECK(!registry.holds(next));
  CHECK(registry.holds(1u));
  CHECK(registry.make_room(~std::uint64_t{0}) == Status::no_memory);
  CHECK(registry.invariants_hold()); }

TEST(host_object_registry, kinds_are_counted_and_released_by_range)
{ MockPlatform platform;
  Registry     registry(platform);
  CHECK(registry.remember(1u, Kind::context, 0u, Detail{}) == Status::ok);
  CHECK(registry.remember(2u, Kind::queue, 0u, Detail{}) == Status::ok);
  CHECK(registry.remember(3u, Kind::mem, 0u, Detail{}) == Status::ok);
  CHECK(registry.remember(4u, Kind::event, 0u, Detail{}) == Status::ok);
  CHECK_EQ(registry.count_kinds(Kind::queue, Kind::mem), 2u);
  CHECK_EQ(registry.count_kinds(Kind::event, Kind::event), 1u);
  std::uint64_t released = 0u;
  registry.release_kinds(Kind::mem, Kind::event, [&](std::uint64_t key, Kind, Detail &, std::uint32_t times) {
      CHECK_EQ(times, 1u);
      released |= key;
  });
  CHECK_EQ(released, 3u | 4u);
  CHECK_EQ(registry.count(), 2u);
  CHECK(registry.holds(1u));
  CHECK(registry.holds(2u));
  CHECK(registry.invariants_hold()); }

TEST(host_object_registry, releasing_everything_lets_go_each_reference_in_the_order_given)
{ MockPlatform platform;
  Registry     registry(platform);
  CHECK(registry.remember(1u, Kind::context, 0u, Detail{}) == Status::ok);
  CHECK(registry.remember(2u, Kind::queue, 1u, Detail{}) == Status::ok);
  CHECK(registry.remember(3u, Kind::mem, 0u, Detail{}) == Status::ok);
  CHECK(registry.remember(4u, Kind::mem, 0u, Detail{}) == Status::ok);
  registry.retain(3u, Kind::mem);
  registry.retain(3u, Kind::mem);
  HostObjectRegistryProbe::set_references(registry, 4u, Registry::saturated);
  const Kind    order[] = {Kind::mem, Kind::queue, Kind::context};
  std::uint64_t keys[8] = {};
  std::uint32_t times[8] = {};
  std::uint32_t calls = 0u;
  registry.release_all(order, 3u, [&](std::uint64_t key, Kind, Detail &, std::uint32_t held) {
      keys[calls] = key;
      times[calls] = held;
      calls++;
  });
  CHECK_EQ(calls, 4u);
  CHECK_EQ(keys[2], 2u);
  CHECK_EQ(keys[3], 1u);
  CHECK((keys[0] == 3u && keys[1] == 4u) || (keys[0] == 4u && keys[1] == 3u));
  CHECK_EQ(times[0] + times[1], 3u + 1u);
  CHECK_EQ(times[2], 1u);
  CHECK_EQ(registry.count(), 0u);
  CHECK(registry.invariants_hold()); }

TEST(host_object_registry, forgetting_everything_leaves_nothing_held)
{ MockPlatform platform;
  Registry     registry(platform);
  CHECK(registry.remember(1u, Kind::context, 0u, Detail{}) == Status::ok);
  CHECK(registry.remember(2u, Kind::queue, 1u, Detail{}) == Status::ok);
  registry.forget_all();
  CHECK_EQ(registry.count(), 0u);
  CHECK(!registry.holds(1u));
  CHECK(registry.invariants_hold()); }

TEST(host_object_registry, slots_are_walked_and_read_back)
{ MockPlatform platform;
  Registry     registry(platform);
  CHECK(registry.remember(0x10u, Kind::mem, 0u, Detail{5}) == Status::ok);
  CHECK(registry.remember(0x20u, Kind::event, 0u, Detail{6}) == Status::ok);
  std::uint32_t seen = 0u;
  for (std::uint32_t slot = registry.next_held(0u); slot != Registry::no_slot; slot = registry.next_held(slot + 1u)) {
      std::uint64_t key = 0u;
      Kind          kind = Kind::context;
      Detail *const detail = registry.at(slot, key, kind);
      CHECK(detail != nullptr);
      CHECK_EQ(registry.slot_of(key), slot);
      CHECK_EQ(detail->tag, key == 0x10u ? 5 : 6);
      seen++;
  }
  CHECK_EQ(seen, 2u); }

TEST(host_object_registry_invariants, an_object_with_no_references_breaks_them)
{ MockPlatform platform;
  Registry     registry(platform);
  CHECK(registry.remember(0x10u, Kind::mem, 0u, Detail{}) == Status::ok);
  HostObjectRegistryProbe::set_references(registry, 0x10u, 0u);
  CHECK(!registry.invariants_hold()); }

TEST(host_object_registry_invariants, an_object_made_through_itself_breaks_them)
{ MockPlatform platform;
  Registry     registry(platform);
  CHECK(registry.remember(0x10u, Kind::mem, 0u, Detail{}) == Status::ok);
  HostObjectRegistryProbe::set_parent_to_itself(registry, 0x10u);
  CHECK(!registry.invariants_hold()); }
