#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_context_rig.hpp"
#include "icd/opengl/buffer_mapping.hpp"
#include "icd/opengl/gl_context_table.hpp"
#include "icd/opengl/sliced_buffers.hpp"

#include <cstring>

using av::GlContextOptions;
using av::GlContextRig;
using av::RecordingThreadLock;
using av::Status;
using av::glicd::GlContextTable;
using av::glicd::GuestGlContext;

namespace av {

struct KeyedSlotsProbe {
    template <typename Key, typename Entry>
    static void corrupt_count(KeyedSlots<Key, Entry> &table, std::uint32_t count) { table.count_ = count; }

    template <typename Key, typename Entry>
    static void corrupt_free_count(KeyedSlots<Key, Entry> &table, std::uint32_t count) { table.free_count_ = count; }
};

}

namespace av::glicd {

struct SliceGroupsProbe {
    static void set_next(SliceGroups &groups, std::uint32_t next) { groups.next_ = next; }
};

struct GuestGlContextProbe {
    static void claim_made(GuestGlContext &context) { context.state_ = GuestGlContext::State::made; }
    static void unclaim(GuestGlContext &context) { context.state_ = GuestGlContext::State::created; }
};

struct GlContextTableProbe {
    static void last_handle(GlContextTable &table) { table.next_ = GlContextTable::last_handle; }
    static void zero_next(GlContextTable &table) { table.next_ = 0u; }
    static void restore_next(GlContextTable &table) { table.next_ = 1u; }
    static GuestGlContext *at(GlContextTable &table, std::uint32_t slot) { return *table.contexts_.at(slot); }
    static void plant(GlContextTable &table, std::uint64_t handle, GuestGlContext *context)
    {
        std::uint32_t slot = 0u;
        static_cast<void>(table.contexts_.insert(handle, context, slot));
    }
    static void pull(GlContextTable &table, std::uint64_t handle) { static_cast<void>(table.contexts_.erase(handle)); }
    static void plant_retired(GlContextTable &table, std::uint64_t handle, GuestGlContext *context)
    {
        std::uint32_t slot = 0u;
        static_cast<void>(table.retired_.insert(handle, context, slot));
        __atomic_store_n(&table.retired_count_, table.retired_.count(), __ATOMIC_RELEASE);
    }
    static void pull_retired(GlContextTable &table, std::uint64_t handle)
    {
        static_cast<void>(table.retired_.erase(handle));
    }
    static GuestGlContext *retired_at(GlContextTable &table, std::uint32_t slot) { return *table.retired_.at(slot); }
    static SliceGroups &groups(GlContextTable &table) { return table.groups_; }
    static void corrupt_contexts(GlContextTable &table, std::uint32_t count)
    {
        KeyedSlotsProbe::corrupt_count(table.contexts_, count);
    }
    static void corrupt_retired(GlContextTable &table, std::uint32_t count)
    {
        KeyedSlotsProbe::corrupt_count(table.retired_, count);
    }
    static void corrupt_contexts_free(GlContextTable &table, std::uint32_t count)
    {
        KeyedSlotsProbe::corrupt_free_count(table.contexts_, count);
    }
    static void corrupt_retired_free(GlContextTable &table, std::uint32_t count)
    {
        KeyedSlotsProbe::corrupt_free_count(table.retired_, count);
    }
    static void zero_group_counter(GlContextTable &table) { SliceGroupsProbe::set_next(table.groups_, 0u); }
};

}

namespace {

struct Table {
    GlContextRig        rig;
    RecordingThreadLock lock{rig.trace(), 'T'};
    RecordingThreadLock slice_lock{rig.trace(), 'L'};
    GlContextTable      table{rig.platform(), rig.connection(), lock, rig.send_lock(), slice_lock, rig.lines(),
                              rig.extensions(), rig.handles()};
};

}

TEST(gl_context_table, a_handle_names_its_context_until_it_is_destroyed)
{ Table fixture;
  GlContextTable &table = fixture.table;
  CHECK(table.invariants_hold());
  std::uint64_t first = 0u;
  std::uint64_t second = 0u;
  CHECK_STATUS(table.create(2, GlContextOptions(), first), Status::ok);
  CHECK_STATUS(table.create(3, GlContextOptions(), second), Status::ok);
  CHECK(table.invariants_hold());
  CHECK_EQ(first, 1u);
  CHECK_EQ(second, 2u);
  CHECK_EQ(table.live(), 2u);
  GuestGlContext *const context = table.resolve(second);
  CHECK(context != nullptr);
  CHECK_EQ(context->pixel_format(), 3);
  CHECK_STATUS(table.destroy(first), Status::ok);
  CHECK(table.invariants_hold());
  CHECK(table.resolve(first) == nullptr);
  CHECK_STATUS(table.destroy(first), Status::invalid_parameter);
  std::uint64_t third = 0u;
  CHECK_STATUS(table.create(4, GlContextOptions(), third), Status::ok);
  CHECK_EQ(third, 3u);
  CHECK(table.resolve(first) == nullptr);
  CHECK_EQ(table.resolve(third)->pixel_format(), 4);
  CHECK_EQ(table.live(), 2u); }

TEST(gl_context_table, forged_handles_resolve_to_nothing)
{ Table fixture;
  std::uint64_t handle = 0u;
  CHECK_STATUS(fixture.table.create(1, GlContextOptions(), handle), Status::ok);
  const std::uint64_t forged[] = {0u, 2u, 0x10000u, 0x10001u, 0x100000001ull, 0xFFFFFFFFu,
                                  0xFFFFFFFFFFFFFFFFull};
  for (const std::uint64_t each : forged) {
      CHECK(fixture.table.resolve(each) == nullptr);
      CHECK_STATUS(fixture.table.destroy(each), Status::invalid_parameter);
  }
  CHECK(fixture.table.resolve(handle) != nullptr);
  CHECK_EQ(fixture.table.live(), 1u); }

TEST(gl_context_table, holds_contexts_past_the_old_bound_and_refuses_one_with_no_memory)
{ Table fixture;
  constexpr std::uint32_t many = 600u;
  static std::uint64_t handles[many];
  for (std::uint32_t each = 0u; each < many; each++) {
      CHECK_STATUS(fixture.table.create(static_cast<std::int32_t>(each % 7u) + 1, GlContextOptions(), handles[each]),
                   Status::ok);
      CHECK_EQ(handles[each], each + 1u);
  }
  CHECK_EQ(fixture.table.live(), many);
  for (std::uint32_t each = 0u; each < many; each++)
      CHECK_EQ(fixture.table.resolve(handles[each])->pixel_format(), static_cast<std::int32_t>(each % 7u) + 1);
  CHECK(fixture.table.invariants_hold());
  for (std::uint32_t each = 0u; each < many; each += 2u) CHECK_STATUS(fixture.table.destroy(handles[each]), Status::ok);
  CHECK_EQ(fixture.table.live(), many / 2u);
  CHECK(fixture.table.resolve(handles[0]) == nullptr);
  CHECK(fixture.table.resolve(handles[1]) != nullptr);
  CHECK(fixture.table.invariants_hold());
  Table starved;
  std::uint64_t handle = 7u;
  starved.rig.platform().fail_allocation_at(1u);
  CHECK_STATUS(starved.table.create(1, GlContextOptions(), handle), Status::no_memory);
  CHECK_EQ(handle, 0u);
  CHECK_EQ(starved.table.live(), 0u);
  handle = 7u;
  starved.rig.platform().fail_allocation_at(3u);
  CHECK_STATUS(starved.table.create(1, GlContextOptions(), handle), Status::no_memory);
  CHECK_EQ(handle, 0u);
  CHECK_EQ(starved.table.live(), 0u);
  CHECK(starved.table.invariants_hold()); }

TEST(gl_context_table, a_handle_wraps_past_the_last_and_skips_one_in_use)
{ Table fixture;
  std::uint64_t first = 0u, last = 0u, wrapped = 0u;
  CHECK_STATUS(fixture.table.create(1, GlContextOptions(), first), Status::ok);
  CHECK_EQ(first, 1u);
  av::glicd::GlContextTableProbe::last_handle(fixture.table);
  CHECK_STATUS(fixture.table.create(1, GlContextOptions(), last), Status::ok);
  CHECK_EQ(last, 0xFFFFFFFFu);
  CHECK_STATUS(fixture.table.create(1, GlContextOptions(), wrapped), Status::ok);
  CHECK_EQ(wrapped, 2u);
  av::glicd::GlContextTableProbe::last_handle(fixture.table);
  std::uint64_t again = 0u;
  CHECK_STATUS(fixture.table.create(1, GlContextOptions(), again), Status::ok);
  CHECK_EQ(again, 3u);
  CHECK(fixture.table.invariants_hold()); }

TEST(gl_context_table, reports_each_live_context_under_its_handle)
{ Table fixture;
  std::uint64_t handle = 0u;
  CHECK_STATUS(fixture.table.create(1, GlContextOptions(), handle), Status::ok);
  char bytes[1024];
  av::StateWriter writer(bytes, sizeof bytes);
  fixture.table.report(writer);
  CHECK(std::strcmp(writer.dump(), "contexts/0 live=1 retired=0\n"
                                   "contexts/0/context/1 state=created owner=0 pixel_format=1 major=0 minor=0 "
                                   "profile=0 flags=0 share=0 share_target=0 error=0 device=0 lost=false "
                                   "fault_code=0 arena=false renderer= extensions=0\n"
                                   "contexts/0/context/1/windows/0 held=0 draws_window=false reads_window=false\n"
                                   "contexts/0/context/1/mappings/0 held=0 persistent_writers=0\n"
                                   "contexts/0/context/1/sliced_buffers/0 granted=false stopped=false group=1 "
                                   "owned=0 held=0\n"
                                   "contexts/0/context/1/shadows/0 trusted=true held=0 held_bytes=0 sizes=0") == 0); }

TEST(gl_context_table_invariants, fail_for_no_next_handle_or_a_broken_context)
{ Table zero;
  av::glicd::GlContextTableProbe::zero_next(zero.table);
  CHECK(!zero.table.invariants_hold());
  av::glicd::GlContextTableProbe::restore_next(zero.table);
  CHECK(zero.table.invariants_hold());
  Table broken;
  std::uint64_t handle = 0u;
  CHECK_STATUS(broken.table.create(1, GlContextOptions(), handle), Status::ok);
  GuestGlContext *const context = av::glicd::GlContextTableProbe::at(broken.table, 0u);
  av::glicd::GuestGlContextProbe::claim_made(*context);
  CHECK(!broken.table.invariants_hold());
  av::glicd::GuestGlContextProbe::unclaim(*context);
  CHECK(broken.table.invariants_hold());
  av::glicd::GlContextTableProbe::plant(broken.table, 99u, nullptr);
  CHECK(!broken.table.invariants_hold());
  av::glicd::GlContextTableProbe::pull(broken.table, 99u);
  av::glicd::GlContextTableProbe::plant(broken.table, 99u, context);
  CHECK(!broken.table.invariants_hold());
  av::glicd::GlContextTableProbe::pull(broken.table, 99u);
  CHECK(broken.table.invariants_hold()); }

namespace {

constexpr std::uint32_t sliced_storage = av::glicd::BufferMapping::map_persistent | av::glicd::BufferMapping::map_write;

struct Sharing {
    Table           fixture;
    std::uint64_t   owner = 0u;
    std::uint64_t   sharer = 0u;
    std::uint32_t   buffer = 5u;

    Sharing()
    {
        CHECK_STATUS(fixture.table.create(1, GlContextOptions(), owner), Status::ok);
        CHECK_STATUS(fixture.table.create(1, GlContextOptions(), sharer), Status::ok);
        CHECK_STATUS(fixture.table.join(sharer, owner), Status::ok);
        fixture.rig.host().gl_make("renderer", "4.6");
        fixture.rig.host().gl_slices(1u);
        CHECK_STATUS(fixture.table.resolve(owner)->make(), Status::ok);
        fixture.rig.host().blob_import(0u, 0x0000700000000100ull);
        fixture.rig.host().gl_slices(1u);
        GuestGlContext *const context = fixture.table.resolve(owner);
        CHECK_STATUS(context->slices().place(*context->wire(), buffer, 4096u, sliced_storage, nullptr), Status::ok);
    }

    void sharer_forgets_the_buffer()
    {
        fixture.table.resolve(sharer)->slices().forget(1, &buffer);
        CHECK_EQ(fixture.table.resolve(owner)->slices_owned(), 0u);
    }
};

}

TEST(gl_context_table, a_destroyed_context_still_owning_slices_a_sharer_may_read_is_retired_and_not_destroyed)
{ Sharing sharing;
  GlContextTable &table = sharing.fixture.table;
  CHECK_EQ(table.live(), 2u);
  CHECK_STATUS(table.destroy(sharing.owner), Status::ok);
  CHECK_EQ(table.live(), 1u);
  CHECK_EQ(table.retired(), 1u);
  CHECK(table.resolve(sharing.owner) == nullptr);
  CHECK(table.invariants_hold()); }

TEST(gl_context_table, reaping_destroys_a_retired_context_only_once_it_owes_its_sharers_nothing)
{ Sharing sharing;
  GlContextTable &table = sharing.fixture.table;
  CHECK_STATUS(table.destroy(sharing.owner), Status::ok);
  table.reap();
  CHECK_EQ(table.retired(), 1u);
  GuestGlContext *const sharer = table.resolve(sharing.sharer);
  sharer->slices().forget(1, &sharing.buffer);
  CHECK_EQ(table.retired(), 1u);
  table.reap();
  CHECK_EQ(table.retired(), 0u);
  CHECK_EQ(table.live(), 1u);
  table.reap();
  CHECK(table.invariants_hold()); }

TEST(gl_context_table, reaping_with_the_lock_already_held_does_the_same_without_taking_it)
{ Sharing sharing;
  GlContextTable &table = sharing.fixture.table;
  CHECK_STATUS(table.destroy(sharing.owner), Status::ok);
  sharing.fixture.table.resolve(sharing.sharer)->slices().forget(1, &sharing.buffer);
  sharing.fixture.lock.acquire();
  table.reap_held();
  sharing.fixture.lock.release();
  CHECK_EQ(table.retired(), 0u);
  CHECK(table.invariants_hold()); }

TEST(gl_context_table, destroying_the_last_sharer_reaps_the_retired_context_it_was_waiting_on)
{ Sharing sharing;
  GlContextTable &table = sharing.fixture.table;
  CHECK_STATUS(table.destroy(sharing.owner), Status::ok);
  CHECK_EQ(table.retired(), 1u);
  CHECK_STATUS(table.destroy(sharing.sharer), Status::ok);
  CHECK_EQ(table.retired(), 0u);
  CHECK_EQ(table.live(), 0u);
  CHECK(table.invariants_hold()); }

namespace {

struct Pair {
    std::uint64_t owner = 0u;
    std::uint64_t sharer = 0u;
    std::uint32_t buffer = 0u;

    Pair(Table &fixture, std::uint32_t buffer_name, std::uint64_t blob)
        : buffer(buffer_name)
    {
        CHECK_STATUS(fixture.table.create(1, GlContextOptions(), owner), Status::ok);
        CHECK_STATUS(fixture.table.create(1, GlContextOptions(), sharer), Status::ok);
        CHECK_STATUS(fixture.table.join(sharer, owner), Status::ok);
        fixture.rig.host().gl_make("renderer", "4.6");
        fixture.rig.host().gl_slices(1u);
        CHECK_STATUS(fixture.table.resolve(owner)->make(), Status::ok);
        fixture.rig.host().blob_import(0u, blob);
        fixture.rig.host().gl_slices(1u);
        GuestGlContext *const context = fixture.table.resolve(owner);
        CHECK_STATUS(context->slices().place(*context->wire(), buffer, 4096u, sliced_storage, nullptr), Status::ok);
    }

    void sharer_forgets(Table &fixture)
    {
        fixture.table.resolve(sharer)->slices().forget(1, &buffer);
    }
};

struct PlacingLock final : av::ThreadLock {
    void acquire() override
    {
        acquisitions++;
        if (acquisitions == placing_at && place != nullptr) place(context);
    }
    bool try_acquire() override { return true; }
    void release() override {}

    std::uint32_t acquisitions = 0u;
    std::uint32_t placing_at = 0u;
    void        (*place)(void *) = nullptr;
    void         *context = nullptr;
};

}

TEST(gl_context_table, a_new_handle_skips_the_one_a_retired_context_still_holds)
{ Sharing sharing;
  GlContextTable &table = sharing.fixture.table;
  CHECK_STATUS(table.destroy(sharing.owner), Status::ok);
  CHECK_EQ(table.retired(), 1u);
  av::glicd::GlContextTableProbe::restore_next(table);
  std::uint64_t made = 0u;
  CHECK_STATUS(table.create(1, GlContextOptions(), made), Status::ok);
  CHECK_EQ(made, 3u);
  CHECK(table.invariants_hold()); }

TEST(gl_context_table, a_join_with_a_handle_that_names_nothing_is_invalid)
{ Table fixture;
  std::uint64_t first = 0u, second = 0u;
  CHECK_STATUS(fixture.table.create(1, GlContextOptions(), first), Status::ok);
  CHECK_STATUS(fixture.table.create(1, GlContextOptions(), second), Status::ok);
  CHECK_STATUS(fixture.table.join(99u, first), Status::invalid_parameter);
  CHECK_STATUS(fixture.table.join(first, 99u), Status::invalid_parameter);
  CHECK_STATUS(fixture.table.join(first, second), Status::ok);
  CHECK(fixture.table.invariants_hold()); }

TEST(gl_context_table, a_context_that_already_placed_slices_cannot_join_another)
{ Table fixture;
  std::uint64_t first = 0u, second = 0u;
  CHECK_STATUS(fixture.table.create(1, GlContextOptions(), first), Status::ok);
  CHECK_STATUS(fixture.table.create(1, GlContextOptions(), second), Status::ok);
  av::glicd::SliceGroups::Row row = {};
  row.owner_ = &fixture.table.resolve(first)->slices();
  row.size_ = 4096u;
  row.slice_.bytes_ = 4096u;
  CHECK_STATUS(av::glicd::GlContextTableProbe::groups(fixture.table).insert(fixture.table.resolve(first)->slice_group(), 5u, row),
               Status::ok);
  CHECK_STATUS(fixture.table.join(first, second), Status::invalid_state);
  CHECK(!av::glicd::GlContextTableProbe::groups(fixture.table).same(fixture.table.resolve(first)->slice_group(),
                                                                    fixture.table.resolve(second)->slice_group()));
  av::glicd::GlContextTableProbe::groups(fixture.table).drop_owner(&fixture.table.resolve(first)->slices()); }

TEST(gl_context_table, a_join_that_loses_the_race_to_a_placement_in_its_group_is_refused)
{ GlContextRig rig;
  RecordingThreadLock lock{rig.trace(), 'T'};
  PlacingLock         slice_lock;
  GlContextTable      table{rig.platform(), rig.connection(), lock, rig.send_lock(), slice_lock, rig.lines(),
                            rig.extensions(), rig.handles()};
  std::uint64_t first = 0u, second = 0u;
  CHECK_STATUS(table.create(1, GlContextOptions(), first), Status::ok);
  CHECK_STATUS(table.create(1, GlContextOptions(), second), Status::ok);
  struct Race {
      GlContextTable *table;
      std::uint64_t   joiner;
  } race = {&table, first};
  slice_lock.context = &race;
  slice_lock.place = [](void *raw) {
      Race &race = *static_cast<Race *>(raw);
      GuestGlContext *const joiner = race.table->resolve(race.joiner);
      av::glicd::SliceGroups::Row row = {};
      row.owner_ = &joiner->slices();
      row.size_ = 4096u;
      row.slice_.bytes_ = 4096u;
      CHECK_STATUS(av::glicd::GlContextTableProbe::groups(*race.table).insert(joiner->slice_group(), 5u, row),
                   Status::ok);
  };
  slice_lock.placing_at = slice_lock.acquisitions + 2u;
  CHECK_STATUS(table.join(first, second), Status::invalid_state);
  slice_lock.place = nullptr;
  av::glicd::GlContextTableProbe::groups(table).drop_owner(&table.resolve(first)->slices()); }

TEST(gl_context_table, a_destroyed_owner_retires_only_while_a_context_outside_its_group_is_not_enough)
{ Table fixture;
  std::uint64_t unrelated = 0u;
  CHECK_STATUS(fixture.table.create(1, GlContextOptions(), unrelated), Status::ok);
  Pair pair(fixture, 5u, 0x0000700000000100ull);
  std::uint64_t second_sharer = 0u;
  CHECK_STATUS(fixture.table.create(1, GlContextOptions(), second_sharer), Status::ok);
  CHECK_STATUS(fixture.table.join(second_sharer, pair.owner), Status::ok);
  CHECK_STATUS(fixture.table.destroy(pair.owner), Status::ok);
  CHECK_EQ(fixture.table.retired(), 1u);
  CHECK_EQ(fixture.table.live(), 3u);
  CHECK(fixture.table.invariants_hold()); }

TEST(gl_context_table, an_owner_nobody_shares_with_any_more_is_destroyed_at_once)
{ Sharing sharing;
  GlContextTable &table = sharing.fixture.table;
  CHECK_STATUS(table.destroy(sharing.sharer), Status::ok);
  CHECK_STATUS(table.destroy(sharing.owner), Status::ok);
  CHECK_EQ(table.retired(), 0u);
  CHECK_EQ(table.live(), 0u);
  CHECK(table.invariants_hold()); }

TEST(gl_context_table, a_retired_context_whose_slices_are_pinned_waits_for_the_pin)
{ Sharing sharing;
  GlContextTable &table = sharing.fixture.table;
  CHECK_STATUS(table.destroy(sharing.owner), Status::ok);
  table.resolve(sharing.sharer)->slices().forget(1, &sharing.buffer);
  GuestGlContext *const retired = av::glicd::GlContextTableProbe::retired_at(table, 0u);
  retired->slices().pin();
  table.reap();
  CHECK_EQ(table.retired(), 1u);
  retired->slices().unpin();
  table.reap();
  CHECK_EQ(table.retired(), 0u);
  CHECK(table.invariants_hold()); }

TEST(gl_context_table, an_owner_that_cannot_be_kept_for_its_sharers_is_destroyed_with_no_memory)
{ Sharing sharing;
  GlContextTable &table = sharing.fixture.table;
  sharing.fixture.rig.platform().fail_allocation_at(1u);
  CHECK_STATUS(table.destroy(sharing.owner), Status::ok);
  CHECK_EQ(table.retired(), 0u);
  CHECK_EQ(table.live(), 1u);
  CHECK(table.invariants_hold()); }

TEST(gl_context_table, every_retired_context_that_owes_nothing_is_reaped_one_after_the_other)
{ Table fixture;
  Pair first(fixture, 5u, 0x0000700000000100ull);
  Pair second(fixture, 6u, 0x0000700000000200ull);
  GlContextTable &table = fixture.table;
  CHECK_STATUS(table.destroy(first.owner), Status::ok);
  CHECK_STATUS(table.destroy(second.owner), Status::ok);
  CHECK_EQ(table.retired(), 2u);
  first.sharer_forgets(fixture);
  second.sharer_forgets(fixture);
  table.reap();
  CHECK_EQ(table.retired(), 0u);
  CHECK_EQ(table.live(), 2u);
  CHECK(table.invariants_hold()); }

TEST(gl_context_table, a_retired_context_the_pool_does_not_know_is_dropped_and_logged)
{ Table fixture;
  Table other;
  std::uint64_t foreign = 0u;
  CHECK_STATUS(other.table.create(1, GlContextOptions(), foreign), Status::ok);
  av::glicd::GlContextTableProbe::plant_retired(fixture.table, 77u, other.table.resolve(foreign));
  fixture.table.reap();
  CHECK_EQ(fixture.table.retired(), 0u);
  CHECK_EQ(fixture.rig.log().holding("a retired OpenGL context could not be destroyed"), 1u);
  CHECK(other.table.invariants_hold()); }

TEST(gl_context_table_invariants, fail_for_contexts_broken_wherever_they_come_in_the_walk)
{ Table both;
  av::glicd::GlContextTableProbe::plant(both.table, 98u, nullptr);
  av::glicd::GlContextTableProbe::plant(both.table, 99u, nullptr);
  CHECK(!both.table.invariants_hold());
  av::glicd::GlContextTableProbe::pull(both.table, 98u);
  av::glicd::GlContextTableProbe::pull(both.table, 99u);
  Table kept;
  std::uint64_t handle = 0u;
  CHECK_STATUS(kept.table.create(1, GlContextOptions(), handle), Status::ok);
  av::glicd::GlContextTableProbe::plant_retired(kept.table, 50u, nullptr);
  CHECK(!kept.table.invariants_hold());
  av::glicd::GlContextTableProbe::pull_retired(kept.table, 50u);
  Table other;
  std::uint64_t foreign = 0u;
  CHECK_STATUS(other.table.create(1, GlContextOptions(), foreign), Status::ok);
  GuestGlContext *const stranger = other.table.resolve(foreign);
  av::glicd::GuestGlContextProbe::claim_made(*stranger);
  av::glicd::GlContextTableProbe::plant_retired(kept.table, 51u, stranger);
  CHECK(!kept.table.invariants_hold());
  av::glicd::GlContextTableProbe::pull_retired(kept.table, 51u);
  av::glicd::GuestGlContextProbe::unclaim(*stranger);
  av::glicd::GlContextTableProbe::plant_retired(kept.table, 52u, nullptr);
  av::glicd::GlContextTableProbe::plant_retired(kept.table, 53u, nullptr);
  CHECK(!kept.table.invariants_hold());
  av::glicd::GlContextTableProbe::pull_retired(kept.table, 52u);
  av::glicd::GlContextTableProbe::pull_retired(kept.table, 53u);
  CHECK(kept.table.invariants_hold()); }

TEST(gl_context_table_invariants, fail_for_tables_that_lost_their_shape)
{ Table contexts;
  av::glicd::GlContextTableProbe::corrupt_contexts(contexts.table, 99u);
  CHECK(!contexts.table.invariants_hold());
  av::glicd::GlContextTableProbe::corrupt_contexts(contexts.table, 0u);
  av::glicd::GlContextTableProbe::corrupt_contexts_free(contexts.table, 1u);
  CHECK(!contexts.table.invariants_hold());
  av::glicd::GlContextTableProbe::corrupt_contexts_free(contexts.table, 0u);
  Table freed;
  av::glicd::GlContextTableProbe::corrupt_retired_free(freed.table, 1u);
  CHECK(!freed.table.invariants_hold());
  av::glicd::GlContextTableProbe::corrupt_retired_free(freed.table, 0u);
  Table retired;
  av::glicd::GlContextTableProbe::corrupt_retired(retired.table, 99u);
  CHECK(!retired.table.invariants_hold());
  av::glicd::GlContextTableProbe::corrupt_retired(retired.table, 0u);
  Table groups;
  av::glicd::GlContextTableProbe::zero_group_counter(groups.table);
  CHECK(!groups.table.invariants_hold()); }
