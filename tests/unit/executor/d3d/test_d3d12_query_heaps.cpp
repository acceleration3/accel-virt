#include "executor/d3d/d3d12_query_heaps.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::D3d12QueryHeaps;

namespace {

constexpr std::uint32_t occlusion_heap = 0u, timestamp_heap = 1u, statistics_heap = 2u, stream_heap = 3u,
                        video_heap = 4u, copy_timestamp_heap = 5u;
constexpr std::uint32_t occlusion = 0u, binary = 1u, timestamp = 2u, statistics = 3u, stream_first = 4u,
                        stream_last = 7u, beyond = 8u;

}

TEST(d3d12_query_heaps, each_heap_takes_the_queries_of_its_own_kind)
{ av::MockPlatform platform;
  D3d12QueryHeaps heaps(platform);
  CHECK_STATUS(heaps.remember(1u, occlusion_heap, 8u), av::Status::ok);
  CHECK_STATUS(heaps.remember(2u, timestamp_heap, 8u), av::Status::ok);
  CHECK_STATUS(heaps.remember(3u, statistics_heap, 8u), av::Status::ok);
  CHECK_STATUS(heaps.remember(4u, stream_heap, 8u), av::Status::ok);
  CHECK_STATUS(heaps.remember(5u, copy_timestamp_heap, 8u), av::Status::ok);
  CHECK_STATUS(heaps.remember(6u, video_heap, 8u), av::Status::ok);
  CHECK(heaps.admits(1u, occlusion, 0u, 1u));
  CHECK(heaps.admits(1u, binary, 0u, 1u));
  CHECK(!heaps.admits(1u, timestamp, 0u, 1u));
  CHECK(heaps.admits(2u, timestamp, 0u, 1u));
  CHECK(!heaps.admits(2u, occlusion, 0u, 1u));
  CHECK(heaps.admits(3u, statistics, 0u, 1u));
  CHECK(!heaps.admits(3u, stream_first, 0u, 1u));
  CHECK(heaps.admits(4u, stream_first, 0u, 1u));
  CHECK(heaps.admits(4u, stream_last, 0u, 1u));
  CHECK(!heaps.admits(4u, beyond, 0u, 1u));
  CHECK(heaps.admits(5u, timestamp, 0u, 1u));
  CHECK(!heaps.admits(6u, timestamp, 0u, 1u));
  CHECK_EQ(heaps.known(), 6u);
  CHECK(heaps.invariants_hold()); }

TEST(d3d12_query_heaps, a_range_must_lie_inside_the_heap)
{ av::MockPlatform platform;
  D3d12QueryHeaps heaps(platform);
  CHECK_STATUS(heaps.remember(1u, occlusion_heap, 8u), av::Status::ok);
  CHECK(heaps.admits(1u, occlusion, 0u, 8u));
  CHECK(heaps.admits(1u, occlusion, 8u, 0u));
  CHECK(heaps.admits(1u, occlusion, 7u, 1u));
  CHECK(!heaps.admits(1u, occlusion, 7u, 2u));
  CHECK(!heaps.admits(1u, occlusion, 9u, 0u));
  CHECK(!heaps.admits(1u, occlusion, 0xFFFFFFFFu, 2u));
  CHECK(!heaps.admits(1u, occlusion, 1u, 0xFFFFFFFFu)); }

TEST(d3d12_query_heaps, a_heap_never_made_or_forgotten_admits_nothing)
{ av::MockPlatform platform;
  D3d12QueryHeaps heaps(platform);
  CHECK(!heaps.admits(9u, occlusion, 0u, 1u));
  CHECK_STATUS(heaps.remember(1u, occlusion_heap, 8u), av::Status::ok);
  CHECK_STATUS(heaps.remember(1u, occlusion_heap, 8u), av::Status::conflict);
  heaps.forget(1u);
  CHECK(!heaps.admits(1u, occlusion, 0u, 1u));
  CHECK_STATUS(heaps.remember(2u, occlusion_heap, 8u), av::Status::ok);
  heaps.clear();
  CHECK_EQ(heaps.known(), 0u);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(heaps.remember(3u, occlusion_heap, 8u), av::Status::no_memory); }
