#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opengl/slice_groups.hpp"

#include <cstdint>

using av::LockTrace;
using av::MockPlatform;
using av::RecordingThreadLock;
using av::Status;
using av::glicd::SliceGroups;
using av::glicd::SlicedBuffers;

namespace {

char tokens_of_owners[8];

SlicedBuffers *owner(std::uint32_t index) { return reinterpret_cast<SlicedBuffers *>(&tokens_of_owners[index]); }

SliceGroups::Row row_of(std::uint32_t owner_index, std::uint64_t size = 4096u)
{
    SliceGroups::Row row = {};
    row.owner_ = owner(owner_index);
    row.slice_ = av::icd::StreamMemory::Slice{65536u, 65536u};
    row.size_ = size;
    row.flags_ = 7u;
    row.access_ = 1u;
    row.stream_ = 2u;
    return row;
}

class Rig {
public:
    Rig() : lock_(trace_, 'G'), groups_(platform_, lock_) {}

    MockPlatform        platform_;
    LockTrace           trace_;
    RecordingThreadLock lock_;
    SliceGroups         groups_;
};

}

TEST(slice_groups, a_fresh_set_holds_nothing)
{ Rig rig;
  CHECK_EQ(rig.groups_.count(), 0u);
  CHECK(rig.groups_.find(1u, 1u) == nullptr);
  CHECK(rig.groups_.member_of(1u) == nullptr);
  CHECK(rig.groups_.invariants_hold()); }

TEST(slice_groups_mint, hands_out_ascending_tokens_starting_at_one_each_in_a_group_of_its_own)
{ Rig rig;
  const std::uint32_t a = rig.groups_.mint();
  const std::uint32_t b = rig.groups_.mint();
  const std::uint32_t c = rig.groups_.mint();
  CHECK_EQ(a, 1u);
  CHECK_EQ(b, 2u);
  CHECK_EQ(c, 3u);
  CHECK(rig.groups_.same(a, a));
  CHECK(!rig.groups_.same(a, b));
  CHECK(!rig.groups_.same(b, c));
  CHECK(rig.groups_.invariants_hold()); }

TEST(slice_groups_join, puts_two_groups_under_one_root_in_either_direction_of_asking)
{ Rig rig;
  const std::uint32_t a = rig.groups_.mint();
  const std::uint32_t b = rig.groups_.mint();
  CHECK_STATUS(rig.groups_.join(a, b), Status::ok);
  CHECK(rig.groups_.same(a, b));
  CHECK(rig.groups_.same(b, a));
  CHECK(rig.groups_.invariants_hold()); }

TEST(slice_groups_join, is_transitive_through_a_chain_of_joins)
{ Rig rig;
  const std::uint32_t a = rig.groups_.mint();
  const std::uint32_t b = rig.groups_.mint();
  const std::uint32_t c = rig.groups_.mint();
  const std::uint32_t d = rig.groups_.mint();
  CHECK_STATUS(rig.groups_.join(a, b), Status::ok);
  CHECK_STATUS(rig.groups_.join(b, c), Status::ok);
  CHECK(rig.groups_.same(a, c));
  CHECK(!rig.groups_.same(a, d));
  CHECK_STATUS(rig.groups_.join(d, a), Status::ok);
  CHECK(rig.groups_.same(d, c));
  CHECK(rig.groups_.invariants_hold()); }

TEST(slice_groups_join, joining_a_group_to_itself_or_to_its_own_root_is_a_no_op)
{ Rig rig;
  const std::uint32_t a = rig.groups_.mint();
  const std::uint32_t b = rig.groups_.mint();
  CHECK_STATUS(rig.groups_.join(a, a), Status::ok);
  CHECK_STATUS(rig.groups_.join(a, b), Status::ok);
  CHECK_STATUS(rig.groups_.join(b, a), Status::ok);
  CHECK_STATUS(rig.groups_.join(a, b), Status::ok);
  CHECK(rig.groups_.same(a, b));
  CHECK(rig.groups_.invariants_hold()); }

TEST(slice_groups_join, a_group_that_already_placed_a_buffer_cannot_join_another)
{ Rig rig;
  const std::uint32_t a = rig.groups_.mint();
  const std::uint32_t b = rig.groups_.mint();
  CHECK_STATUS(rig.groups_.insert(a, 5u, row_of(0u)), Status::ok);
  CHECK_STATUS(rig.groups_.join(a, b), Status::invalid_state);
  CHECK(!rig.groups_.same(a, b));
  CHECK(rig.groups_.find(a, 5u) != nullptr);
  CHECK(rig.groups_.invariants_hold()); }

TEST(slice_groups_join, a_group_with_buffers_can_be_the_target_and_keeps_them)
{ Rig rig;
  const std::uint32_t a = rig.groups_.mint();
  const std::uint32_t b = rig.groups_.mint();
  CHECK_STATUS(rig.groups_.insert(b, 5u, row_of(0u)), Status::ok);
  CHECK_STATUS(rig.groups_.join(a, b), Status::ok);
  CHECK(rig.groups_.find(a, 5u) != nullptr);
  CHECK(rig.groups_.find(a, 5u) == rig.groups_.find(b, 5u));
  CHECK(rig.groups_.invariants_hold()); }

TEST(slice_groups_join, a_group_that_joined_is_judged_by_its_whole_group_for_placed_buffers)
{ Rig rig;
  const std::uint32_t a = rig.groups_.mint();
  const std::uint32_t b = rig.groups_.mint();
  const std::uint32_t c = rig.groups_.mint();
  CHECK_STATUS(rig.groups_.join(a, b), Status::ok);
  CHECK_STATUS(rig.groups_.insert(a, 9u, row_of(0u)), Status::ok);
  CHECK_STATUS(rig.groups_.join(b, c), Status::invalid_state);
  CHECK(!rig.groups_.same(b, c));
  CHECK_STATUS(rig.groups_.join(c, b), Status::ok);
  CHECK(rig.groups_.same(c, a)); }

TEST(slice_groups_join, a_failed_allocation_leaves_the_groups_apart)
{ Rig rig;
  const std::uint32_t a = rig.groups_.mint();
  const std::uint32_t b = rig.groups_.mint();
  rig.platform_.fail_allocation_at(1u);
  CHECK_STATUS(rig.groups_.join(a, b), Status::no_memory);
  CHECK(!rig.groups_.same(a, b));
  CHECK(rig.groups_.invariants_hold());
  CHECK_STATUS(rig.groups_.join(a, b), Status::ok);
  CHECK(rig.groups_.same(a, b)); }

TEST(slice_groups_admit_join, refuses_a_group_with_buffers_and_otherwise_reserves_room)
{ Rig rig;
  const std::uint32_t a = rig.groups_.mint();
  const std::uint32_t b = rig.groups_.mint();
  CHECK_STATUS(rig.groups_.admit_join(a), Status::ok);
  CHECK_STATUS(rig.groups_.insert(a, 1u, row_of(0u)), Status::ok);
  CHECK_STATUS(rig.groups_.admit_join(a), Status::invalid_state);
  CHECK_STATUS(rig.groups_.admit_join(b), Status::ok);
  CHECK(rig.groups_.invariants_hold()); }

TEST(slice_groups_admit_join, a_failed_allocation_is_no_memory)
{ Rig rig;
  const std::uint32_t a = rig.groups_.mint();
  rig.platform_.fail_allocation_at(1u);
  CHECK_STATUS(rig.groups_.admit_join(a), Status::no_memory);
  CHECK(rig.groups_.invariants_hold()); }

TEST(slice_groups_admit_join, a_reserved_join_cannot_then_run_out_of_memory)
{ Rig rig;
  const std::uint32_t a = rig.groups_.mint();
  const std::uint32_t b = rig.groups_.mint();
  CHECK_STATUS(rig.groups_.admit_join(a), Status::ok);
  rig.platform_.fail_allocation_at(1u);
  CHECK_STATUS(rig.groups_.join(a, b), Status::ok);
  CHECK(rig.groups_.same(a, b)); }

TEST(slice_groups_insert, files_a_buffer_under_the_root_of_its_group)
{ Rig rig;
  const std::uint32_t a = rig.groups_.mint();
  CHECK_STATUS(rig.groups_.insert(a, 5u, row_of(1u, 2048u)), Status::ok);
  CHECK_EQ(rig.groups_.count(), 1u);
  const SliceGroups::Row *const found = rig.groups_.find(a, 5u);
  CHECK(found != nullptr);
  CHECK(found->owner_ == owner(1u));
  CHECK_EQ(found->size_, 2048u);
  CHECK_EQ(found->slice_.offset_, 65536u);
  CHECK(rig.groups_.find(a, 6u) == nullptr);
  CHECK(rig.groups_.find(a + 1u, 5u) == nullptr);
  CHECK(rig.groups_.invariants_hold()); }

TEST(slice_groups_insert, buffer_name_zero_and_a_taken_name_are_conflicts)
{ Rig rig;
  const std::uint32_t a = rig.groups_.mint();
  CHECK_STATUS(rig.groups_.insert(a, 0u, row_of(0u)), Status::conflict);
  CHECK_STATUS(rig.groups_.insert(a, 5u, row_of(0u)), Status::ok);
  CHECK_STATUS(rig.groups_.insert(a, 5u, row_of(1u)), Status::conflict);
  CHECK(rig.groups_.find(a, 5u)->owner_ == owner(0u));
  CHECK_EQ(rig.groups_.count(), 1u); }

TEST(slice_groups_insert, the_same_name_in_two_groups_is_two_buffers)
{ Rig rig;
  const std::uint32_t a = rig.groups_.mint();
  const std::uint32_t b = rig.groups_.mint();
  CHECK_STATUS(rig.groups_.insert(a, 5u, row_of(0u)), Status::ok);
  CHECK_STATUS(rig.groups_.insert(b, 5u, row_of(1u)), Status::ok);
  CHECK(rig.groups_.find(a, 5u)->owner_ == owner(0u));
  CHECK(rig.groups_.find(b, 5u)->owner_ == owner(1u));
  CHECK_EQ(rig.groups_.count(), 2u); }

TEST(slice_groups_insert, the_name_space_is_the_whole_thirty_two_bits)
{ Rig rig;
  const std::uint32_t a = rig.groups_.mint();
  const std::uint32_t names[] = {1u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu};
  for (const std::uint32_t name : names) CHECK_STATUS(rig.groups_.insert(a, name, row_of(0u)), Status::ok);
  for (const std::uint32_t name : names) CHECK(rig.groups_.find(a, name) != nullptr);
  CHECK(rig.groups_.invariants_hold()); }

TEST(slice_groups_insert, a_failed_allocation_adds_no_row)
{ Rig rig;
  const std::uint32_t a = rig.groups_.mint();
  rig.platform_.fail_allocation_at(1u);
  CHECK_STATUS(rig.groups_.insert(a, 5u, row_of(0u)), Status::no_memory);
  CHECK_EQ(rig.groups_.count(), 0u);
  CHECK(rig.groups_.invariants_hold()); }

TEST(slice_groups_find, is_the_row_itself_so_a_change_through_it_sticks)
{ Rig rig;
  const std::uint32_t a = rig.groups_.mint();
  CHECK_STATUS(rig.groups_.insert(a, 5u, row_of(0u)), Status::ok);
  rig.groups_.find(a, 5u)->flags_ = 99u;
  CHECK_EQ(rig.groups_.find(a, 5u)->flags_, 99u);
  CHECK(rig.groups_.find(a, 0u) == nullptr); }

TEST(slice_groups_take, removes_the_row_and_hands_it_back_once)
{ Rig rig;
  const std::uint32_t a = rig.groups_.mint();
  CHECK_STATUS(rig.groups_.insert(a, 5u, row_of(0u, 1024u)), Status::ok);
  SliceGroups::Row row = {};
  CHECK(!rig.groups_.take(a, 0u, row));
  CHECK(rig.groups_.take(a, 5u, row));
  CHECK_EQ(row.size_, 1024u);
  CHECK(!rig.groups_.take(a, 5u, row));
  CHECK_EQ(rig.groups_.count(), 0u);
  CHECK(rig.groups_.invariants_hold()); }

TEST(slice_groups_drop_owner, removes_every_row_of_that_owner_and_no_others)
{ Rig rig;
  const std::uint32_t a = rig.groups_.mint();
  for (std::uint32_t buffer = 1u; buffer <= 6u; buffer++)
      CHECK_STATUS(rig.groups_.insert(a, buffer, row_of(buffer % 2u)), Status::ok);
  CHECK_EQ(rig.groups_.owned_by(owner(0u)), 3u);
  CHECK_EQ(rig.groups_.owned_by(owner(1u)), 3u);
  rig.groups_.drop_owner(owner(0u));
  CHECK_EQ(rig.groups_.count(), 3u);
  CHECK_EQ(rig.groups_.owned_by(owner(0u)), 0u);
  CHECK(rig.groups_.find(a, 2u) == nullptr);
  CHECK(rig.groups_.find(a, 1u) != nullptr);
  rig.groups_.drop_owner(owner(5u));
  CHECK_EQ(rig.groups_.count(), 3u);
  CHECK(rig.groups_.invariants_hold()); }

TEST(slice_groups_enroll, mints_member_ids_in_order_and_remembers_the_member)
{ Rig rig;
  std::uint64_t first = 0u, second = 0u;
  CHECK_STATUS(rig.groups_.enroll(owner(0u), 1u, first), Status::ok);
  CHECK_STATUS(rig.groups_.enroll(owner(1u), 1u, second), Status::ok);
  CHECK_EQ(first, 1u);
  CHECK_EQ(second, 2u);
  CHECK(rig.groups_.member_of(first) == owner(0u));
  CHECK(rig.groups_.member_of(second) == owner(1u));
  CHECK(rig.groups_.member_of(3u) == nullptr);
  CHECK(rig.groups_.member_of(0u) == nullptr);
  CHECK(rig.groups_.invariants_hold()); }

TEST(slice_groups_enroll, an_already_enrolled_id_is_kept_and_nothing_is_minted)
{ Rig rig;
  std::uint64_t id = 0u;
  CHECK_STATUS(rig.groups_.enroll(owner(0u), 1u, id), Status::ok);
  const std::uint64_t kept = id;
  CHECK_STATUS(rig.groups_.enroll(owner(1u), 2u, id), Status::ok);
  CHECK_EQ(id, kept);
  CHECK(rig.groups_.member_of(id) == owner(0u));
  std::uint64_t next = 0u;
  CHECK_STATUS(rig.groups_.enroll(owner(2u), 1u, next), Status::ok);
  CHECK_EQ(next, kept + 1u); }

TEST(slice_groups_enroll, a_withdrawn_member_enrolls_again_under_a_new_id_and_ids_are_never_reused)
{ Rig rig;
  std::uint64_t id = 0u;
  CHECK_STATUS(rig.groups_.enroll(owner(0u), 1u, id), Status::ok);
  const std::uint64_t old = id;
  rig.groups_.withdraw(id);
  CHECK(rig.groups_.member_of(old) == nullptr);
  CHECK_STATUS(rig.groups_.enroll(owner(0u), 1u, id), Status::ok);
  CHECK(id != old);
  CHECK(rig.groups_.member_of(old) == nullptr);
  CHECK(rig.groups_.member_of(id) == owner(0u));
  rig.groups_.withdraw(old);
  rig.groups_.withdraw(0u);
  rig.groups_.withdraw(1000u);
  CHECK(rig.groups_.member_of(id) == owner(0u));
  CHECK(rig.groups_.invariants_hold()); }

TEST(slice_groups_enroll, a_failed_allocation_enrolls_nothing_and_mints_nothing)
{ Rig rig;
  std::uint64_t id = 0u;
  rig.platform_.fail_allocation_at(1u);
  CHECK_STATUS(rig.groups_.enroll(owner(0u), 1u, id), Status::no_memory);
  CHECK_EQ(id, 0u);
  CHECK_STATUS(rig.groups_.enroll(owner(0u), 1u, id), Status::ok);
  CHECK_EQ(id, 1u); }

TEST(slice_groups_next_member, walks_the_members_of_one_group_and_of_every_group_joined_to_it)
{ Rig rig;
  const std::uint32_t a = rig.groups_.mint();
  const std::uint32_t b = rig.groups_.mint();
  const std::uint32_t c = rig.groups_.mint();
  std::uint64_t       ida = 0u, idb = 0u, idc = 0u, idb2 = 0u;
  CHECK_STATUS(rig.groups_.enroll(owner(0u), a, ida), Status::ok);
  CHECK_STATUS(rig.groups_.enroll(owner(1u), b, idb), Status::ok);
  CHECK_STATUS(rig.groups_.enroll(owner(2u), c, idc), Status::ok);
  CHECK_STATUS(rig.groups_.enroll(owner(3u), b, idb2), Status::ok);
  CHECK_STATUS(rig.groups_.join(a, b), Status::ok);
  std::uint32_t cursor = 0u;
  std::uint32_t seen = 0u;
  std::uint64_t order[4] = {};
  for (const SliceGroups::Member *member = rig.groups_.next_member(a, cursor); member != nullptr;
       member = rig.groups_.next_member(a, cursor))
      order[seen++] = member->id_;
  CHECK_EQ(seen, 3u);
  CHECK_EQ(order[0], ida);
  CHECK_EQ(order[1], idb);
  CHECK_EQ(order[2], idb2);
  CHECK(cursor != 0u);
  cursor = 0u;
  const SliceGroups::Member *const only = rig.groups_.next_member(c, cursor);
  CHECK(only != nullptr && only->id_ == idc);
  CHECK(rig.groups_.next_member(c, cursor) == nullptr); }

TEST(slice_groups_next_member, a_group_with_no_members_has_none_and_a_withdrawn_one_is_skipped)
{ Rig rig;
  const std::uint32_t a = rig.groups_.mint();
  std::uint32_t       cursor = 0u;
  CHECK(rig.groups_.next_member(a, cursor) == nullptr);
  std::uint64_t first = 0u, second = 0u;
  CHECK_STATUS(rig.groups_.enroll(owner(0u), a, first), Status::ok);
  CHECK_STATUS(rig.groups_.enroll(owner(1u), a, second), Status::ok);
  rig.groups_.withdraw(first);
  cursor = 0u;
  const SliceGroups::Member *const member = rig.groups_.next_member(a, cursor);
  CHECK(member != nullptr && member->id_ == second);
  CHECK(rig.groups_.next_member(a, cursor) == nullptr); }

TEST(slice_groups_next_member, a_member_enrolled_while_walking_is_met_by_the_same_walk)
{ Rig rig;
  const std::uint32_t a = rig.groups_.mint();
  std::uint64_t       first = 0u, late = 0u;
  CHECK_STATUS(rig.groups_.enroll(owner(0u), a, first), Status::ok);
  std::uint32_t cursor = 0u;
  CHECK(rig.groups_.next_member(a, cursor) != nullptr);
  CHECK_STATUS(rig.groups_.enroll(owner(1u), a, late), Status::ok);
  const SliceGroups::Member *const member = rig.groups_.next_member(a, cursor);
  CHECK(member != nullptr && member->id_ == late); }

TEST(slice_groups_invariants, a_row_that_breaks_a_rule_is_noticed)
{ const SliceGroups::Row good = row_of(0u);
  SliceGroups::Row       cases[6] = {good, good, good, good, good, good};
  cases[0].owner_ = nullptr;
  cases[1].size_ = 0u;
  cases[2].size_ = good.slice_.bytes_ + 1u;
  cases[3].map_offset_ = good.size_;
  cases[3].map_bytes_ = 1u;
  cases[4].map_bytes_ = 8u;
  cases[4].access_ = 0u;
  cases[5].map_offset_ = ~std::uint64_t{0};
  cases[5].map_bytes_ = 8u;
  cases[5].access_ = 1u;
  for (std::uint32_t index = 0u; index < 6u; index++) {
      Rig rig;
      const std::uint32_t a = rig.groups_.mint();
      CHECK_STATUS(rig.groups_.insert(a, 1u, cases[index]), Status::ok);
      if (rig.groups_.invariants_hold())
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "case %u passed the invariants", index);
  }
  Rig rig;
  const std::uint32_t a = rig.groups_.mint();
  CHECK_STATUS(rig.groups_.insert(a, 1u, good), Status::ok);
  CHECK(rig.groups_.invariants_hold()); }
