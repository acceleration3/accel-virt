/* Where a Direct3D 12 device's upload and readback heaps sit in its session's
 * blob arena: 64 KiB aligned ranges inside what the arena does not lend,
 * first fit, a range given back reused by the next heap that fits, and every
 * placement and removal under the lock and nothing else. */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "icd/d3d/d3d12_heap_arena.hpp"

#include <cstdint>

using av::LockTrace;
using av::RecordingThreadLock;
using av::Status;
using av::d3d::D3d12HeapArena;

namespace {

constexpr std::uint64_t page = D3d12HeapArena::alignment;

}  // namespace

TEST(d3d12_heap_arena, heaps_are_placed_first_fit_and_aligned_inside_the_range)
{ LockTrace trace;
  RecordingThreadLock lock(trace, 'A');
  D3d12HeapArena arena(lock);
  /* Rounded inward: the first whole page after 16 MiB + 1, up to the last
   * whole page before the end. */
  arena.take((16u << 20) + 1u, (16u << 20) + 5u * page + 100u);
  std::uint64_t first = 0u, second = 0u, third = 0u;
  CHECK_STATUS(arena.place(1u, first), Status::ok);
  CHECK_EQ(first, (16u << 20) + page);
  CHECK_STATUS(arena.place(page + 1u, second), Status::ok);
  CHECK_EQ(second, first + page);
  CHECK_EQ(arena.bytes_placed(), 3u * page);
  CHECK(arena.invariants_hold());
  /* One page is left before the end: two do not fit, one does. */
  CHECK_STATUS(arena.place(2u * page, third), Status::no_space);
  CHECK_EQ(third, 0u);
  CHECK_STATUS(arena.place(page, third), Status::ok);
  CHECK_EQ(third, second + 2u * page);
  CHECK_EQ(arena.placed(), 3u);
  CHECK(!lock.held());
  CHECK(arena.invariants_hold()); }

TEST(d3d12_heap_arena, a_removed_heap_leaves_a_gap_the_next_that_fits_takes)
{ LockTrace trace;
  RecordingThreadLock lock(trace, 'A');
  D3d12HeapArena arena(lock);
  arena.take(0u, 8u * page);
  std::uint64_t a = 0u, b = 0u, c = 0u, d = 0u;
  CHECK_STATUS(arena.place(2u * page, a), Status::ok);
  CHECK_STATUS(arena.place(2u * page, b), Status::ok);
  CHECK_STATUS(arena.place(2u * page, c), Status::ok);
  CHECK_STATUS(arena.remove(b), Status::ok);
  CHECK(arena.invariants_hold());
  /* Three pages do not fit in b's two, so they go after c. */
  CHECK_STATUS(arena.place(3u * page, d), Status::no_space);
  CHECK_STATUS(arena.place(page, d), Status::ok);
  CHECK_EQ(d, b);
  std::uint64_t e = 0u;
  CHECK_STATUS(arena.place(page, e), Status::ok);
  CHECK_EQ(e, b + page);
  CHECK_EQ(arena.placed(), 4u);
  CHECK(arena.invariants_hold()); }

TEST(d3d12_heap_arena, an_offset_no_heap_has_is_refused_and_nothing_moves)
{ LockTrace trace;
  RecordingThreadLock lock(trace, 'A');
  D3d12HeapArena arena(lock);
  arena.take(0u, 4u * page);
  std::uint64_t a = 0u;
  CHECK_STATUS(arena.place(page, a), Status::ok);
  CHECK_STATUS(arena.remove(a + 1u), Status::invalid_parameter);
  CHECK_STATUS(arena.remove(a), Status::ok);
  CHECK_STATUS(arena.remove(a), Status::invalid_parameter);
  CHECK_EQ(arena.placed(), 0u);
  CHECK(!lock.held());
  CHECK(arena.invariants_hold()); }

TEST(d3d12_heap_arena, no_bytes_no_arena_and_a_full_table_are_refused)
{ LockTrace trace;
  RecordingThreadLock lock(trace, 'A');
  D3d12HeapArena arena(lock);
  std::uint64_t at = 7u;
  /* Nothing taken: there is no room at all. */
  CHECK_STATUS(arena.place(page, at), Status::no_space);
  CHECK_EQ(at, 0u);
  arena.take(0u, (D3d12HeapArena::max_heaps + 1u) * page);
  CHECK_STATUS(arena.place(0u, at), Status::invalid_parameter);
  for (std::uint32_t index = 0u; index < D3d12HeapArena::max_heaps; index++)
      CHECK_STATUS(arena.place(page, at), Status::ok);
  CHECK_STATUS(arena.place(page, at), Status::no_space);
  CHECK_EQ(arena.placed(), D3d12HeapArena::max_heaps);
  /* An arena taken again forgets what was placed in the old one. */
  arena.take(page, 3u * page);
  CHECK_EQ(arena.placed(), 0u);
  CHECK_STATUS(arena.place(page, at), Status::ok);
  CHECK_EQ(at, page);
  /* An end before the begin is an empty arena, not a wrapped one. */
  arena.take(4u * page, page);
  CHECK_STATUS(arena.place(page, at), Status::no_space);
  CHECK(arena.invariants_hold()); }
