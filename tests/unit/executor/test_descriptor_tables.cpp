#include "executor/descriptor_tables.hpp"
#include "harness/executor/counted_object.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::CountedObject;
using Tables = av::DescriptorTables<CountedObject, void>;

namespace {

constexpr std::uint64_t owner_a = 5u;
constexpr std::uint64_t owner_b = 6u;

Tables::Slot slot_of(CountedObject *first, CountedObject *second = nullptr)
{
    return Tables::Slot{first, second, nullptr, nullptr};
}

}

TEST(descriptor_tables, a_slot_keeps_both_its_objects_alive_and_is_found_by_owner_and_key)
{ av::MockPlatform platform;
  Tables tables(platform);
  CountedObject first, second;
  Tables::Slot displaced = {};
  CHECK_STATUS(tables.put(owner_a, 9u, slot_of(&first, &second), displaced), av::Status::ok);
  CHECK(displaced.first_ == nullptr && displaced.second_ == nullptr);
  CHECK_EQ(first.references(), 2u);
  CHECK_EQ(second.references(), 2u);
  CHECK(tables.find(owner_a, 9u) != nullptr);
  CHECK(tables.find(owner_a, 8u) == nullptr);
  CHECK(tables.find(owner_b, 9u) == nullptr);
  CHECK_EQ(tables.count(), 1u);
  CHECK_EQ(tables.owners(), 1u);
  CHECK(tables.known(owner_a));
  CHECK(tables.invariants_hold()); }

TEST(descriptor_tables, putting_over_a_slot_hands_back_what_it_held_and_takes_the_new_references)
{ av::MockPlatform platform;
  Tables tables(platform);
  CountedObject old_one, new_one;
  Tables::Slot displaced = {};
  CHECK_STATUS(tables.put(owner_a, 1u, slot_of(&old_one), displaced), av::Status::ok);
  CHECK_STATUS(tables.put(owner_a, 1u, slot_of(&new_one), displaced), av::Status::ok);
  CHECK(displaced.first_ == &old_one);
  CHECK_EQ(old_one.references(), 2u);
  CHECK_EQ(new_one.references(), 2u);
  CHECK_EQ(tables.count(), 1u);
  CHECK(tables.invariants_hold()); }

TEST(descriptor_tables, putting_nothing_clears_the_slot_and_the_last_slot_takes_its_table_with_it)
{ av::MockPlatform platform;
  Tables tables(platform);
  CountedObject one;
  Tables::Slot displaced = {};
  CHECK_STATUS(tables.put(owner_a, 1u, slot_of(&one), displaced), av::Status::ok);
  CHECK_STATUS(tables.put(owner_a, 1u, slot_of(nullptr), displaced), av::Status::ok);
  CHECK(displaced.first_ == &one);
  CHECK_EQ(tables.count(), 0u);
  CHECK_EQ(tables.owners(), 0u);
  CHECK_STATUS(tables.put(owner_a, 1u, slot_of(nullptr), displaced), av::Status::ok);
  CHECK(displaced.first_ == nullptr);
  CHECK(tables.invariants_hold()); }

TEST(descriptor_tables, a_slot_that_cannot_be_kept_takes_no_reference_and_leaves_no_table)
{ av::MockPlatform platform;
  Tables tables(platform);
  CountedObject one;
  Tables::Slot displaced = {};
  platform.fail_allocation_at(1u);
  CHECK_STATUS(tables.put(owner_a, 1u, slot_of(&one), displaced), av::Status::no_memory);
  CHECK_EQ(one.references(), 1u);
  CHECK_EQ(tables.owners(), 0u);
  CHECK(tables.invariants_hold()); }

TEST(descriptor_tables, releasing_an_owner_lets_every_slot_go_with_the_table_s_mark)
{ av::MockPlatform platform;
  Tables tables(platform);
  CountedObject one, two;
  Tables::Slot displaced = {};
  CHECK_STATUS(tables.put(owner_a, 1u, slot_of(&one), displaced), av::Status::ok);
  CHECK_STATUS(tables.put(owner_a, 2u, slot_of(&two), displaced), av::Status::ok);
  tables.submitted(owner_a, 4u);
  tables.submitted(owner_a, 3u);
  tables.submitted(owner_b, 9u);
  CHECK_EQ(tables.mark_of(owner_a), 4u);
  CHECK_EQ(tables.mark_of(owner_b), 0u);
  std::uint64_t seen_mark = 0u;
  CHECK_STATUS(tables.release(owner_a, [&](Tables::Slot &slot, std::uint64_t mark) {
      seen_mark = mark;
      slot.first_->Release();
      slot.first_ = nullptr;
      return av::Status::ok;
  }), av::Status::ok);
  CHECK_EQ(seen_mark, 4u);
  CHECK_EQ(one.references(), 1u);
  CHECK_EQ(two.references(), 1u);
  CHECK_EQ(tables.count(), 0u);
  CHECK(!tables.known(owner_a));
  CHECK_STATUS(tables.release(owner_a, [](Tables::Slot &, std::uint64_t) { return av::Status::no_memory; }), av::Status::ok);
  CHECK(tables.invariants_hold()); }

TEST(descriptor_tables, a_release_that_fails_keeps_the_slots_not_yet_let_go_with_what_is_left_of_them)
{ av::MockPlatform platform;
  Tables tables(platform);
  CountedObject one, two;
  Tables::Slot displaced = {};
  CHECK_STATUS(tables.put(owner_a, 1u, slot_of(&one, &two), displaced), av::Status::ok);
  CHECK_STATUS(tables.release(owner_a, [](Tables::Slot &slot, std::uint64_t) {
      slot.first_->Release();
      slot.first_ = nullptr;
      return av::Status::no_memory;
  }), av::Status::no_memory);
  CHECK_EQ(tables.count(), 1u);
  CHECK(tables.find(owner_a, 1u)->first_ == nullptr);
  CHECK(tables.find(owner_a, 1u)->second_ == &two);
  CHECK(tables.invariants_hold()); }

TEST(descriptor_tables, draining_everything_visits_each_slot_once_even_when_the_visit_releases_another_owner)
{ av::MockPlatform platform;
  Tables tables(platform);
  CountedObject one, two, three;
  Tables::Slot displaced = {};
  CHECK_STATUS(tables.put(owner_a, 1u, slot_of(&one), displaced), av::Status::ok);
  CHECK_STATUS(tables.put(owner_a, 2u, slot_of(&two), displaced), av::Status::ok);
  CHECK_STATUS(tables.put(owner_b, 1u, slot_of(&three), displaced), av::Status::ok);
  std::uint32_t visited = 0u;
  tables.drain_all([&](Tables::Slot &slot) {
      visited++;
      slot.first_->Release();
      static_cast<void>(tables.release(owner_b, [](Tables::Slot &inner, std::uint64_t) {
          inner.first_->Release();
          inner.first_ = nullptr;
          return av::Status::ok;
      }));
  });
  CHECK(visited >= 2u && visited <= 3u);
  CHECK_EQ(tables.count(), 0u);
  CHECK_EQ(tables.owners(), 0u);
  CHECK_EQ(one.references(), 1u);
  CHECK_EQ(two.references(), 1u);
  CHECK_EQ(three.references(), 1u);
  CHECK(tables.invariants_hold()); }

TEST(descriptor_tables, the_slots_of_an_owner_are_visited_and_none_for_an_owner_without_a_table)
{ av::MockPlatform platform;
  Tables tables(platform);
  CountedObject one, two;
  Tables::Slot displaced = {};
  CHECK_STATUS(tables.put(owner_a, 1u, slot_of(&one), displaced), av::Status::ok);
  CHECK_STATUS(tables.put(owner_a, 2u, slot_of(&two), displaced), av::Status::ok);
  std::uint32_t visited = 0u;
  tables.for_each_slot(owner_a, [&](const Tables::Slot &) { visited++; });
  CHECK_EQ(visited, 2u);
  tables.for_each_slot(owner_b, [&](const Tables::Slot &) { visited++; });
  CHECK_EQ(visited, 2u);
  tables.drain_all([](Tables::Slot &slot) { slot.first_->Release(); }); }
