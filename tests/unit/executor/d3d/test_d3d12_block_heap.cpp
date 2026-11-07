#include "core/log/driver_log.hpp"
#include "executor/d3d/d3d12_block_heap.hpp"
#include "executor/d3d/d3d12_block_heaps.hpp"
#include "executor/d3d/d3d12_queue_progress.hpp"
#include "executor/eventfd_wake.hpp"
#include "executor/shim/d3d12_host_device.hpp"
#include "executor/source_memory.hpp"
#include "harness/executor/slice_store_rig.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::D3d12BlockHeap;
using av::D3d12BlockHeaps;
using av::SliceStoreRig;
using av::SourceMemory;

namespace {

struct Heaps {
    SliceStoreRig          rig;
    av::EventfdWake        wake;
    av::D3d12QueueProgress progress{wake, rig.platform()};
    av::RecordingLog       sink;
    av::DriverLog          lines{sink, "heaps: "};
    av::D3d12HostDevice    device{lines};
    D3d12BlockHeaps        heaps{rig.platform(), progress, lines};
};

}

TEST(d3d12_block_heap, a_block_holds_a_reference_to_its_memory_for_as_long_as_it_lives)
{ SliceStoreRig rig;
  SourceMemory &store = *rig.store(1u);
  const std::uint32_t before = store.references();
  {
      D3d12BlockHeap block(store);
      CHECK_EQ(store.references(), before + 1u);
      CHECK(block.holds(store));
      CHECK_EQ(block.bytes(), store.size());
      CHECK(block.heap() == nullptr);
      CHECK(!block.abandoned());
      CHECK(block.unreferenced());
      CHECK(block.invariants_hold());
  }
  CHECK_EQ(store.references(), before); }

TEST(d3d12_block_heap, a_block_belongs_to_one_memory_only)
{ SliceStoreRig rig;
  SourceMemory &one = *rig.store(1u);
  SourceMemory &two = *rig.store(2u);
  D3d12BlockHeap block(one);
  CHECK(block.holds(one));
  CHECK(!block.holds(two)); }

TEST(d3d12_block_heap, a_heap_or_resource_taken_is_referenced_until_it_is_let_go)
{ SliceStoreRig rig;
  D3d12BlockHeap block(*rig.store(1u));
  block.took_heap();
  block.took_heap();
  block.took_resource();
  CHECK(!block.unreferenced());
  block.let_go_heap(4u);
  CHECK(!block.unreferenced());
  block.let_go_heap(4u);
  CHECK(!block.unreferenced());
  block.let_go_resource(4u);
  CHECK(block.unreferenced());
  block.let_go_heap(4u);
  block.let_go_resource(4u);
  CHECK(block.unreferenced()); }

TEST(d3d12_block_heap, a_block_is_done_once_abandoned_unreferenced_and_the_gpu_has_passed_its_last_use)
{ SliceStoreRig rig;
  D3d12BlockHeap block(*rig.store(1u));
  block.took_heap();
  block.took_resource();
  CHECK(!block.done(100u));
  block.let_go_heap(5u);
  block.let_go_resource(9u);
  CHECK(block.unreferenced());
  CHECK(!block.done(100u));
  block.abandon(3u);
  CHECK(block.abandoned());
  CHECK(!block.done(8u));
  CHECK(block.done(9u));
  CHECK(block.done(100u)); }

TEST(d3d12_block_heap, abandoning_while_referenced_waits_for_the_last_to_be_let_go)
{ SliceStoreRig rig;
  D3d12BlockHeap block(*rig.store(1u));
  block.took_resource();
  block.abandon(7u);
  CHECK(!block.done(100u));
  block.let_go_resource(2u);
  CHECK(!block.done(6u));
  CHECK(block.done(7u)); }

TEST(d3d12_block_heap, a_host_device_that_is_not_open_makes_no_heap)
{ Heaps held;
  D3d12BlockHeap block(*held.rig.store(1u));
  CHECK(!block.import(held.device));
  CHECK(block.heap() == nullptr);
  CHECK(block.invariants_hold()); }

TEST(d3d12_block_heaps, a_new_set_holds_no_block_and_knows_no_window)
{ Heaps held;
  std::uint64_t base = 7u, size = 7u;
  CHECK_EQ(held.heaps.count(), 0u);
  CHECK(!held.heaps.window(1u, base, size));
  CHECK_EQ(base, 7u);
  CHECK(!held.heaps.attach(1u, 1u));
  CHECK(held.heaps.pin(1u) == nullptr);
  held.heaps.detach(1u);
  held.heaps.release_heap(1u);
  held.heaps.reclaim();
  CHECK(held.heaps.invariants_hold()); }

TEST(d3d12_block_heaps, a_window_that_is_empty_unaligned_or_past_its_memory_is_refused_with_a_line_in_the_log)
{ Heaps held;
  SourceMemory &store = *held.rig.store(1u);
  CHECK(held.heaps.open(held.device, store, 1u, 0u, 0u) == nullptr);
  CHECK(held.heaps.open(held.device, store, 1u, 100u, 64u) == nullptr);
  CHECK(held.heaps.open(held.device, store, 1u, 0u, store.size() + 1u) == nullptr);
  CHECK(held.heaps.open(held.device, store, 1u, store.size() + D3d12BlockHeaps::window_alignment, 1u) == nullptr);
  CHECK_EQ(held.sink.lines(), 4u);
  CHECK_EQ(held.heaps.count(), 0u);
  CHECK(held.heaps.invariants_hold()); }

TEST(d3d12_block_heaps, memory_the_host_cannot_open_a_heap_over_leaves_no_block_behind)
{ Heaps held;
  SourceMemory &store = *held.rig.store(1u);
  const std::uint32_t before = store.references();
  CHECK(held.heaps.open(held.device, store, 1u, 0u, 64u) == nullptr);
  CHECK_EQ(held.sink.lines(), 1u);
  CHECK_EQ(held.heaps.count(), 0u);
  CHECK_EQ(store.references(), before);
  std::uint64_t base = 0u, size = 0u;
  CHECK(!held.heaps.window(1u, base, size));
  held.heaps.abandon(store);
  held.heaps.destroy_all();
  CHECK_EQ(held.heaps.count(), 0u);
  CHECK(held.heaps.invariants_hold()); }

TEST(d3d12_block_heap, a_leaked_block_is_not_found_by_its_memory_not_reclaimed_and_not_destroyed_with_the_rest)
{ SliceStoreRig rig;
  SourceMemory &store = *rig.store(1u);
  const std::uint32_t before = store.references();
  {
      D3d12BlockHeap block(store);
      block.took_heap();
      block.leak();
      CHECK(block.leaked());
      CHECK(block.abandoned());
      CHECK(!block.holds(store));
      CHECK(!block.done(~std::uint64_t{0}));
      CHECK(block.unreferenced());
  }
  CHECK_EQ(store.references(), before); }
