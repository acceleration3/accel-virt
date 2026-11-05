#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/icd/scripted_gpu_timeline.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/common/stream_memory.hpp"

#include <cstdint>

using av::LockTrace;
using av::MockPlatform;
using av::ScriptedGpuTimeline;
using av::RecordingThreadLock;
using av::Status;
using av::icd::StreamMemory;

namespace {

constexpr std::uint64_t kib = 1024u;
constexpr std::uint64_t page = StreamMemory::large_alignment;
constexpr std::uint64_t start = 0u;

class Rig {
public:
    Rig() : lock_(trace_, 'S'), slices_(platform_, timeline_, lock_) {}
    StreamMemory &slices() { return slices_; }
    void          take(std::uint64_t pages) { slices_.take(pages * page); }
    ScriptedGpuTimeline &timeline() { return timeline_; }
    MockPlatform        &platform() { return platform_; }

private:
    MockPlatform        platform_;
    LockTrace           trace_;
    RecordingThreadLock lock_;
    ScriptedGpuTimeline timeline_;
    StreamMemory        slices_;
};

}

TEST(buffer_slices, small_slices_come_from_their_class_and_come_back)
{ Rig rig;
  rig.take(16u);
  StreamMemory::Slice first = {}, second = {}, again = {};
  CHECK_STATUS(rig.slices().take_slice(100u, first), Status::ok);
  CHECK_EQ(first.bytes_, 256u);
  CHECK_EQ(first.offset_, start);
  CHECK_STATUS(rig.slices().take_slice(256u, second), Status::ok);
  CHECK_EQ(second.offset_, start + 256u);
  StreamMemory::Slice wider = {};
  CHECK_STATUS(rig.slices().take_slice(257u, wider), Status::ok);
  CHECK_EQ(wider.bytes_, 512u);
  CHECK_EQ(wider.offset_ % 512u, 0u);
  CHECK(wider.offset_ + wider.bytes_ <= start + 16u * page);
  const std::uint32_t free = rig.slices().free_in_class(256u);
  rig.slices().give_back(first);
  CHECK_EQ(rig.slices().free_in_class(256u), free + 1u);
  CHECK_STATUS(rig.slices().take_slice(1u, again), Status::ok);
  CHECK_EQ(again.offset_, first.offset_);
  CHECK_EQ(rig.timeline().waits(), 0u);
  CHECK(rig.slices().invariants_hold()); }

TEST(buffer_slices, a_retired_slice_waits_for_the_gpu_to_finish_its_batch)
{ Rig rig;
  rig.take(16u);
  StreamMemory::Slice old = {};
  CHECK_STATUS(rig.slices().take_slice(64u * kib, old), Status::ok);
  rig.slices().retire(old);
  CHECK_EQ(rig.slices().retired(), 1u);
  StreamMemory::Slice next = {};
  CHECK_STATUS(rig.slices().take_slice(64u * kib, next), Status::ok);
  CHECK(next.offset_ != old.offset_);
  rig.slices().give_back(next);
  rig.timeline().send();
  StreamMemory::Slice still = {};
  CHECK_STATUS(rig.slices().take_slice(64u * kib, still), Status::ok);
  CHECK(still.offset_ != old.offset_);
  CHECK_EQ(rig.slices().retired(), 1u);
  rig.timeline().finish(1u);
  StreamMemory::Slice back = {};
  CHECK_STATUS(rig.slices().take_slice(64u * kib, back), Status::ok);
  CHECK_EQ(rig.slices().retired(), 0u);
  CHECK_EQ(back.offset_, old.offset_);
  CHECK_EQ(rig.timeline().waits(), 0u);
  CHECK(rig.slices().invariants_hold()); }

TEST(buffer_slices, a_large_slice_is_whole_pages_and_goes_back_whole)
{ Rig rig;
  rig.slices().take(3u * page + 100u);
  CHECK_EQ(rig.slices().begin(), start);
  CHECK_EQ(rig.slices().end(), start + 3u * page);
  StreamMemory::Slice large = {}, rest = {}, again = {};
  CHECK_STATUS(rig.slices().take_slice(64u * kib + 1u, large), Status::ok);
  CHECK_EQ(large.offset_, start);
  CHECK_EQ(large.bytes_, 2u * page);
  CHECK_STATUS(rig.slices().take_slice(page + 1u, rest), Status::no_space);
  CHECK_STATUS(rig.slices().take_slice(64u * kib, rest), Status::no_space);
  CHECK_EQ(rest.bytes_, 0u);
  rig.slices().retire(large);
  rig.timeline().finish(1u);
  CHECK_STATUS(rig.slices().take_slice(2u * page, again), Status::ok);
  CHECK_EQ(again.offset_, large.offset_);
  CHECK(rig.slices().invariants_hold()); }

TEST(buffer_slices, a_full_arena_waits_for_the_oldest_retired_slice)
{ Rig rig;
  rig.take(4u);
  StreamMemory::Slice first = {}, second = {}, third = {};
  CHECK_STATUS(rig.slices().take_slice(2u * page, first), Status::ok);
  CHECK_STATUS(rig.slices().take_slice(2u * page, second), Status::ok);
  CHECK_STATUS(rig.slices().take_slice(2u * page, third), Status::no_space);
  CHECK_EQ(rig.timeline().waits(), 0u);
  rig.slices().retire(first);
  CHECK_STATUS(rig.slices().take_slice(2u * page, third), Status::ok);
  CHECK_EQ(rig.timeline().waits(), 1u);
  CHECK_EQ(third.offset_, first.offset_);
  const av::icd::GpuTimeline &gpu = rig.timeline();
  CHECK(gpu.completed() >= 1u);
  CHECK(gpu.recording() > 1u);
  CHECK(rig.slices().invariants_hold()); }

TEST(buffer_slices, a_class_carves_while_the_store_has_room)
{ Rig rig;
  rig.take(1024u);
  static bool         used[1024u * 64u * 1024u / 256u];
  StreamMemory::Slice slice = {};
  std::uint32_t       twice = 0u;
  for (std::uint32_t index = 0u; index < 40000u; index++) {
      CHECK_STATUS(rig.slices().take_slice(1u, slice), Status::ok);
      twice += used[slice.offset_ / 256u] ? 1u : 0u;
      used[slice.offset_ / 256u] = true;
  }
  CHECK_EQ(twice, 0u);
  CHECK_EQ(rig.timeline().waits(), 0u);
  CHECK(rig.slices().invariants_hold()); }

TEST(buffer_slices, the_retired_queue_holds_every_slice_in_flight)
{ Rig rig;
  rig.take(1024u);
  StreamMemory::Slice slice = {};
  for (std::uint32_t index = 0u; index < 40000u; index++) {
      CHECK_STATUS(rig.slices().take_slice(index % 2u == 0u ? 256u : 512u, slice), Status::ok);
      rig.slices().retire(slice);
      if (index % 1000u == 999u) rig.timeline().send();
  }
  CHECK_EQ(rig.slices().retired(), 40000u);
  CHECK_EQ(rig.timeline().waits(), 0u);
  CHECK(rig.slices().invariants_hold());
  rig.timeline().finish(20u);
  CHECK_STATUS(rig.slices().take_slice(256u, slice), Status::ok);
  CHECK_EQ(rig.slices().retired(), 20000u);
  CHECK(rig.slices().invariants_hold());
  rig.timeline().finish(41u);
  CHECK_STATUS(rig.slices().take_slice(256u, slice), Status::ok);
  CHECK_EQ(rig.slices().retired(), 0u);
  CHECK_EQ(rig.slices().free_in_class(256u), 20u * 1024u - 2u);
  CHECK(rig.slices().invariants_hold()); }

TEST(buffer_slices, a_partly_reclaimed_queue_keeps_its_order)
{ Rig rig;
  rig.take(16u);
  StreamMemory::Slice a = {}, b = {}, c = {}, next = {};
  CHECK_STATUS(rig.slices().take_slice(256u, a), Status::ok);
  CHECK_STATUS(rig.slices().take_slice(256u, b), Status::ok);
  CHECK_STATUS(rig.slices().take_slice(256u, c), Status::ok);
  rig.slices().retire(a);
  rig.timeline().send();
  rig.slices().retire(b);
  rig.slices().retire(c);
  rig.timeline().finish(1u);
  CHECK_STATUS(rig.slices().take_slice(256u, next), Status::ok);
  CHECK_EQ(next.offset_, a.offset_);
  CHECK_EQ(rig.slices().retired(), 2u);
  CHECK(rig.slices().invariants_hold());
  rig.timeline().finish(2u);
  CHECK_STATUS(rig.slices().take_slice(256u, next), Status::ok);
  CHECK_EQ(rig.slices().retired(), 0u);
  CHECK(rig.slices().invariants_hold()); }

TEST(buffer_slices, no_memory_for_the_bookkeeping_refuses_the_slice)
{ Rig rig;
  rig.take(16u);
  StreamMemory::Slice slice = {7u, 7u};
  rig.platform().fail_allocation_at(1u);
  CHECK_STATUS(rig.slices().take_slice(256u, slice), Status::no_memory);
  CHECK_EQ(slice.bytes_, 0u);
  rig.platform().fail_allocation_at(2u);
  CHECK_STATUS(rig.slices().take_slice(256u, slice), Status::no_memory);
  CHECK_EQ(rig.slices().free_in_class(256u), 0u);
  rig.platform().fail_allocation_at(2u);
  CHECK_STATUS(rig.slices().take_slice(page, slice), Status::no_memory);
  CHECK(rig.slices().invariants_hold());
  StreamMemory::Slice small = {}, large = {};
  CHECK_STATUS(rig.slices().take_slice(256u, small), Status::ok);
  CHECK_STATUS(rig.slices().take_slice(page, large), Status::ok);
  rig.slices().retire(small);
  rig.slices().give_back(large);
  CHECK_EQ(rig.slices().retired(), 1u);
  CHECK(rig.slices().invariants_hold()); }

TEST(buffer_slices, a_slice_from_before_the_store_was_taken_again_is_harmless)
{ Rig rig;
  rig.take(16u);
  StreamMemory::Slice old = {}, other = {};
  CHECK_STATUS(rig.slices().take_slice(page, old), Status::ok);
  CHECK_STATUS(rig.slices().take_slice(page, other), Status::ok);
  rig.take(16u);
  rig.slices().retire(old);
  rig.slices().give_back(other);
  CHECK_EQ(rig.slices().retired(), 1u);
  CHECK(rig.slices().invariants_hold()); }

TEST(buffer_slices, a_lost_session_hands_out_nothing_more)
{ Rig rig;
  rig.take(2u);
  StreamMemory::Slice slice = {};
  CHECK_STATUS(rig.slices().take_slice(2u * page, slice), Status::ok);
  rig.slices().retire(slice);
  rig.timeline().lose();
  CHECK_STATUS(rig.slices().take_slice(2u * page, slice), Status::invalid_state);
  CHECK_STATUS(rig.slices().take_slice(page + 1u, slice), Status::invalid_state);
  rig.take(2u);
  CHECK_EQ(rig.slices().retired(), 0u);
  CHECK_STATUS(rig.slices().take_slice(2u * page, slice), Status::ok); }

TEST(buffer_slices, no_bytes_or_more_than_the_arena_is_refused)
{ Rig rig;
  StreamMemory::Slice slice = {7u, 7u};
  CHECK_STATUS(rig.slices().take_slice(1u, slice), Status::invalid_parameter);
  CHECK_EQ(slice.bytes_, 0u);
  rig.take(2u);
  CHECK_STATUS(rig.slices().take_slice(0u, slice), Status::invalid_parameter);
  CHECK_STATUS(rig.slices().take_slice(2u * page + 1u, slice), Status::invalid_parameter);
  CHECK_STATUS(rig.slices().take_slice(2u * page, slice), Status::ok);
  rig.slices().retire(StreamMemory::Slice{0u, 0u});
  rig.slices().give_back(StreamMemory::Slice{0u, 0u});
  CHECK_EQ(rig.slices().retired(), 0u);
  CHECK(rig.slices().invariants_hold()); }

TEST(buffer_slices, a_store_under_a_page_holds_none)
{ Rig rig;
  StreamMemory::Slice slice = {};
  rig.slices().take(page - 1u);
  CHECK_EQ(rig.slices().end(), 0u);
  CHECK_STATUS(rig.slices().take_slice(1u, slice), Status::invalid_parameter);
  rig.slices().retire(StreamMemory::Slice{start, 256u});
  CHECK_EQ(rig.slices().retired(), 0u);
  CHECK(rig.slices().invariants_hold());
  rig.slices().take(2u * page + page / 2u);
  CHECK_EQ(rig.slices().begin(), start);
  CHECK_EQ(rig.slices().end(), 2u * page);
  CHECK_STATUS(rig.slices().take_slice(1u, slice), Status::no_space);
  CHECK_STATUS(rig.slices().take_slice(page + 1u, slice), Status::ok);
  CHECK_EQ(slice.offset_, start);
  CHECK(rig.slices().invariants_hold()); }
