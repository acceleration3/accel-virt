#include "core/support/status.hpp"
#include "core/trace/event_ring.hpp"
#include "core/trace/event_ring_layout.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::EventRing;
using av::EventRingLayout;
using av::Status;

namespace {

/* docs/trace.md's event ring, written out so a mistake in the code does not
 * move the test with it: a 32-byte header, then per slot a 16-byte header
 * (the count written) and 16-byte events of time, name and thread. */
constexpr std::size_t header = 32u;
constexpr std::size_t slot_header = 16u;
constexpr std::size_t event = 16u;
constexpr std::uint32_t slots = 2u;
constexpr std::uint32_t events = 3u;
constexpr std::size_t slot_bytes = slot_header + events * event;
constexpr std::size_t region_bytes = header + slots * slot_bytes;
constexpr std::uint32_t begin_bit = 0x80000000u;

std::uint64_t u64_at(const std::uint8_t *bytes, std::size_t offset)
{
    std::uint64_t value = 0u;
    std::memcpy(&value, bytes + offset, sizeof value);
    return value;
}

std::uint32_t u32_at(const std::uint8_t *bytes, std::size_t offset)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, bytes + offset, sizeof value);
    return value;
}

std::size_t event_at(std::uint32_t slot, std::uint32_t index)
{
    return header + slot * slot_bytes + slot_header + index * event;
}

struct Ring {
    alignas(8) std::uint8_t bytes[region_bytes + 16u];
    EventRing ring;

    Ring()
    {
        std::memset(bytes, 0, sizeof bytes);
        CHECK_EQ(EventRingLayout(slots, events).bytes(), region_bytes);
        CHECK_STATUS(ring.initialize(bytes, region_bytes, slots, events), Status::ok);
    }
    std::uint64_t written(std::uint32_t slot) const { return u64_at(bytes, header + slot * slot_bytes); }
    void          stop_at(std::uint64_t time) { std::memcpy(bytes + 24u, &time, sizeof time); }
};

}  // namespace

TEST(event_ring_initialize, writes_the_header_a_reader_checks)
{ Ring ring;
  CHECK(ring.ring.recording());
  CHECK_EQ(u32_at(ring.bytes, 0u), 0x45564B52u);
  CHECK_EQ(u32_at(ring.bytes, 4u), 1u);
  CHECK_EQ(u32_at(ring.bytes, 8u), slots);
  CHECK_EQ(u32_at(ring.bytes, 12u), events);
  CHECK(u64_at(ring.bytes, 16u) == 0u);   /* no clock described yet */
  CHECK(u64_at(ring.bytes, 24u) == 0u); }

TEST(event_ring_initialize, refuses_a_region_that_is_missing_or_too_small_or_a_shape_of_nothing)
{ alignas(8) std::uint8_t bytes[region_bytes] = {};
  EventRing ring;
  CHECK_STATUS(ring.initialize(nullptr, region_bytes, slots, events), Status::invalid_parameter);
  CHECK_STATUS(ring.initialize(bytes, region_bytes - 1u, slots, events), Status::invalid_parameter);
  CHECK_STATUS(ring.initialize(bytes, region_bytes, 0u, events), Status::invalid_parameter);
  CHECK_STATUS(ring.initialize(bytes, region_bytes, slots, 0u), Status::invalid_parameter);
  CHECK(!ring.recording());
  CHECK_EQ(u32_at(bytes, 0u), 0u);   /* nothing written */
  /* Not recording, a ring records and describes nothing. */
  ring.record(0u, 5u, 1u, 1u, true);
  ring.describe_clock(10000000u);
  for (const std::uint8_t byte : bytes) CHECK_EQ(byte, 0u); }

/* A reader needs the stamps' unit to place events on a timeline. */
TEST(event_ring_describe_clock, puts_the_ticks_per_second_in_the_header)
{ Ring ring;
  ring.ring.describe_clock(10000000u);
  CHECK(u64_at(ring.bytes, 16u) == 10000000u);
  ring.ring.describe_clock(1000000000u);
  CHECK(u64_at(ring.bytes, 16u) == 1000000000u);
  CHECK_EQ(u32_at(ring.bytes, 0u), 0x45564B52u); }

TEST(event_ring_record, writes_the_time_name_and_thread_with_the_begin_bit)
{ Ring ring;
  ring.ring.record(1u, 0x1122334455ull, 0xABCDu, 7u, true);
  ring.ring.record(1u, 0x1122334466ull, 0u, 7u, false);
  CHECK(ring.written(1u) == 2u);
  CHECK(ring.written(0u) == 0u);   /* another slot's ring is untouched */
  CHECK(u64_at(ring.bytes, event_at(1u, 0u)) == 0x1122334455ull);
  CHECK_EQ(u32_at(ring.bytes, event_at(1u, 0u) + 8u), 0xABCDu);
  CHECK_EQ(u32_at(ring.bytes, event_at(1u, 0u) + 12u), 7u | begin_bit);
  CHECK(u64_at(ring.bytes, event_at(1u, 1u)) == 0x1122334466ull);
  CHECK_EQ(u32_at(ring.bytes, event_at(1u, 1u) + 12u), 7u);
  /* A thread number cannot forge the begin bit. */
  ring.ring.record(0u, 1u, 1u, 0xFFFFFFFFu, false);
  CHECK_EQ(u32_at(ring.bytes, event_at(0u, 0u) + 12u), 0x7FFFFFFFu); }

/* The newest events_per_slot are kept: the fourth overwrites the first. */
TEST(event_ring_record, overwrites_the_oldest_once_full)
{ Ring ring;
  for (std::uint64_t time = 1u; time <= 4u; time++) ring.ring.record(0u, time, 0u, 1u, true);
  CHECK(ring.written(0u) == 4u);
  CHECK(u64_at(ring.bytes, event_at(0u, 0u)) == 4u);
  CHECK(u64_at(ring.bytes, event_at(0u, 1u)) == 2u);
  CHECK(u64_at(ring.bytes, event_at(0u, 2u)) == 3u);
  /* Nothing past the last slot's last event was written. */
  CHECK(u64_at(ring.bytes, region_bytes) == 0u); }

TEST(event_ring_record, a_slot_past_the_ring_is_ignored)
{ Ring ring;
  ring.ring.record(slots, 9u, 1u, 1u, true);
  CHECK(ring.written(0u) == 0u);
  CHECK(ring.written(1u) == 0u);
  for (std::size_t offset = region_bytes; offset < sizeof ring.bytes; offset++) CHECK_EQ(ring.bytes[offset], 0u); }

/* A reader stops every ring at one instant so a capture holds still: nothing
 * at or after the stop time is recorded, anything before it is. */
TEST(event_ring_record, stops_at_the_time_a_reader_set)
{ Ring ring;
  ring.stop_at(100u);
  ring.ring.record(0u, 99u, 1u, 1u, true);
  ring.ring.record(0u, 100u, 1u, 1u, true);
  ring.ring.record(0u, 101u, 1u, 1u, true);
  CHECK(ring.written(0u) == 1u);
  CHECK(u64_at(ring.bytes, event_at(0u, 0u)) == 99u);
  ring.stop_at(0u);   /* recording again */
  ring.ring.record(0u, 200u, 1u, 1u, true);
  CHECK(ring.written(0u) == 2u); }
