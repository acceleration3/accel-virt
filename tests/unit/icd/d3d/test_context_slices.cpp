/* The slices one Direct3D 11 context places its buffers on: an immediate
 * context places, renames and retires them; a deferred context, whose
 * resource records are copies of the immediate one's, owns none and only
 * forgets the slice a copy names - so the immediate buffer's slice is never
 * free while the buffer is still on it. */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/icd/scripted_gpu_timeline.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/d3d/buffer_slices.hpp"
#include "icd/d3d/context_slices.hpp"

#include <cstdint>

using av::LockTrace;
using av::RecordingThreadLock;
using av::ScriptedGpuTimeline;
using av::Status;
using av::d3d::BufferSlices;
using av::d3d::ContextSlices;

namespace {

constexpr std::uint64_t page = BufferSlices::large_alignment;

/* A session's slices over a store of 16 pages, an immediate context that owns
 * them and a deferred one that does not. */
class Rig {
public:
    Rig() : lock_(trace_, 'S'), slices_(platform_, timeline_, lock_)
    {
        slices_.take(16u * page);
        immediate_.own(&slices_);
    }
    BufferSlices        &slices() { return slices_; }
    ContextSlices       &immediate() { return immediate_; }
    ContextSlices       &deferred() { return deferred_; }
    ScriptedGpuTimeline &timeline() { return timeline_; }

private:
    av::MockPlatform    platform_;
    LockTrace           trace_;
    RecordingThreadLock lock_;
    ScriptedGpuTimeline timeline_;
    BufferSlices        slices_;
    ContextSlices       immediate_;
    ContextSlices       deferred_;
};

}  // namespace

/* The deferred context's copy of a record leaves the slice - as a map there
 * moves the buffer to the copy path - and the slice stays the immediate
 * buffer's: nothing is retired or freed, and the next buffer the immediate
 * context places is elsewhere, even with the GPU done with everything. */
TEST(context_slices_leave, a_deferred_context_forgets_the_slice_without_retiring_it)
{ Rig rig;
  BufferSlices::Slice immediate = {};
  CHECK(rig.immediate().place(256u, immediate));
  CHECK_EQ(immediate.bytes_, 256u);
  const std::uint32_t free = rig.slices().free_in_class(256u);
  BufferSlices::Slice copy = immediate;   /* the deferred context's record */
  rig.deferred().leave(copy);
  CHECK_EQ(copy.bytes_, 0u);
  CHECK_EQ(copy.offset_, 0u);
  CHECK_EQ(rig.slices().retired(), 0u);
  CHECK_EQ(rig.slices().free_in_class(256u), free);
  rig.timeline().send();
  rig.timeline().finish(rig.timeline().recording());
  for (std::uint32_t taken = 0u; taken <= free; taken++) {
      BufferSlices::Slice other = {};
      CHECK(rig.immediate().place(256u, other));
      CHECK(other.offset_ != immediate.offset_);
  }
  CHECK(rig.slices().invariants_hold()); }

/* The immediate context leaving its own record's slice retires it behind
 * what was recorded: it is free again once the GPU finishes that batch, and
 * not before. */
TEST(context_slices_leave, the_immediate_context_retires_the_slice_it_leaves)
{ Rig rig;
  BufferSlices::Slice slice = {};
  CHECK(rig.immediate().place(2u * page, slice));
  const BufferSlices::Slice placed = slice;
  rig.immediate().leave(slice);
  CHECK_EQ(slice.bytes_, 0u);
  CHECK_EQ(rig.slices().retired(), 1u);
  rig.timeline().send();
  BufferSlices::Slice next = {};
  CHECK(rig.immediate().place(2u * page, next));
  CHECK(next.offset_ != placed.offset_);
  rig.timeline().finish(1u);
  BufferSlices::Slice back = {};
  CHECK(rig.immediate().place(2u * page, back));
  CHECK_EQ(back.offset_, placed.offset_);
  CHECK(rig.slices().invariants_hold()); }

/* A deferred context places nothing and renames nothing - its buffers are
 * copied - and leaves `slice` as it was. */
TEST(context_slices_place, a_context_with_no_slices_places_and_renames_nothing)
{ Rig rig;
  BufferSlices::Slice slice = {7u * page, 256u};
  CHECK(!rig.deferred().place(256u, slice));
  CHECK(!rig.deferred().rename(256u, slice));
  CHECK_EQ(slice.offset_, 7u * page);
  CHECK_EQ(slice.bytes_, 256u);
  CHECK_EQ(rig.slices().retired(), 0u); }

/* A rename is a fresh slice for the buffer and the old one retired behind
 * what was recorded; with none to take, the buffer keeps its slice and
 * nothing is retired. */
TEST(context_slices_rename, takes_a_fresh_slice_and_retires_the_old_or_changes_nothing)
{ Rig rig;
  BufferSlices::Slice slice = {};
  CHECK(rig.immediate().place(8u * page, slice));
  const BufferSlices::Slice first = slice;
  CHECK(rig.immediate().rename(8u * page, slice));
  CHECK(slice.offset_ != first.offset_);
  CHECK_EQ(slice.bytes_, 8u * page);
  CHECK_EQ(rig.slices().retired(), 1u);
  const BufferSlices::Slice second = slice;
  CHECK(!rig.immediate().rename(16u * page + 1u, slice));   /* more than the store */
  CHECK_EQ(slice.offset_, second.offset_);
  CHECK_EQ(slice.bytes_, second.bytes_);
  CHECK_EQ(rig.slices().retired(), 1u);
  CHECK(rig.slices().invariants_hold()); }
