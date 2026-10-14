/* The slices of a session's slice store that CPU-written buffers live in
 * (docs/mapped-buffers-plan.md, 3.1; docs/host-memory.md): offsets into host
 * upload memory, with the bookkeeping kept apart from it. A slice retired is
 * not handed out again until the GPU has finished the batch that was being
 * recorded when it was retired; small slices come from their class's stack,
 * large ones whole from the store; and when the store is full the oldest
 * retired slice is waited for rather than any slice reused early. */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/icd/scripted_gpu_timeline.hpp"
#include "icd/d3d/buffer_slices.hpp"

#include <cstdint>

using av::LockTrace;
using av::ScriptedGpuTimeline;
using av::RecordingThreadLock;
using av::Status;
using av::d3d::BufferSlices;

namespace {

constexpr std::uint64_t kib = 1024u;
constexpr std::uint64_t page = BufferSlices::large_alignment;
/* Slices are offsets into the store, from its first byte: the bookkeeping is
 * elsewhere. */
constexpr std::uint64_t start = 0u;
/* The bookkeeping, as the session's arena holds it. */
alignas(4096) std::uint8_t books[BufferSlices::metadata_bytes];

class Rig {
public:
    Rig() : lock_(trace_, 'S'), slices_(timeline_, lock_) {}
    BufferSlices &slices() { return slices_; }
    /* Slices in a store of `pages`. */
    void          take(std::uint64_t pages) { slices_.take(books, pages * page); }
    ScriptedGpuTimeline &timeline() { return timeline_; }

private:
    LockTrace           trace_;
    RecordingThreadLock lock_;
    ScriptedGpuTimeline timeline_;
    BufferSlices        slices_;
};

}  // namespace

/* A small slice is its class's size and alignment; two are apart; one given
 * back is the next of its class handed out. */
TEST(buffer_slices, small_slices_come_from_their_class_and_come_back)
{ Rig rig;
  rig.take(16u);
  BufferSlices::Slice first = {}, second = {}, again = {};
  CHECK_STATUS(rig.slices().take_slice(100u, first), Status::ok);
  CHECK_EQ(first.bytes_, 256u);
  CHECK_EQ(first.offset_, start);
  CHECK_STATUS(rig.slices().take_slice(256u, second), Status::ok);
  CHECK_EQ(second.offset_, start + 256u);
  BufferSlices::Slice wider = {};
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

/* A retired slice comes back only once the GPU has finished the batch being
 * recorded when it was retired: not when that batch is merely sent. */
TEST(buffer_slices, a_retired_slice_waits_for_the_gpu_to_finish_its_batch)
{ Rig rig;
  rig.take(16u);
  BufferSlices::Slice old = {};
  CHECK_STATUS(rig.slices().take_slice(64u * kib, old), Status::ok);
  rig.slices().retire(old);                 /* tagged with batch 1 */
  CHECK_EQ(rig.slices().retired(), 1u);
  BufferSlices::Slice next = {};
  CHECK_STATUS(rig.slices().take_slice(64u * kib, next), Status::ok);
  CHECK(next.offset_ != old.offset_);
  rig.slices().give_back(next);
  rig.timeline().send();                    /* batch 1 out, not finished */
  BufferSlices::Slice still = {};
  CHECK_STATUS(rig.slices().take_slice(64u * kib, still), Status::ok);
  CHECK(still.offset_ != old.offset_);
  CHECK_EQ(rig.slices().retired(), 1u);
  rig.timeline().finish(1u);
  BufferSlices::Slice back = {};
  CHECK_STATUS(rig.slices().take_slice(64u * kib, back), Status::ok);
  CHECK_EQ(rig.slices().retired(), 0u);
  CHECK_EQ(back.offset_, old.offset_);
  CHECK_EQ(rig.timeline().waits(), 0u);
  CHECK(rig.slices().invariants_hold()); }

/* A large slice is whole pages placed first fit, and goes back to the arena
 * whole, where the next one that fits takes it. */
TEST(buffer_slices, a_large_slice_is_whole_pages_and_goes_back_whole)
{ Rig rig;
  rig.slices().take(books, 3u * page + 100u);   /* rounded down to whole pages */
  CHECK_EQ(rig.slices().begin(), start);
  CHECK_EQ(rig.slices().end(), start + 3u * page);
  BufferSlices::Slice large = {}, rest = {}, again = {};
  CHECK_STATUS(rig.slices().take_slice(64u * kib + 1u, large), Status::ok);
  CHECK_EQ(large.offset_, start);
  CHECK_EQ(large.bytes_, 2u * page);
  CHECK_STATUS(rig.slices().take_slice(page + 1u, rest), Status::no_space);   /* two pages: one left */
  CHECK_STATUS(rig.slices().take_slice(64u * kib, rest), Status::no_space);    /* a small class's block: no room */
  CHECK_EQ(rest.bytes_, 0u);
  rig.slices().retire(large);
  rig.timeline().finish(1u);
  CHECK_STATUS(rig.slices().take_slice(2u * page, again), Status::ok);
  CHECK_EQ(again.offset_, large.offset_);
  CHECK(rig.slices().invariants_hold()); }

/* An arena with no room waits for the oldest retired slice - sending the
 * batch being recorded if that is the one - rather than reuse one early; with
 * nothing retired there is nothing to wait for. */
TEST(buffer_slices, a_full_arena_waits_for_the_oldest_retired_slice)
{ Rig rig;
  rig.take(4u);
  BufferSlices::Slice first = {}, second = {}, third = {};
  CHECK_STATUS(rig.slices().take_slice(2u * page, first), Status::ok);
  CHECK_STATUS(rig.slices().take_slice(2u * page, second), Status::ok);
  CHECK_STATUS(rig.slices().take_slice(2u * page, third), Status::no_space);
  CHECK_EQ(rig.timeline().waits(), 0u);
  rig.slices().retire(first);
  CHECK_STATUS(rig.slices().take_slice(2u * page, third), Status::ok);
  CHECK_EQ(rig.timeline().waits(), 1u);
  CHECK_EQ(third.offset_, first.offset_);
  const av::d3d::GpuTimeline &gpu = rig.timeline();   /* as the slices see it */
  CHECK(gpu.completed() >= 1u);
  CHECK(gpu.recording() > 1u);   /* the batch was sent to be waited for */
  CHECK(rig.slices().invariants_hold()); }

/* A small class carves no more than max_per_class slices: past that it waits
 * for its own to come back, even with the arena free. */
TEST(buffer_slices, a_class_at_its_bound_waits_for_its_own_slices)
{ Rig rig;
  rig.take(1024u);
  BufferSlices::Slice slice = {};
  for (std::uint32_t index = 0u; index < BufferSlices::max_per_class; index++) {
      CHECK_STATUS(rig.slices().take_slice(1u, slice), Status::ok);
      if (index == 0u) rig.slices().retire(slice);
  }
  CHECK_EQ(rig.timeline().waits(), 0u);
  CHECK_STATUS(rig.slices().take_slice(1u, slice), Status::ok);
  CHECK_EQ(rig.timeline().waits(), 1u);
  CHECK_EQ(slice.offset_, start);
  CHECK(rig.slices().invariants_hold()); }

/* A queue of retired slices that is full waits for its oldest before it
 * takes another, and keeps them in order of their batches. Two small classes
 * fill it, half each. */
TEST(buffer_slices, a_full_retired_queue_waits_for_its_oldest)
{ Rig rig;
  rig.take(1024u);
  BufferSlices::Slice slice = {};
  for (std::uint32_t index = 0u; index < BufferSlices::max_retired; index++) {
      CHECK_STATUS(rig.slices().take_slice(index % 2u == 0u ? 256u : 512u, slice), Status::ok);
      rig.slices().retire(slice);
      if (index % 1000u == 999u) rig.timeline().send();
  }
  CHECK_EQ(rig.slices().retired(), BufferSlices::max_retired);
  CHECK_EQ(rig.timeline().waits(), 0u);
  CHECK_STATUS(rig.slices().take_slice(64u * kib, slice), Status::ok);
  rig.slices().retire(slice);
  CHECK_EQ(rig.timeline().waits(), 1u);
  CHECK(rig.slices().retired() < BufferSlices::max_retired);
  CHECK(rig.slices().invariants_hold()); }

/* A session that cannot be waited on hands out nothing more that would need a
 * wait, and nothing at all once lost; take() starts afresh. */
TEST(buffer_slices, a_lost_session_hands_out_nothing_more)
{ Rig rig;
  rig.take(2u);
  BufferSlices::Slice slice = {};
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
  BufferSlices::Slice slice = {7u, 7u};
  CHECK_STATUS(rig.slices().take_slice(1u, slice), Status::invalid_parameter);   /* no arena */
  CHECK_EQ(slice.bytes_, 0u);
  rig.take(2u);
  CHECK_STATUS(rig.slices().take_slice(0u, slice), Status::invalid_parameter);
  CHECK_STATUS(rig.slices().take_slice(2u * page + 1u, slice), Status::invalid_parameter);
  CHECK_STATUS(rig.slices().take_slice(2u * page, slice), Status::ok);
  rig.slices().retire(BufferSlices::Slice{0u, 0u});   /* none: nothing */
  rig.slices().give_back(BufferSlices::Slice{0u, 0u});
  CHECK_EQ(rig.slices().retired(), 0u);
  CHECK(rig.slices().invariants_hold()); }

/* A store smaller than one large slice, or no bookkeeping to keep them in,
 * holds no slices; the whole of a store that does is slices, from its first
 * byte to its last whole page. */
TEST(buffer_slices, a_store_under_a_page_or_without_books_holds_none)
{ Rig rig;
  BufferSlices::Slice slice = {};
  rig.slices().take(books, page - 1u);
  CHECK_EQ(rig.slices().end(), 0u);
  CHECK_STATUS(rig.slices().take_slice(1u, slice), Status::invalid_parameter);
  rig.slices().take(nullptr, 4u * page);
  CHECK_STATUS(rig.slices().take_slice(1u, slice), Status::invalid_parameter);
  rig.slices().retire(BufferSlices::Slice{start, 256u});   /* nothing to retire into */
  CHECK_EQ(rig.slices().retired(), 0u);
  CHECK(rig.slices().invariants_hold());
  rig.slices().take(books, 2u * page + page / 2u);
  CHECK_EQ(rig.slices().begin(), start);
  CHECK_EQ(rig.slices().end(), 2u * page);
  CHECK_STATUS(rig.slices().take_slice(1u, slice), Status::no_space);       /* a block is four pages */
  CHECK_STATUS(rig.slices().take_slice(page + 1u, slice), Status::ok);      /* two pages, large */
  CHECK_EQ(slice.offset_, start);
  CHECK(rig.slices().invariants_hold()); }
