#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/d3d/command_recording.hpp"

#include <cstdint>

using av::MockPlatform;
using av::Status;
using av::d3d::CommandRecording;

namespace {

constexpr std::size_t first = 64u;
constexpr std::size_t most = 200u;

void fill(std::uint8_t *bytes, std::size_t count)
{
    for (std::size_t index = 0u; index < count; index++) bytes[index] = static_cast<std::uint8_t>(index + 1u);
}

bool filled(const std::uint8_t *bytes, std::size_t count)
{
    for (std::size_t index = 0u; index < count; index++)
        if (bytes[index] != static_cast<std::uint8_t>(index + 1u)) return false;
    return true;
}

}

TEST(command_recording, the_first_block_is_made_once_and_released_with_the_recording)
{ MockPlatform platform;
  {
      CommandRecording recording(platform, first, most);
      CHECK(recording.bytes() == nullptr);
      CHECK(!recording.hold(0u, 1u));
      CHECK_STATUS(recording.initialize(), Status::ok);
      CHECK(recording.bytes() != nullptr);
      CHECK_EQ(recording.capacity(), first);
      CHECK_EQ(platform.live_allocations(), 1u);
      CHECK_STATUS(recording.initialize(), Status::invalid_state);
      CHECK_EQ(platform.live_allocations(), 1u);
  }
  CHECK_EQ(platform.live_allocations(), 0u);

  CommandRecording empty(platform, 0u, most);
  CHECK_STATUS(empty.initialize(), Status::invalid_parameter);
  platform.fail_allocation_at(1u);
  CommandRecording refused(platform, first, most);
  CHECK_STATUS(refused.initialize(), Status::no_memory);
  CHECK(refused.bytes() == nullptr);
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(command_recording, what_fits_keeps_the_block_and_what_does_not_doubles_it)
{ MockPlatform platform;
  CommandRecording recording(platform, first, most);
  CHECK_STATUS(recording.initialize(), Status::ok);
  std::uint8_t *const block = recording.bytes();
  CHECK(recording.hold(first, 0u));
  CHECK(recording.hold(first - 4u, 4u));
  CHECK(recording.bytes() == block);
  CHECK_EQ(recording.capacity(), first);

  fill(recording.bytes(), first - 4u);
  CHECK(recording.hold(first - 4u, 5u));
  CHECK_EQ(recording.capacity(), 2u * first);
  CHECK(filled(recording.bytes(), first - 4u));
  CHECK_EQ(platform.live_allocations(), 1u);

  CHECK(recording.hold(40u, most - 40u));
  CHECK_EQ(recording.capacity(), most);
  CHECK(filled(recording.bytes(), 40u));
  CHECK_EQ(platform.live_allocations(), 1u); }

TEST(command_recording, a_block_that_cannot_grow_is_left_as_it_was)
{ MockPlatform platform;
  CommandRecording recording(platform, first, most);
  CHECK_STATUS(recording.initialize(), Status::ok);
  fill(recording.bytes(), first);
  std::uint8_t *const block = recording.bytes();

  CHECK(!recording.hold(first + 1u, 0u));
  CHECK(!recording.hold(first, most - first + 1u));
  CHECK(!recording.hold(0u, most + 1u));
  platform.fail_allocation_at(1u);
  CHECK(!recording.hold(first, 1u));
  CHECK(recording.bytes() == block);
  CHECK_EQ(recording.capacity(), first);
  CHECK(filled(recording.bytes(), first));

  CHECK(recording.hold(first, most - first));
  CHECK_EQ(recording.capacity(), most);
  CHECK(filled(recording.bytes(), first)); }

TEST(command_recording, a_largest_size_below_the_first_is_the_first)
{ MockPlatform platform;
  CommandRecording recording(platform, first, first / 2u);
  CHECK_STATUS(recording.initialize(), Status::ok);
  CHECK(recording.hold(0u, first));
  CHECK(!recording.hold(0u, first + 1u));
  CHECK_EQ(recording.capacity(), first); }

TEST(command_recording, growing_an_empty_recording_copies_nothing_and_still_doubles)
{ MockPlatform platform;
  CommandRecording recording(platform, first, most);
  CHECK_STATUS(recording.initialize(), Status::ok);
  CHECK(recording.hold(0u, first + 1u));
  CHECK_EQ(recording.capacity(), 2u * first);
  CHECK_EQ(platform.live_allocations(), 1u); }
