#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/scripted_gpu_timeline.hpp"
#include "harness/icd/slice_channel_rig.hpp"
#include "icd/opengl/deferred_slices.hpp"
#include "icd/opengl/slice_groups.hpp"
#include "icd/opengl/sliced_buffers.hpp"

#include <cstdint>

using av::ScriptedGpuTimeline;
using av::SliceChannelRig;
using av::Status;
using av::glicd::DeferredSlices;
using av::glicd::SliceGroups;
using av::glicd::SlicedBuffers;

namespace av {

struct GrowableArrayProbe {
    template <typename T>
    static void corrupt_capacity(GrowableArray<T> &array, std::uint32_t capacity) { array.capacity_ = capacity; }
};

}

namespace av::glicd {

struct DeferredSlicesProbe {
    static void claim_marks(DeferredSlices &deferred, std::uint32_t marks) { deferred.entries_.at(0u).marks_ = marks; }
    static void corrupt_entries(DeferredSlices &deferred) { GrowableArrayProbe::corrupt_capacity(deferred.entries_, 3u); }
    static void corrupt_marks(DeferredSlices &deferred) { GrowableArrayProbe::corrupt_capacity(deferred.marks_, 3u); }
};

}

using av::glicd::DeferredSlicesProbe;

namespace {

class Member {
public:
    Member(SliceChannelRig &channel, SliceGroups &groups, std::uint32_t group)
        : buffers_(channel.platform(), channel.channel(), timeline_, groups, group, channel.lines(), this)
    {
        CHECK_STATUS(buffers_.enroll(), Status::ok);
    }
    ~Member() { buffers_.release(); }

    ScriptedGpuTimeline timeline_;
    SlicedBuffers       buffers_;
};

class Rig {
public:
    Rig() : groups_(channel_.platform(), channel_.stream_lock()), deferred_(channel_.platform()) {}

    DeferredSlices::Entry entry(std::uint64_t offset, std::uint32_t stream = 1u) const
    {
        DeferredSlices::Entry made = {};
        made.slice_ = av::icd::StreamMemory::Slice{offset, 256u};
        made.stream_ = stream;
        made.owner_fence_ = 40u + offset;
        return made;
    }

    SliceChannelRig channel_;
    SliceGroups     groups_;
    DeferredSlices  deferred_;
};

}

TEST(deferred_slices, starts_with_nothing_pending)
{ Rig rig;
  DeferredSlices::Entry ready = {};
  CHECK_EQ(rig.deferred_.pending(), 0u);
  CHECK(!rig.deferred_.take_ready(rig.groups_, ready));
  CHECK(rig.deferred_.invariants_hold()); }

TEST(deferred_slices_add, a_slice_with_no_other_member_is_ready_at_once_with_its_fields_intact)
{ Rig               rig;
  const std::uint32_t group = rig.groups_.mint();
  Member              owner(rig.channel_, rig.groups_, group);
  CHECK_STATUS(rig.deferred_.add(rig.groups_, group, &owner.buffers_, rig.entry(4096u, 3u)), Status::ok);
  CHECK_EQ(rig.deferred_.pending(), 1u);
  CHECK(rig.deferred_.invariants_hold());
  DeferredSlices::Entry ready = {};
  CHECK(rig.deferred_.take_ready(rig.groups_, ready));
  CHECK_EQ(ready.slice_.offset_, 4096u);
  CHECK_EQ(ready.slice_.bytes_, 256u);
  CHECK_EQ(ready.stream_, 3u);
  CHECK_EQ(ready.owner_fence_, 40u + 4096u);
  CHECK_EQ(ready.marks_, 0u);
  CHECK_EQ(rig.deferred_.pending(), 0u);
  CHECK(rig.deferred_.invariants_hold()); }

TEST(deferred_slices_add, waits_for_every_other_member_of_the_group_but_not_the_owner)
{ Rig               rig;
  const std::uint32_t group = rig.groups_.mint();
  Member              owner(rig.channel_, rig.groups_, group);
  Member              one(rig.channel_, rig.groups_, group);
  Member              two(rig.channel_, rig.groups_, group);
  owner.timeline_.hold();
  one.timeline_.hold();
  two.timeline_.hold();
  CHECK_STATUS(rig.deferred_.add(rig.groups_, group, &owner.buffers_, rig.entry(1u)), Status::ok);
  CHECK(rig.deferred_.invariants_hold());
  DeferredSlices::Entry ready = {};
  CHECK(!rig.deferred_.take_ready(rig.groups_, ready));
  one.timeline_.finish(1u);
  CHECK(!rig.deferred_.take_ready(rig.groups_, ready));
  two.timeline_.finish(1u);
  CHECK(rig.deferred_.take_ready(rig.groups_, ready));
  CHECK_EQ(ready.marks_, 2u);
  CHECK_EQ(rig.deferred_.pending(), 0u);
  CHECK(rig.deferred_.invariants_hold()); }

TEST(deferred_slices_add, an_idle_member_is_covered_at_the_fence_it_published_not_the_one_it_may_still_record)
{ Rig               rig;
  const std::uint32_t group = rig.groups_.mint();
  Member              owner(rig.channel_, rig.groups_, group);
  Member              idle(rig.channel_, rig.groups_, group);
  CHECK_STATUS(rig.deferred_.add(rig.groups_, group, &owner.buffers_, rig.entry(1u)), Status::ok);
  DeferredSlices::Entry ready = {};
  CHECK(rig.deferred_.take_ready(rig.groups_, ready)); }

TEST(deferred_slices_add, a_member_that_had_work_in_flight_when_the_slice_was_deferred_must_pass_it)
{ Rig               rig;
  const std::uint32_t group = rig.groups_.mint();
  Member              owner(rig.channel_, rig.groups_, group);
  Member              busy(rig.channel_, rig.groups_, group);
  busy.timeline_.send();
  busy.timeline_.send();
  CHECK_STATUS(rig.deferred_.add(rig.groups_, group, &owner.buffers_, rig.entry(1u)), Status::ok);
  DeferredSlices::Entry ready = {};
  CHECK(!rig.deferred_.take_ready(rig.groups_, ready));
  busy.timeline_.finish(1u);
  CHECK(!rig.deferred_.take_ready(rig.groups_, ready));
  busy.timeline_.finish(2u);
  CHECK(rig.deferred_.take_ready(rig.groups_, ready));
  CHECK(rig.deferred_.invariants_hold()); }

TEST(deferred_slices_add, work_the_member_records_after_the_deferral_is_not_waited_for)
{ Rig               rig;
  const std::uint32_t group = rig.groups_.mint();
  Member              owner(rig.channel_, rig.groups_, group);
  Member              later(rig.channel_, rig.groups_, group);
  later.timeline_.send();
  CHECK_STATUS(rig.deferred_.add(rig.groups_, group, &owner.buffers_, rig.entry(1u)), Status::ok);
  later.timeline_.send();
  later.timeline_.send();
  later.timeline_.finish(1u);
  DeferredSlices::Entry ready = {};
  CHECK(rig.deferred_.take_ready(rig.groups_, ready)); }

TEST(deferred_slices_add, members_of_a_group_joined_to_the_slices_group_are_waited_for_too)
{ Rig               rig;
  const std::uint32_t mine = rig.groups_.mint();
  const std::uint32_t other = rig.groups_.mint();
  Member              owner(rig.channel_, rig.groups_, mine);
  Member              joined(rig.channel_, rig.groups_, other);
  joined.timeline_.hold();
  CHECK_STATUS(rig.groups_.join(other, mine), Status::ok);
  CHECK_STATUS(rig.deferred_.add(rig.groups_, mine, &owner.buffers_, rig.entry(1u)), Status::ok);
  DeferredSlices::Entry ready = {};
  CHECK(!rig.deferred_.take_ready(rig.groups_, ready));
  joined.timeline_.finish(1u);
  CHECK(rig.deferred_.take_ready(rig.groups_, ready)); }

TEST(deferred_slices_add, a_member_of_a_different_group_is_not_waited_for)
{ Rig               rig;
  const std::uint32_t mine = rig.groups_.mint();
  const std::uint32_t other = rig.groups_.mint();
  Member              owner(rig.channel_, rig.groups_, mine);
  Member              stranger(rig.channel_, rig.groups_, other);
  stranger.timeline_.hold();
  CHECK_STATUS(rig.deferred_.add(rig.groups_, mine, &owner.buffers_, rig.entry(1u)), Status::ok);
  DeferredSlices::Entry ready = {};
  CHECK(rig.deferred_.take_ready(rig.groups_, ready)); }

TEST(deferred_slices_take_ready, a_member_that_leaves_the_group_stops_being_waited_for)
{ Rig               rig;
  const std::uint32_t group = rig.groups_.mint();
  Member              owner(rig.channel_, rig.groups_, group);
  Member              leaving(rig.channel_, rig.groups_, group);
  Member              staying(rig.channel_, rig.groups_, group);
  leaving.timeline_.hold();
  staying.timeline_.hold();
  CHECK_STATUS(rig.deferred_.add(rig.groups_, group, &owner.buffers_, rig.entry(1u)), Status::ok);
  leaving.buffers_.release();
  DeferredSlices::Entry ready = {};
  CHECK(!rig.deferred_.take_ready(rig.groups_, ready));
  staying.timeline_.finish(1u);
  CHECK(rig.deferred_.take_ready(rig.groups_, ready));
  CHECK_EQ(ready.marks_, 2u);
  CHECK(rig.deferred_.invariants_hold()); }

TEST(deferred_slices_take_ready, every_member_leaving_makes_the_slice_ready)
{ Rig               rig;
  const std::uint32_t group = rig.groups_.mint();
  Member              owner(rig.channel_, rig.groups_, group);
  Member              other(rig.channel_, rig.groups_, group);
  other.timeline_.hold();
  CHECK_STATUS(rig.deferred_.add(rig.groups_, group, &owner.buffers_, rig.entry(1u)), Status::ok);
  other.buffers_.release();
  DeferredSlices::Entry ready = {};
  CHECK(rig.deferred_.take_ready(rig.groups_, ready)); }

TEST(deferred_slices_take_ready, returns_the_first_entry_that_is_ready_not_the_first_added)
{ Rig               rig;
  const std::uint32_t slow_group = rig.groups_.mint();
  const std::uint32_t quick_group = rig.groups_.mint();
  Member              slow_owner(rig.channel_, rig.groups_, slow_group);
  Member              slow_other(rig.channel_, rig.groups_, slow_group);
  Member              quick_owner(rig.channel_, rig.groups_, quick_group);
  slow_other.timeline_.hold();
  CHECK_STATUS(rig.deferred_.add(rig.groups_, slow_group, &slow_owner.buffers_, rig.entry(1u)), Status::ok);
  CHECK_STATUS(rig.deferred_.add(rig.groups_, quick_group, &quick_owner.buffers_, rig.entry(2u)), Status::ok);
  CHECK_EQ(rig.deferred_.pending(), 2u);
  DeferredSlices::Entry ready = {};
  CHECK(rig.deferred_.take_ready(rig.groups_, ready));
  CHECK_EQ(ready.slice_.offset_, 2u);
  CHECK_EQ(rig.deferred_.pending(), 1u);
  CHECK(rig.deferred_.invariants_hold());
  CHECK(!rig.deferred_.take_ready(rig.groups_, ready));
  slow_other.timeline_.finish(1u);
  CHECK(rig.deferred_.take_ready(rig.groups_, ready));
  CHECK_EQ(ready.slice_.offset_, 1u);
  CHECK_EQ(rig.deferred_.pending(), 0u);
  CHECK(rig.deferred_.invariants_hold()); }

TEST(deferred_slices_take_ready, taking_an_entry_from_the_middle_keeps_the_marks_of_the_others_aligned)
{ Rig               rig;
  const std::uint32_t one_group = rig.groups_.mint();
  const std::uint32_t two_group = rig.groups_.mint();
  const std::uint32_t three_group = rig.groups_.mint();
  Member              a_owner(rig.channel_, rig.groups_, one_group);
  Member              a_other(rig.channel_, rig.groups_, one_group);
  Member              b_owner(rig.channel_, rig.groups_, two_group);
  Member              b_x(rig.channel_, rig.groups_, two_group);
  Member              b_y(rig.channel_, rig.groups_, two_group);
  Member              c_owner(rig.channel_, rig.groups_, three_group);
  Member              c_other(rig.channel_, rig.groups_, three_group);
  a_other.timeline_.hold();
  b_x.timeline_.hold();
  b_y.timeline_.hold();
  c_other.timeline_.hold();
  CHECK_STATUS(rig.deferred_.add(rig.groups_, one_group, &a_owner.buffers_, rig.entry(1u)), Status::ok);
  CHECK_STATUS(rig.deferred_.add(rig.groups_, two_group, &b_owner.buffers_, rig.entry(2u)), Status::ok);
  CHECK_STATUS(rig.deferred_.add(rig.groups_, three_group, &c_owner.buffers_, rig.entry(3u)), Status::ok);
  CHECK(rig.deferred_.invariants_hold());
  b_x.timeline_.finish(1u);
  b_y.timeline_.finish(1u);
  DeferredSlices::Entry ready = {};
  CHECK(rig.deferred_.take_ready(rig.groups_, ready));
  CHECK_EQ(ready.slice_.offset_, 2u);
  CHECK_EQ(ready.marks_, 2u);
  CHECK(rig.deferred_.invariants_hold());
  CHECK(!rig.deferred_.take_ready(rig.groups_, ready));
  c_other.timeline_.finish(1u);
  CHECK(rig.deferred_.take_ready(rig.groups_, ready));
  CHECK_EQ(ready.slice_.offset_, 3u);
  CHECK(!rig.deferred_.take_ready(rig.groups_, ready));
  a_other.timeline_.finish(1u);
  CHECK(rig.deferred_.take_ready(rig.groups_, ready));
  CHECK_EQ(ready.slice_.offset_, 1u);
  CHECK(rig.deferred_.invariants_hold()); }

TEST(deferred_slices_add, a_failed_allocation_adds_neither_the_entry_nor_its_marks)
{ Rig               rig;
  const std::uint32_t group = rig.groups_.mint();
  Member              owner(rig.channel_, rig.groups_, group);
  Member              other(rig.channel_, rig.groups_, group);
  rig.channel_.platform().fail_allocation_at(1u);
  CHECK_STATUS(rig.deferred_.add(rig.groups_, group, &owner.buffers_, rig.entry(1u)), Status::no_memory);
  CHECK_EQ(rig.deferred_.pending(), 0u);
  CHECK(rig.deferred_.invariants_hold());
  rig.channel_.platform().fail_allocation_at(2u);
  CHECK_STATUS(rig.deferred_.add(rig.groups_, group, &owner.buffers_, rig.entry(1u)), Status::no_memory);
  CHECK_EQ(rig.deferred_.pending(), 0u);
  CHECK(rig.deferred_.invariants_hold());
  CHECK_STATUS(rig.deferred_.add(rig.groups_, group, &owner.buffers_, rig.entry(1u)), Status::ok);
  CHECK_EQ(rig.deferred_.pending(), 1u);
  CHECK(rig.deferred_.invariants_hold()); }

TEST(deferred_slices_clear, drops_every_entry_and_every_mark)
{ Rig               rig;
  const std::uint32_t group = rig.groups_.mint();
  Member              owner(rig.channel_, rig.groups_, group);
  Member              other(rig.channel_, rig.groups_, group);
  other.timeline_.hold();
  for (std::uint64_t offset = 1u; offset <= 5u; offset++)
      CHECK_STATUS(rig.deferred_.add(rig.groups_, group, &owner.buffers_, rig.entry(offset)), Status::ok);
  CHECK_EQ(rig.deferred_.pending(), 5u);
  rig.deferred_.clear();
  CHECK_EQ(rig.deferred_.pending(), 0u);
  CHECK(rig.deferred_.invariants_hold());
  DeferredSlices::Entry ready = {};
  CHECK(!rig.deferred_.take_ready(rig.groups_, ready));
  CHECK_STATUS(rig.deferred_.add(rig.groups_, group, &owner.buffers_, rig.entry(9u)), Status::ok);
  CHECK(rig.deferred_.invariants_hold()); }

TEST(deferred_slices_invariants, fail_for_marks_the_entries_claim_but_the_list_lacks_or_a_list_that_lost_its_shape)
{ Rig               rig;
  const std::uint32_t group = rig.groups_.mint();
  Member              owner(rig.channel_, rig.groups_, group);
  CHECK_STATUS(rig.deferred_.add(rig.groups_, group, &owner.buffers_, rig.entry(1u)), Status::ok);
  CHECK(rig.deferred_.invariants_hold());
  DeferredSlicesProbe::claim_marks(rig.deferred_, 5u);
  CHECK(!rig.deferred_.invariants_hold());
  DeferredSlicesProbe::claim_marks(rig.deferred_, 0u);
  CHECK(rig.deferred_.invariants_hold());
  rig.deferred_.clear();
  DeferredSlicesProbe::corrupt_entries(rig.deferred_);
  CHECK(!rig.deferred_.invariants_hold());
  Rig other;
  DeferredSlicesProbe::corrupt_marks(other.deferred_);
  CHECK(!other.deferred_.invariants_hold()); }
