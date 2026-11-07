#include "executor/d3d/d3d12_block_heap.hpp"
#include "executor/d3d/d3d12_object_holds.hpp"
#include "harness/executor/counted_object.hpp"
#include "harness/executor/slice_store_rig.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::CountedObject;
using av::D3d12BlockHeap;
using av::D3d12ObjectHolds;

namespace {

constexpr std::uint32_t list_a = 11u;
constexpr std::uint32_t list_b = 12u;

}

TEST(d3d12_object_holds, a_list_keeps_what_it_records_alive_until_it_is_released)
{ av::MockPlatform platform;
  D3d12ObjectHolds holds(platform);
  CountedObject target;
  CHECK_STATUS(holds.hold(list_a, &target, nullptr), av::Status::ok);
  CHECK_EQ(target.references(), 2u);
  CHECK(holds.holding(list_a, &target));
  CHECK(!holds.holding(list_b, &target));
  CHECK_EQ(holds.lists(), 1u);
  CHECK(holds.invariants_hold());
  CHECK_STATUS(holds.release_list(list_a, 0u), av::Status::ok);
  CHECK_EQ(target.references(), 1u);
  CHECK_EQ(holds.lists(), 0u);
  CHECK(holds.invariants_hold()); }

TEST(d3d12_object_holds, an_object_recorded_twice_is_referenced_once)
{ av::MockPlatform platform;
  D3d12ObjectHolds holds(platform);
  CountedObject target;
  CHECK_STATUS(holds.hold(list_a, &target, nullptr), av::Status::ok);
  CHECK_STATUS(holds.hold(list_a, &target, nullptr), av::Status::ok);
  CHECK_EQ(target.references(), 2u);
  CHECK_STATUS(holds.release_list(list_a, 0u), av::Status::ok);
  CHECK_EQ(target.references(), 1u); }

TEST(d3d12_object_holds, two_lists_each_keep_the_same_object_alive)
{ av::MockPlatform platform;
  D3d12ObjectHolds holds(platform);
  CountedObject target;
  CHECK_STATUS(holds.hold(list_a, &target, nullptr), av::Status::ok);
  CHECK_STATUS(holds.hold(list_b, &target, nullptr), av::Status::ok);
  CHECK_EQ(target.references(), 3u);
  CHECK_STATUS(holds.release_list(list_a, 0u), av::Status::ok);
  CHECK_EQ(target.references(), 2u);
  CHECK(holds.holding(list_b, &target));
  CHECK_STATUS(holds.release_list(list_b, 0u), av::Status::ok);
  CHECK_EQ(target.references(), 1u); }

TEST(d3d12_object_holds, a_list_the_gpu_may_still_run_gives_up_its_objects_only_when_the_gpu_is_past_its_last_submission)
{ av::MockPlatform platform;
  D3d12ObjectHolds holds(platform);
  CountedObject target, other;
  CHECK_STATUS(holds.hold(list_a, &target, nullptr), av::Status::ok);
  CHECK_STATUS(holds.hold(list_a, &other, nullptr), av::Status::ok);
  holds.submitted(list_a, 5u);
  holds.submitted(list_b, 9u);
  CHECK_STATUS(holds.release_list(list_a, 4u), av::Status::ok);
  CHECK_EQ(target.references(), 2u);
  CHECK_EQ(other.references(), 2u);
  CHECK_EQ(holds.retired(), 2u);
  CHECK_EQ(holds.lists(), 0u);
  holds.reap(4u);
  CHECK_EQ(target.references(), 2u);
  CHECK_EQ(holds.retired(), 2u);
  holds.reap(5u);
  CHECK_EQ(target.references(), 1u);
  CHECK_EQ(other.references(), 1u);
  CHECK_EQ(holds.retired(), 0u);
  CHECK(holds.invariants_hold()); }

TEST(d3d12_object_holds, a_list_run_again_is_kept_to_its_latest_submission)
{ av::MockPlatform platform;
  D3d12ObjectHolds holds(platform);
  CountedObject target;
  CHECK_STATUS(holds.hold(list_a, &target, nullptr), av::Status::ok);
  holds.submitted(list_a, 3u);
  holds.submitted(list_a, 8u);
  CHECK_STATUS(holds.release_list(list_a, 7u), av::Status::ok);
  holds.reap(7u);
  CHECK_EQ(target.references(), 2u);
  holds.reap(8u);
  CHECK_EQ(target.references(), 1u); }

TEST(d3d12_object_holds, a_reset_list_starts_again_with_nothing_held_and_no_submission)
{ av::MockPlatform platform;
  D3d12ObjectHolds holds(platform);
  CountedObject old_target, new_target;
  CHECK_STATUS(holds.hold(list_a, &old_target, nullptr), av::Status::ok);
  holds.submitted(list_a, 6u);
  CHECK_STATUS(holds.release_list(list_a, 0u), av::Status::ok);
  CHECK_STATUS(holds.hold(list_a, &new_target, nullptr), av::Status::ok);
  CHECK_STATUS(holds.release_list(list_a, 0u), av::Status::ok);
  CHECK_EQ(new_target.references(), 1u);
  CHECK_EQ(old_target.references(), 2u);
  holds.reap(6u);
  CHECK_EQ(old_target.references(), 1u); }

TEST(d3d12_object_holds, a_destroyed_object_is_released_when_the_gpu_is_past_its_mark)
{ av::MockPlatform platform;
  D3d12ObjectHolds holds(platform);
  CountedObject idle, busy;
  CHECK_STATUS(holds.retire(&idle, 4u, 4u), av::Status::ok);
  CHECK_EQ(idle.references(), 0u);
  CHECK_STATUS(holds.retire(&busy, 5u, 4u), av::Status::ok);
  CHECK_EQ(busy.references(), 1u);
  CHECK_EQ(holds.retired(), 1u);
  holds.reap(4u);
  CHECK_EQ(busy.references(), 1u);
  holds.reap(5u);
  CHECK_EQ(busy.references(), 0u);
  CHECK_EQ(holds.retired(), 0u);
  CHECK(holds.invariants_hold()); }

TEST(d3d12_object_holds, an_object_retired_while_a_list_still_holds_it_outlives_both)
{ av::MockPlatform platform;
  D3d12ObjectHolds holds(platform);
  CountedObject target;
  CHECK_STATUS(holds.hold(list_a, &target, nullptr), av::Status::ok);
  holds.submitted(list_a, 6u);
  CHECK_STATUS(holds.retire(&target, 6u, 2u), av::Status::ok);
  CHECK_EQ(target.references(), 2u);
  CHECK_STATUS(holds.release_list(list_a, 2u), av::Status::ok);
  holds.reap(5u);
  CHECK_EQ(target.references(), 2u);
  holds.reap(6u);
  CHECK_EQ(target.references(), 0u); }

TEST(d3d12_object_holds, nothing_is_retired_that_is_not_there)
{ av::MockPlatform platform;
  D3d12ObjectHolds holds(platform);
  CHECK_STATUS(holds.retire(nullptr, 1u, 0u), av::Status::invalid_parameter);
  CHECK_STATUS(holds.release_list(99u, 0u), av::Status::ok);
  holds.submitted(99u, 3u);
  holds.reap(10u);
  CHECK_EQ(holds.lists(), 0u);
  CHECK(holds.invariants_hold()); }

TEST(d3d12_object_holds, a_hold_that_cannot_be_recorded_takes_no_reference)
{ av::MockPlatform platform;
  D3d12ObjectHolds holds(platform);
  CountedObject target;
  platform.fail_allocation_at(1u);
  CHECK_STATUS(holds.hold(list_a, &target, nullptr), av::Status::no_memory);
  CHECK_EQ(target.references(), 1u);
  CHECK(holds.invariants_hold());
  CHECK_STATUS(holds.hold(list_a, &target, nullptr), av::Status::ok);
  CHECK_EQ(target.references(), 2u); }

TEST(d3d12_object_holds, a_release_that_cannot_be_deferred_keeps_what_it_has_not_moved)
{ av::MockPlatform platform;
  D3d12ObjectHolds holds(platform);
  CountedObject first, second;
  CHECK_STATUS(holds.hold(list_a, &first, nullptr), av::Status::ok);
  CHECK_STATUS(holds.hold(list_a, &second, nullptr), av::Status::ok);
  holds.submitted(list_a, 5u);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(holds.release_list(list_a, 0u), av::Status::no_memory);
  CHECK_EQ(first.references(), 2u);
  CHECK_EQ(second.references(), 2u);
  CHECK(holds.invariants_hold());
  CHECK_STATUS(holds.release_list(list_a, 5u), av::Status::ok);
  CHECK_EQ(first.references(), 1u);
  CHECK_EQ(second.references(), 1u);
  CHECK_EQ(holds.lists(), 0u); }

TEST(d3d12_object_holds, releasing_everything_gives_back_every_reference)
{ av::MockPlatform platform;
  D3d12ObjectHolds holds(platform);
  CountedObject held, retired, listed;
  CHECK_STATUS(holds.hold(list_a, &held, nullptr), av::Status::ok);
  CHECK_STATUS(holds.hold(list_b, &listed, nullptr), av::Status::ok);
  holds.submitted(list_b, 4u);
  CHECK_STATUS(holds.release_list(list_b, 0u), av::Status::ok);
  CHECK_STATUS(holds.retire(&retired, 9u, 0u), av::Status::ok);
  holds.release_all();
  CHECK_EQ(held.references(), 1u);
  CHECK_EQ(listed.references(), 1u);
  CHECK_EQ(retired.references(), 0u);
  CHECK_EQ(holds.lists(), 0u);
  CHECK_EQ(holds.retired(), 0u);
  CHECK(holds.invariants_hold()); }

TEST(d3d12_object_holds, a_resource_in_a_host_memory_window_pins_its_block_for_as_long_as_the_list_holds_it)
{ av::SliceStoreRig rig;
  D3d12BlockHeap block(*rig.store(1u));
  D3d12ObjectHolds holds(rig.platform());
  CountedObject target;
  block.abandon(0u);
  CHECK(block.done(0u));
  block.took_resource();
  CHECK_STATUS(holds.hold(list_a, &target, &block), av::Status::ok);
  CHECK(!block.unreferenced());
  CHECK(!block.done(100u));
  holds.submitted(list_a, 8u);
  CHECK_STATUS(holds.release_list(list_a, 0u), av::Status::ok);
  CHECK(block.unreferenced());
  CHECK(!block.done(7u));
  CHECK(block.done(8u)); }

TEST(d3d12_object_holds, a_repeated_hold_on_a_windowed_resource_gives_back_the_pin_it_took)
{ av::SliceStoreRig rig;
  D3d12BlockHeap block(*rig.store(1u));
  D3d12ObjectHolds holds(rig.platform());
  CountedObject target;
  block.took_resource();
  CHECK_STATUS(holds.hold(list_a, &target, &block), av::Status::ok);
  block.took_resource();
  CHECK_STATUS(holds.hold(list_a, &target, &block), av::Status::ok);
  CHECK(!block.unreferenced());
  CHECK_STATUS(holds.release_list(list_a, 0u), av::Status::ok);
  CHECK(block.unreferenced()); }

TEST(d3d12_object_holds, a_windowed_resource_that_cannot_be_recorded_gives_back_its_pin)
{ av::SliceStoreRig rig;
  D3d12BlockHeap block(*rig.store(1u));
  D3d12ObjectHolds holds(rig.platform());
  CountedObject target;
  block.took_resource();
  rig.platform().fail_allocation_at(1u);
  CHECK_STATUS(holds.hold(list_a, &target, &block), av::Status::no_memory);
  CHECK(block.unreferenced());
  CHECK_EQ(target.references(), 1u); }

TEST(d3d12_object_holds, a_list_that_cannot_be_indexed_is_not_kept)
{ av::MockPlatform platform;
  D3d12ObjectHolds holds(platform);
  CountedObject target;
  platform.fail_allocation_at(2u);
  CHECK_STATUS(holds.hold(list_a, &target, nullptr), av::Status::no_memory);
  CHECK_EQ(holds.lists(), 0u);
  CHECK_EQ(target.references(), 1u);
  CHECK(holds.invariants_hold()); }

TEST(d3d12_object_holds, an_object_that_cannot_be_listed_is_not_referenced)
{ av::MockPlatform platform;
  D3d12ObjectHolds holds(platform);
  CountedObject objects[9];
  for (std::uint32_t index = 0u; index < 8u; index++)
      CHECK_STATUS(holds.hold(list_a, &objects[index], nullptr), av::Status::ok);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(holds.hold(list_a, &objects[8], nullptr), av::Status::no_memory);
  CHECK_EQ(objects[8].references(), 1u);
  CHECK(!holds.holding(list_a, &objects[8]));
  CHECK(holds.invariants_hold());
  CHECK_STATUS(holds.release_list(list_a, 0u), av::Status::ok);
  for (std::uint32_t index = 0u; index < 8u; index++) CHECK_EQ(objects[index].references(), 1u); }

TEST(d3d12_object_holds, retired_objects_leave_in_mark_order_whatever_order_they_came_in)
{ av::MockPlatform platform;
  D3d12ObjectHolds holds(platform);
  CountedObject first, second, third, fourth;
  CHECK_STATUS(holds.retire(&third, 9u, 0u), av::Status::ok);
  CHECK_STATUS(holds.retire(&first, 3u, 0u), av::Status::ok);
  CHECK_STATUS(holds.retire(&fourth, 12u, 0u), av::Status::ok);
  CHECK_STATUS(holds.retire(&second, 5u, 0u), av::Status::ok);
  CHECK(holds.invariants_hold());
  holds.reap(5u);
  CHECK_EQ(first.references(), 0u);
  CHECK_EQ(second.references(), 0u);
  CHECK_EQ(third.references(), 1u);
  CHECK_EQ(holds.retired(), 2u);
  holds.reap(8u);
  CHECK_EQ(third.references(), 1u);
  holds.reap(12u);
  CHECK_EQ(third.references(), 0u);
  CHECK_EQ(fourth.references(), 0u);
  CHECK_EQ(holds.retired(), 0u);
  CHECK(holds.invariants_hold()); }

TEST(d3d12_object_holds, a_long_queue_of_retired_objects_is_reaped_a_run_at_a_time)
{ av::MockPlatform platform;
  D3d12ObjectHolds holds(platform);
  CountedObject objects[100];
  for (std::uint32_t index = 0u; index < 100u; index++)
      CHECK_STATUS(holds.retire(&objects[index], index + 1u, 0u), av::Status::ok);
  holds.reap(60u);
  CHECK_EQ(holds.retired(), 40u);
  for (std::uint32_t index = 0u; index < 100u; index++) CHECK_EQ(objects[index].references(), index < 60u ? 0u : 1u);
  CHECK(holds.invariants_hold());
  CHECK_STATUS(holds.retire(&objects[0], 70u, 0u), av::Status::ok);
  objects[0].AddRef();
  holds.reap(100u);
  CHECK_EQ(holds.retired(), 0u); }

TEST(d3d12_object_holds, a_slot_held_without_a_pin_takes_the_pin_of_a_later_hold_of_the_same_object)
{ av::SliceStoreRig rig;
  D3d12BlockHeap block(*rig.store(1u));
  D3d12ObjectHolds holds(rig.platform());
  CountedObject target;
  CHECK_STATUS(holds.hold(list_a, &target, nullptr), av::Status::ok);
  block.took_resource();
  CHECK_STATUS(holds.hold(list_a, &target, &block), av::Status::ok);
  CHECK_EQ(target.references(), 2u);
  CHECK(!block.unreferenced());
  CHECK_STATUS(holds.release_list(list_a, 0u), av::Status::ok);
  CHECK(block.unreferenced());
  CHECK_EQ(target.references(), 1u); }

TEST(d3d12_object_holds, a_pinned_hold_is_not_replaced_by_a_second_pin)
{ av::SliceStoreRig rig;
  D3d12BlockHeap block(*rig.store(1u));
  D3d12BlockHeap other(*rig.store(2u));
  D3d12ObjectHolds holds(rig.platform());
  CountedObject target;
  block.took_resource();
  CHECK_STATUS(holds.hold(list_a, &target, &block), av::Status::ok);
  other.took_resource();
  CHECK_STATUS(holds.hold(list_a, &target, &other), av::Status::ok);
  CHECK(other.unreferenced());
  CHECK(!block.unreferenced());
  CHECK_STATUS(holds.release_list(list_a, 0u), av::Status::ok);
  CHECK(block.unreferenced()); }

TEST(d3d12_object_holds, an_inherited_hold_takes_its_own_pin_in_every_list_that_holds_the_holder)
{ av::SliceStoreRig rig;
  D3d12BlockHeap block(*rig.store(1u));
  D3d12ObjectHolds holds(rig.platform());
  CountedObject holder, inherited;
  CHECK_STATUS(holds.hold(list_a, &holder, nullptr), av::Status::ok);
  CHECK_STATUS(holds.hold(list_b, &holder, nullptr), av::Status::ok);
  block.took_resource();
  CHECK_STATUS(holds.inherit(&holder, &inherited, &block), av::Status::ok);
  CHECK_STATUS(holds.release_list(list_a, 0u), av::Status::ok);
  CHECK(!block.unreferenced());
  CHECK_STATUS(holds.release_list(list_b, 0u), av::Status::ok);
  block.let_go_resource(0u);
  CHECK(block.unreferenced()); }

TEST(d3d12_object_holds, letting_everything_go_finishes_objects_whose_release_defers_more)
{ av::MockPlatform platform;
  D3d12ObjectHolds holds(platform);
  CountedObject first, second;
  CHECK_STATUS(holds.hold(list_a, &first, nullptr), av::Status::ok);
  CHECK_STATUS(holds.hold(list_b, &second, nullptr), av::Status::ok);
  holds.submitted(list_a, 4u);
  CHECK_STATUS(holds.release_list(list_a, 1u), av::Status::ok);
  CHECK_EQ(holds.retired(), 1u);
  holds.release_all();
  CHECK_EQ(first.references(), 1u);
  CHECK_EQ(second.references(), 1u);
  CHECK_EQ(holds.retired(), 0u);
  CHECK_EQ(holds.lists(), 0u);
  CHECK(holds.invariants_hold()); }

TEST(d3d12_object_holds, forgetting_everything_drops_the_lists_and_retired_work_without_releasing_or_unpinning)
{ av::MockPlatform platform;
  D3d12ObjectHolds holds(platform);
  CountedObject target, other;
  CHECK_STATUS(holds.hold(list_a, &target, nullptr), av::Status::ok);
  CHECK_STATUS(holds.hold(list_b, &other, nullptr), av::Status::ok);
  holds.submitted(list_b, 9u);
  CHECK_STATUS(holds.release_list(list_b, 1u), av::Status::ok);
  CHECK_EQ(holds.retired(), 1u);
  holds.forget_all();
  CHECK_EQ(holds.lists(), 0u);
  CHECK_EQ(holds.retired(), 0u);
  CHECK_EQ(target.references(), 2u);
  CHECK_EQ(other.references(), 2u);
  CHECK(!holds.holding(list_a, &target));
  CHECK_STATUS(holds.release_list(list_a, 0u), av::Status::ok);
  CHECK_EQ(target.references(), 2u);
  CHECK_STATUS(holds.hold(list_a, &other, nullptr), av::Status::ok);
  CHECK_EQ(other.references(), 3u);
  CHECK(holds.invariants_hold()); }
