#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/common/open_mappings.hpp"
#include "icd/common/staged_mapping.hpp"

#include <cstdint>

using av::MockPlatform;
using av::Status;
using av::icd::OpenMappings;
using av::icd::StagedMapping;

namespace av {
struct KeyedSlotsProbe {
    template <typename Key, typename Entry>
    static void corrupt_free_count(KeyedSlots<Key, Entry> &table, std::uint32_t count) { table.free_count_ = count; }
};

}

namespace av::icd {
struct OpenMappingsProbe {
    template <typename Maps>
    static void miscount_rows(Maps &maps, std::uint32_t count) { KeyedSlotsProbe::corrupt_free_count(maps.rows_, count); }
    template <typename Maps>
    static void miscount_objects(Maps &maps, std::uint32_t count) { KeyedSlotsProbe::corrupt_free_count(maps.objects_, count); }
    template <typename Maps>
    static void miscount_pointers(Maps &maps, std::uint32_t count) { KeyedSlotsProbe::corrupt_free_count(maps.pointers_, count); }
    template <typename Maps>
    static void set_object_sharers(Maps &maps, std::uint64_t object, std::uint32_t sharers)
    { maps.objects_.find(object)->sharers_ = sharers; }
    template <typename Maps>
    static void set_pointer_sharers(Maps &maps, const void *pointer, std::uint32_t sharers)
    { maps.pointers_.find(maps.pointer_key(pointer))->sharers_ = sharers; }
    template <typename Maps>
    static void drop_row_of(Maps &maps, std::uint64_t object)
    { maps.rows_.erase_slot(maps.objects_.find(object)->slot_); }
};

}

using av::icd::OpenMappingsProbe;

namespace {

constexpr std::uint32_t discard = 0x2u;
constexpr std::uint32_t no_overwrite = 0x1u;
constexpr std::uint64_t bytes = 64u;

using Maps = OpenMappings<StagedMapping>;

StagedMapping mapping(std::uint64_t object, std::uint32_t subresource, std::uint32_t flags, std::uint64_t size = bytes)
{
    return StagedMapping(object, subresource, 0u, size, flags);
}

}

TEST(open_mappings, any_subresource_is_found_with_its_own_flags_and_gone_once_closed)
{ MockPlatform platform;
  Maps maps(platform);
  StagedMapping *first = nullptr, *second = nullptr;
  CHECK_STATUS(maps.stage(mapping(7u, 59u, discard), first), Status::ok);
  CHECK_STATUS(maps.stage(mapping(7u, 3u, no_overwrite), second), Status::ok);
  CHECK(first != nullptr && second != nullptr);
  CHECK(maps.invariants_hold());
  CHECK(maps.find(7u, 59u) == first);
  CHECK(maps.find(7u, 3u) == second);
  CHECK_EQ(maps.find(7u, 59u)->flags(), discard);
  CHECK_EQ(maps.find(7u, 3u)->flags(), no_overwrite);
  CHECK(maps.find(7u, 4u) == nullptr);
  maps.close(*maps.find(7u, 59u));
  CHECK(maps.find(7u, 59u) == nullptr);
  CHECK(maps.find(7u, 3u) != nullptr);
  CHECK(maps.invariants_hold());
  maps.close(*maps.find(7u, 3u));
  CHECK_EQ(maps.count(), 0u);
  CHECK(maps.invariants_hold()); }

TEST(open_mappings, a_map_of_no_object_or_no_bytes_is_refused_and_a_closed_map_is_not_found)
{ MockPlatform platform;
  Maps maps(platform);
  StagedMapping *opened = nullptr;
  CHECK(maps.find(7u, 0u) == nullptr);
  CHECK_STATUS(maps.stage(mapping(0u, 0u, discard), opened), Status::invalid_parameter);
  CHECK(opened == nullptr);
  CHECK_STATUS(maps.stage(mapping(7u, 0u, discard, 0u), opened), Status::invalid_parameter);
  CHECK_STATUS(maps.lend(mapping(0u, 0u, discard), nullptr, opened), Status::invalid_parameter);
  std::uint8_t memory[bytes] = {};
  CHECK_STATUS(maps.lend(mapping(7u, 0u, discard), nullptr, opened), Status::invalid_parameter);
  CHECK_STATUS(maps.lend(mapping(0u, 0u, discard), memory, opened), Status::invalid_parameter);
  CHECK_EQ(maps.count(), 0u);
  CHECK_STATUS(maps.stage(mapping(7u, 0u, 0u), opened), Status::ok);
  maps.close(*opened);
  CHECK(maps.find(7u, 0u) == nullptr);
  CHECK(maps.invariants_hold()); }

TEST(open_mappings, a_staged_map_owns_zeroed_memory_and_a_lent_map_only_names_the_callers)
{ MockPlatform platform;
  Maps maps(platform);
  StagedMapping *staged = nullptr, *lent = nullptr;
  CHECK_STATUS(maps.stage(mapping(4u, 0u, 0u), staged), Status::ok);
  maps.close(*staged);
  const std::uint32_t tables = platform.live_allocations();
  CHECK_STATUS(maps.stage(mapping(4u, 0u, 0u), staged), Status::ok);
  CHECK(staged->copied());
  CHECK(staged->at() != nullptr);
  for (std::uint64_t index = 0u; index < bytes; index++) CHECK_EQ(staged->at()[index], 0u);
  CHECK_EQ(platform.live_allocations(), tables + 1u);
  std::uint8_t memory[bytes] = {};
  CHECK_STATUS(maps.lend(mapping(5u, 0u, discard), memory, lent), Status::ok);
  CHECK(!lent->copied());
  CHECK(lent->at() == memory);
  CHECK(maps.invariants_hold());
  maps.close(*staged);
  maps.close(*maps.find(5u, 0u));
  CHECK_EQ(platform.live_allocations(), tables);
  CHECK(maps.invariants_hold()); }

TEST(open_mappings, a_pointer_answers_only_for_the_object_whose_memory_it_points_into)
{ MockPlatform platform;
  Maps maps(platform);
  StagedMapping *first = nullptr, *second = nullptr;
  CHECK_STATUS(maps.stage(mapping(4u, 0u, 0u), first), Status::ok);
  CHECK_STATUS(maps.stage(mapping(5u, 0u, 0u), second), Status::ok);
  CHECK(maps.answering(4u, first->at()) == first);
  CHECK(maps.answering(5u, second->at()) == second);
  CHECK(maps.answering(5u, first->at()) == nullptr);
  CHECK(maps.answering(4u, nullptr) == nullptr);
  CHECK(maps.answering(6u, second->at()) == nullptr);
  maps.close(*first);
  CHECK(maps.answering(4u, second->at()) == nullptr);
  CHECK(maps.answering(5u, second->at()) == second);
  CHECK(maps.invariants_hold()); }

TEST(open_mappings, a_destroyed_object_takes_its_maps_and_only_its)
{ MockPlatform platform;
  Maps maps(platform);
  StagedMapping *opened = nullptr;
  CHECK_STATUS(maps.stage(mapping(4u, 0u, 0u), opened), Status::ok);
  CHECK_STATUS(maps.stage(mapping(5u, 0u, discard), opened), Status::ok);
  CHECK_STATUS(maps.stage(mapping(4u, 2u, 0u), opened), Status::ok);
  CHECK_STATUS(maps.stage(mapping(4u, 9u, 0u), opened), Status::ok);
  const std::uint32_t copies = platform.live_allocations();
  CHECK_EQ(maps.close_if([](const StagedMapping &open) { return open.object() == 4u; }), 3u);
  CHECK_EQ(maps.count(), 1u);
  CHECK(maps.invariants_hold());
  CHECK(maps.find(4u, 0u) == nullptr);
  CHECK(maps.find(4u, 9u) == nullptr);
  const StagedMapping *const kept = maps.find(5u, 0u);
  CHECK(kept != nullptr);
  CHECK_EQ(kept->flags(), discard);
  CHECK_EQ(platform.live_allocations(), copies - 3u);
  maps.close_all();
  CHECK_EQ(maps.count(), 0u);
  CHECK_EQ(platform.live_allocations(), copies - 4u);
  CHECK(maps.invariants_hold()); }

TEST(open_mappings, holds_every_map_an_application_keeps_open)
{ MockPlatform platform;
  Maps maps(platform);
  static std::uint8_t memory[5001u * 8u];
  for (std::uint64_t object = 1u; object <= 5000u; object++) {
      StagedMapping *opened = nullptr;
      CHECK_STATUS(maps.lend(mapping(object, static_cast<std::uint32_t>(object % 7u),
                                     object % 2u == 0u ? discard : no_overwrite, 8u), memory + object * 8u, opened),
                   Status::ok);
  }
  CHECK_EQ(maps.count(), 5000u);
  CHECK(maps.invariants_hold());
  for (std::uint64_t object = 1u; object <= 5000u; object++) {
      StagedMapping *const open = maps.find(object, static_cast<std::uint32_t>(object % 7u));
      CHECK(open != nullptr);
      if (open == nullptr) continue;
      CHECK_EQ(open->flags(), object % 2u == 0u ? discard : no_overwrite);
      maps.close(*open);
  }
  CHECK_EQ(maps.count(), 0u);
  CHECK(maps.invariants_hold()); }

TEST(open_mappings, a_map_with_no_memory_for_it_is_refused_and_the_others_kept)
{ MockPlatform platform;
  Maps maps(platform);
  StagedMapping *opened = nullptr;
  for (std::uint32_t index = 0u; index < 8u; index++) CHECK_STATUS(maps.stage(mapping(1u, index, 0u), opened), Status::ok);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(maps.stage(mapping(2u, 0u, discard), opened), Status::no_memory);
  CHECK(opened == nullptr);
  CHECK_EQ(maps.count(), 8u);
  CHECK(maps.find(2u, 0u) == nullptr);
  CHECK(maps.find(1u, 5u) != nullptr);
  CHECK(maps.invariants_hold());
  CHECK_STATUS(maps.stage(mapping(2u, 0u, discard), opened), Status::ok);
  CHECK_EQ(maps.count(), 9u);
  CHECK(maps.invariants_hold()); }

TEST(open_mappings, reflagging_a_map_changes_its_flags_and_nothing_else)
{ MockPlatform platform;
  Maps maps(platform);
  StagedMapping *staged = nullptr;
  CHECK_STATUS(maps.stage(StagedMapping(6u, 2u, 16u, bytes, discard), staged), Status::ok);
  std::uint8_t *const at = staged->at();
  staged->reflag(no_overwrite);
  CHECK_EQ(staged->flags(), no_overwrite);
  CHECK_EQ(staged->object(), 6u);
  CHECK_EQ(staged->subresource(), 2u);
  CHECK_EQ(staged->offset(), 16u);
  CHECK_EQ(staged->bytes(), bytes);
  CHECK(staged->at() == at);
  CHECK(maps.find(6u, 2u)->flags() == no_overwrite);
  CHECK(maps.invariants_hold());
  maps.close(*staged); }

TEST(open_mappings, a_map_that_is_not_open_is_not_closed_and_a_later_map_of_an_object_closes_on_its_own)
{ MockPlatform platform;
  Maps maps(platform);
  StagedMapping *first = nullptr, *second = nullptr, *third = nullptr;
  std::uint8_t memory[3u * bytes] = {};
  CHECK_STATUS(maps.lend(mapping(7u, 1u, 0u), memory, first), Status::ok);
  CHECK_STATUS(maps.lend(mapping(7u, 2u, 0u), memory + bytes, second), Status::ok);
  CHECK_STATUS(maps.lend(mapping(7u, 3u, 0u), memory + 2u * bytes, third), Status::ok);
  StagedMapping stranger = mapping(99u, 0u, 0u);
  maps.close(stranger);
  StagedMapping twin = *first;
  maps.close(twin);
  CHECK_EQ(maps.count(), 3u);
  CHECK(maps.invariants_hold());
  maps.close(*third);
  CHECK_EQ(maps.count(), 2u);
  CHECK(maps.find(7u, 3u) == nullptr);
  CHECK(maps.find(7u, 2u) == second);
  CHECK(maps.find(7u, 1u) == first);
  CHECK(maps.invariants_hold());
  maps.close(*second);
  CHECK(maps.find(7u, 1u) == first);
  CHECK(maps.invariants_hold()); }

TEST(open_mappings, a_subresource_is_found_among_the_other_maps_of_a_busy_table)
{ MockPlatform platform;
  Maps maps(platform);
  StagedMapping *opened = nullptr;
  CHECK_STATUS(maps.stage(mapping(4u, 0u, 0u), opened), Status::ok);
  CHECK_STATUS(maps.stage(mapping(5u, 0u, 0u), opened), Status::ok);
  CHECK_STATUS(maps.stage(mapping(4u, 2u, 0u), opened), Status::ok);
  CHECK(maps.find(4u, 2u) == opened);
  CHECK(maps.find(4u, 9u) == nullptr);
  const Maps &view = maps;
  CHECK(view.find(4u, 2u) == opened);
  CHECK(maps.invariants_hold()); }

TEST(open_mappings, maps_sharing_one_memory_are_told_apart_by_their_object)
{ MockPlatform platform;
  Maps maps(platform);
  StagedMapping *first = nullptr, *second = nullptr, *third = nullptr;
  std::uint8_t memory[bytes] = {};
  CHECK_STATUS(maps.lend(mapping(1u, 0u, 0u), memory, first), Status::ok);
  CHECK_STATUS(maps.lend(mapping(2u, 0u, 0u), memory, second), Status::ok);
  CHECK_STATUS(maps.lend(mapping(3u, 0u, 0u), memory, third), Status::ok);
  CHECK(maps.answering(1u, memory) == first);
  CHECK(maps.answering(2u, memory) == second);
  CHECK(maps.answering(3u, memory) == third);
  CHECK(maps.answering(4u, memory) == nullptr);
  CHECK(maps.invariants_hold());
  maps.close(*first);
  CHECK(maps.answering(2u, memory) == second);
  CHECK(maps.answering(1u, memory) == nullptr);
  CHECK(maps.invariants_hold()); }

TEST(open_mappings, a_table_with_no_room_for_its_indexes_refuses_the_map_whole)
{ for (std::uint32_t failing = 1u; failing <= 6u; failing++) {
      MockPlatform platform;
      Maps maps(platform);
      StagedMapping *opened = nullptr;
      std::uint8_t memory[bytes] = {};
      platform.fail_allocation_at(failing);
      CHECK_STATUS(maps.lend(mapping(7u, 0u, 0u), memory, opened), Status::no_memory);
      CHECK(opened == nullptr);
      CHECK_EQ(maps.count(), 0u);
      CHECK(maps.invariants_hold());
      CHECK_STATUS(maps.lend(mapping(7u, 0u, 0u), memory, opened), Status::ok);
      CHECK_EQ(maps.count(), 1u);
      CHECK(maps.invariants_hold());
  } }

TEST(open_mappings, a_staged_map_with_no_room_for_its_indexes_gives_its_copy_back)
{ for (std::uint32_t failing = 2u; failing <= 7u; failing++) {
      MockPlatform platform;
      {
          Maps maps(platform);
          StagedMapping *opened = nullptr;
          platform.fail_allocation_at(failing);
          CHECK_STATUS(maps.stage(mapping(7u, 0u, 0u), opened), Status::no_memory);
          CHECK(opened == nullptr);
          CHECK_EQ(maps.count(), 0u);
          CHECK(maps.invariants_hold());
          CHECK_STATUS(maps.stage(mapping(7u, 0u, 0u), opened), Status::ok);
          CHECK_EQ(maps.count(), 1u);
      }
      CHECK_EQ(platform.live_allocations(), 0u);
  } }

TEST(open_mappings, a_default_map_owns_no_memory_and_a_copy_of_no_bytes_is_broken)
{ StagedMapping none;
  CHECK(!none.copied());
  CHECK(none.at() == nullptr);
  std::uint8_t memory[bytes] = {};
  StagedMapping empty(7u, 0u, 0u, 0u, 0u);
  empty.hand(memory, StagedMapping::Memory::copy);
  CHECK(!empty.invariants_hold());
  empty.hand(memory, StagedMapping::Memory::named);
  CHECK(empty.invariants_hold()); }

TEST(open_mappings, its_invariants_catch_a_broken_map_row_or_index)
{ MockPlatform platform;
  Maps maps(platform);
  StagedMapping *opened = nullptr;
  std::uint8_t memory[bytes] = {};
  CHECK_STATUS(maps.lend(mapping(7u, 0u, 0u), memory, opened), Status::ok);
  CHECK(maps.invariants_hold());

  StagedMapping *other = nullptr;
  std::uint8_t more[bytes] = {};
  CHECK_STATUS(maps.lend(mapping(9u, 0u, 0u), more, other), Status::ok);
  opened->hand(nullptr, StagedMapping::Memory::named);
  CHECK(!maps.invariants_hold());
  opened->hand(memory, StagedMapping::Memory::named);
  CHECK(maps.invariants_hold());

  const StagedMapping whole = *opened;
  *opened = StagedMapping(0u, 0u, 0u, bytes, 0u);
  opened->hand(memory, StagedMapping::Memory::named);
  CHECK(!maps.invariants_hold());
  *opened = whole;
  CHECK(maps.invariants_hold());

  *opened = StagedMapping(8u, 0u, 0u, bytes, 0u);
  opened->hand(memory, StagedMapping::Memory::named);
  CHECK(!maps.invariants_hold());
  *opened = whole;
  CHECK(maps.invariants_hold());

  OpenMappingsProbe::miscount_rows(maps, 5u);
  CHECK(!maps.invariants_hold());
  OpenMappingsProbe::miscount_rows(maps, 0u);
  OpenMappingsProbe::miscount_objects(maps, 5u);
  CHECK(!maps.invariants_hold());
  OpenMappingsProbe::miscount_objects(maps, 0u);
  OpenMappingsProbe::miscount_pointers(maps, 5u);
  CHECK(!maps.invariants_hold());
  OpenMappingsProbe::miscount_pointers(maps, 0u);
  CHECK(maps.invariants_hold());
  maps.close_all();
  CHECK_EQ(maps.count(), 0u); }

TEST(open_mappings, its_invariants_catch_an_index_that_names_no_map_the_wrong_map_or_the_wrong_count)
{ MockPlatform platform;
  Maps maps(platform);
  StagedMapping *opened = nullptr;
  std::uint8_t memory[bytes] = {};
  CHECK_STATUS(maps.lend(mapping(7u, 0u, 0u), memory, opened), Status::ok);

  StagedMapping *other = nullptr;
  std::uint8_t more[bytes] = {};
  CHECK_STATUS(maps.lend(mapping(9u, 0u, 0u), more, other), Status::ok);
  const StagedMapping whole = *opened;
  *opened = StagedMapping(8u, 0u, 0u, bytes, 0u);
  opened->hand(memory, StagedMapping::Memory::named);
  CHECK(!maps.invariants_hold());
  *opened = whole;
  CHECK(maps.invariants_hold());

  OpenMappingsProbe::set_object_sharers(maps, 7u, 0u);
  CHECK(!maps.invariants_hold());
  OpenMappingsProbe::set_object_sharers(maps, 7u, 2u);
  CHECK(!maps.invariants_hold());
  OpenMappingsProbe::set_object_sharers(maps, 7u, 1u);
  OpenMappingsProbe::set_pointer_sharers(maps, memory, 2u);
  CHECK(!maps.invariants_hold());
  OpenMappingsProbe::set_pointer_sharers(maps, memory, 1u);
  CHECK(maps.invariants_hold());

  OpenMappingsProbe::drop_row_of(maps, 7u);
  CHECK_EQ(maps.count(), 1u);
  CHECK(!maps.invariants_hold()); }
