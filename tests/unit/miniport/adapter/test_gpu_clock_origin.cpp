#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "miniport/adapter/gpu_clock_origin.hpp"
#include "proto/escape/clock_origin_record.hpp"

#include <cstdint>

using av::ClockOriginRecord;
using av::GpuClockOrigin;
using av::MockPlatform;

namespace {

constexpr std::uint64_t counter_frequency = 10000000u;
constexpr std::uint64_t gpu_frequency = 1000000000u;

}

TEST(gpu_clock_origin, answers_nothing_until_an_origin_is_taken)
{ MockPlatform platform;
  GpuClockOrigin origin(platform);
  std::uint64_t ticks = 5u, frequency = 6u;
  CHECK(!origin.project(1000u, counter_frequency, ticks, frequency));
  CHECK_EQ(ticks, 5u);
  CHECK_EQ(frequency, 6u); }

TEST(gpu_clock_origin, carries_the_gpu_ticks_forward_at_the_gpu_rate_from_the_counter_it_was_taken_at)
{ MockPlatform platform;
  GpuClockOrigin origin(platform);
  origin.adopt(ClockOriginRecord(2000u, 777000u, gpu_frequency));
  std::uint64_t ticks = 0u, frequency = 0u;
  CHECK(origin.project(2000u, counter_frequency, ticks, frequency));
  CHECK_EQ(ticks, 777000u);
  CHECK_EQ(frequency, gpu_frequency);
  CHECK(origin.project(2000u + counter_frequency, counter_frequency, ticks, frequency));
  CHECK_EQ(ticks, 777000u + gpu_frequency);
  CHECK(origin.project(2003u, counter_frequency, ticks, frequency));
  CHECK_EQ(ticks, 777000u + 300u);
  CHECK(origin.project(2000u + 3u * counter_frequency + 7u, counter_frequency, ticks, frequency));
  CHECK_EQ(ticks, 777000u + 3u * gpu_frequency + 700u); }

TEST(gpu_clock_origin, a_counter_before_the_origin_goes_back_and_stops_at_zero)
{ MockPlatform platform;
  GpuClockOrigin origin(platform);
  origin.adopt(ClockOriginRecord(2000u, 5000u, gpu_frequency));
  std::uint64_t ticks = 0u, frequency = 0u;
  CHECK(origin.project(1990u, counter_frequency, ticks, frequency));
  CHECK_EQ(ticks, 5000u - 1000u);
  CHECK(origin.project(1u, counter_frequency, ticks, frequency));
  CHECK_EQ(ticks, 0u); }

TEST(gpu_clock_origin, a_counter_rate_it_cannot_scale_from_is_not_answered)
{ MockPlatform platform;
  GpuClockOrigin origin(platform);
  origin.adopt(ClockOriginRecord(2000u, 5000u, gpu_frequency));
  std::uint64_t ticks = 9u, frequency = 9u;
  CHECK(!origin.project(2000u, 0u, ticks, frequency));
  CHECK(!origin.project(2000u, ClockOriginRecord::most_frequency + 1u, ticks, frequency));
  CHECK_EQ(ticks, 9u);
  CHECK_EQ(frequency, 9u); }

TEST(gpu_clock_origin, the_latest_origin_replaces_the_one_before)
{ MockPlatform platform;
  GpuClockOrigin origin(platform);
  origin.adopt(ClockOriginRecord(2000u, 5000u, gpu_frequency));
  origin.adopt(ClockOriginRecord(9000u, 100u, 12000000u));
  std::uint64_t ticks = 0u, frequency = 0u;
  CHECK(origin.project(9000u + counter_frequency, counter_frequency, ticks, frequency));
  CHECK_EQ(ticks, 100u + 12000000u);
  CHECK_EQ(frequency, 12000000u); }

TEST(gpu_clock_origin, a_later_origin_replaces_the_drifted_one_and_projects_from_its_own_counter)
{ MockPlatform platform;
  GpuClockOrigin origin(platform);
  origin.adopt(ClockOriginRecord(2000u, 1000000u, gpu_frequency));
  const std::uint64_t later = 2000u + 60u * counter_frequency;
  std::uint64_t ticks = 0u, frequency = 0u;
  CHECK(origin.project(later, counter_frequency, ticks, frequency));
  CHECK_EQ(ticks, 1000000u + 60u * gpu_frequency);
  origin.adopt(ClockOriginRecord(later, 1000000u + 60u * gpu_frequency + 6000u, gpu_frequency));
  CHECK(origin.project(later, counter_frequency, ticks, frequency));
  CHECK_EQ(ticks, 1000000u + 60u * gpu_frequency + 6000u);
  CHECK(origin.project(later + counter_frequency, counter_frequency, ticks, frequency));
  CHECK_EQ(ticks, 1000000u + 61u * gpu_frequency + 6000u); }
