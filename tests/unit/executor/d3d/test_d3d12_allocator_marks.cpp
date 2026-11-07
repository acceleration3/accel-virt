#include "executor/d3d/d3d12_allocator_marks.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::D3d12AllocatorMarks;

TEST(d3d12_allocator_marks, an_allocator_is_as_busy_as_the_latest_list_recorded_from_it_that_ran)
{ av::MockPlatform platform;
  D3d12AllocatorMarks marks(platform);
  CHECK_STATUS(marks.bind(1u, 10u), av::Status::ok);
  CHECK_STATUS(marks.bind(2u, 10u), av::Status::ok);
  CHECK_STATUS(marks.bind(3u, 11u), av::Status::ok);
  CHECK_EQ(marks.mark_of(10u), 0u);
  CHECK_STATUS(marks.submitted(1u, 5u), av::Status::ok);
  CHECK_STATUS(marks.submitted(2u, 3u), av::Status::ok);
  CHECK_EQ(marks.mark_of(10u), 5u);
  CHECK_STATUS(marks.submitted(2u, 9u), av::Status::ok);
  CHECK_EQ(marks.mark_of(10u), 9u);
  CHECK_EQ(marks.mark_of(11u), 0u);
  CHECK(marks.invariants_hold()); }

TEST(d3d12_allocator_marks, a_list_reset_with_another_allocator_marks_that_one)
{ av::MockPlatform platform;
  D3d12AllocatorMarks marks(platform);
  CHECK_STATUS(marks.bind(1u, 10u), av::Status::ok);
  CHECK_STATUS(marks.bind(1u, 11u), av::Status::ok);
  CHECK_EQ(marks.lists(), 1u);
  CHECK_STATUS(marks.submitted(1u, 4u), av::Status::ok);
  CHECK_EQ(marks.mark_of(10u), 0u);
  CHECK_EQ(marks.mark_of(11u), 4u); }

TEST(d3d12_allocator_marks, a_list_no_allocator_is_bound_to_marks_nothing)
{ av::MockPlatform platform;
  D3d12AllocatorMarks marks(platform);
  CHECK_STATUS(marks.submitted(7u, 4u), av::Status::ok);
  CHECK_EQ(marks.mark_of(7u), 0u); }

TEST(d3d12_allocator_marks, forgetting_a_list_or_an_allocator_forgets_what_hangs_on_it)
{ av::MockPlatform platform;
  D3d12AllocatorMarks marks(platform);
  CHECK_STATUS(marks.bind(1u, 10u), av::Status::ok);
  CHECK_STATUS(marks.bind(2u, 10u), av::Status::ok);
  CHECK_STATUS(marks.submitted(1u, 4u), av::Status::ok);
  marks.forget_list(1u);
  CHECK_EQ(marks.lists(), 1u);
  CHECK_STATUS(marks.submitted(1u, 8u), av::Status::ok);
  CHECK_EQ(marks.mark_of(10u), 4u);
  marks.forget_allocator(10u);
  CHECK_EQ(marks.mark_of(10u), 0u);
  CHECK_EQ(marks.lists(), 0u);
  CHECK_STATUS(marks.bind(3u, 12u), av::Status::ok);
  marks.clear();
  CHECK_EQ(marks.lists(), 0u);
  CHECK(marks.invariants_hold()); }

TEST(d3d12_allocator_marks, what_cannot_be_kept_is_refused)
{ av::MockPlatform platform;
  D3d12AllocatorMarks marks(platform);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(marks.bind(1u, 10u), av::Status::no_memory);
  CHECK_STATUS(marks.bind(1u, 10u), av::Status::ok);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(marks.submitted(1u, 4u), av::Status::no_memory);
  CHECK_EQ(marks.mark_of(10u), 0u); }
