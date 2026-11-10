#include "executor/d3d/d3d9_surface_holds.hpp"
#include "harness/executor/counted_object.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::CountedObject;
using av::D3d9SurfaceHolds;
using av::MockPlatform;
using av::Status;

TEST(d3d9_surface_holds, a_held_surface_stays_alive_for_the_caller_who_released_it_until_its_copy_is_reached)
{ MockPlatform platform;
  CountedObject source;
  D3d9SurfaceHolds holds(platform);
  CHECK_STATUS(holds.hold(&source, 1u), Status::ok);
  CHECK_EQ(source.references(), 2u);
  CHECK_EQ(holds.pending(), 1u);
  CHECK_EQ(holds.oldest(), 1u);
  source.Release();
  CHECK_EQ(source.references(), 1u);
  holds.release_oldest();
  CHECK_EQ(source.references(), 0u);
  CHECK_EQ(holds.pending(), 0u);
  CHECK_EQ(holds.oldest(), 0u);
  CHECK(holds.invariants_hold()); }

TEST(d3d9_surface_holds, surfaces_are_released_oldest_first_and_only_the_one_asked_for)
{ MockPlatform platform;
  CountedObject first, second, third;
  D3d9SurfaceHolds holds(platform);
  CHECK_STATUS(holds.hold(&first, 3u), Status::ok);
  CHECK_STATUS(holds.hold(&second, 5u), Status::ok);
  CHECK_STATUS(holds.hold(&third, 8u), Status::ok);
  CHECK_EQ(holds.pending(), 3u);
  CHECK_EQ(holds.oldest(), 3u);
  holds.release_oldest();
  CHECK_EQ(first.references(), 1u);
  CHECK_EQ(second.references(), 2u);
  CHECK_EQ(third.references(), 2u);
  CHECK_EQ(holds.oldest(), 5u);
  CHECK(holds.invariants_hold());
  holds.release_oldest();
  CHECK_EQ(holds.oldest(), 8u);
  CHECK_EQ(holds.pending(), 1u);
  CHECK(holds.invariants_hold()); }

TEST(d3d9_surface_holds, values_must_rise_and_a_surface_is_never_held_for_value_zero)
{ MockPlatform platform;
  CountedObject source;
  D3d9SurfaceHolds holds(platform);
  CHECK_STATUS(holds.hold(&source, 0u), Status::invalid_parameter);
  CHECK_STATUS(holds.hold(nullptr, 1u), Status::invalid_parameter);
  CHECK_STATUS(holds.hold(&source, 4u), Status::ok);
  CHECK_STATUS(holds.hold(&source, 4u), Status::invalid_parameter);
  CHECK_STATUS(holds.hold(&source, 3u), Status::invalid_parameter);
  CHECK_EQ(source.references(), 2u);
  CHECK_EQ(holds.pending(), 1u); }

TEST(d3d9_surface_holds, a_hold_there_is_no_memory_for_takes_no_reference)
{ MockPlatform platform;
  CountedObject source;
  D3d9SurfaceHolds holds(platform);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(holds.hold(&source, 1u), Status::no_memory);
  CHECK_EQ(source.references(), 1u);
  CHECK_EQ(holds.pending(), 0u);
  CHECK(holds.invariants_hold()); }

TEST(d3d9_surface_holds, releasing_all_lets_go_of_every_surface_and_destruction_does_the_same)
{ MockPlatform platform;
  CountedObject first, second;
  {
    D3d9SurfaceHolds holds(platform);
    CHECK_STATUS(holds.hold(&first, 1u), Status::ok);
    CHECK_STATUS(holds.hold(&second, 2u), Status::ok);
    holds.release_all();
    CHECK_EQ(first.references(), 1u);
    CHECK_EQ(second.references(), 1u);
    CHECK_EQ(holds.pending(), 0u);
    CHECK_STATUS(holds.hold(&first, 3u), Status::ok);
    CHECK_EQ(first.references(), 2u);
  }
  CHECK_EQ(first.references(), 1u); }

TEST(d3d9_surface_holds, a_released_queue_takes_values_that_continue_the_sequence)
{ MockPlatform platform;
  CountedObject source;
  D3d9SurfaceHolds holds(platform);
  CHECK_STATUS(holds.hold(&source, 1u), Status::ok);
  holds.release_oldest();
  CHECK_STATUS(holds.hold(&source, 2u), Status::ok);
  CHECK_EQ(holds.oldest(), 2u);
  holds.release_all();
  CHECK(holds.invariants_hold()); }
