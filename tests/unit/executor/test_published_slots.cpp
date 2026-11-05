#include "executor/published_slots.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/published_slot_layout.hpp"
#include "proto/wire/guest_bytes.hpp"

#include <cstdint>
#include <cstring>

using av::GuestBytes;
using av::PublishedSlotLayout;
using av::PublishedSlots;
using av::Status;

namespace {

alignas(4096) std::uint8_t control[PublishedSlotLayout::control_bytes];
alignas(4096) std::uint8_t ring[8192];

class Bound {
public:
    Bound()
    {
        std::memset(control, 0, sizeof control);
        CHECK_STATUS(bytes.bind(0u, control, sizeof control), Status::ok);
        CHECK_STATUS(bytes.bind(GuestBytes::slot_bytes, ring, sizeof ring), Status::ok);
    }

    GuestBytes bytes;
};

std::uint64_t word_at(std::uint64_t offset)
{
    std::uint64_t value = 0u;
    std::memcpy(&value, control + offset, sizeof value);
    return value;
}

}

TEST(published_slots_bind, slots_inside_the_control_segment_below_the_error_word_are_bound)
{ Bound       held;
  PublishedSlots slots(held.bytes);
  CHECK(!slots.bound());
  const std::uint64_t at = PublishedSlotLayout(sizeof control).slots_at(16u);
  CHECK(slots.bind(at, 16u));
  CHECK(slots.bound());
  CHECK(slots.invariants_hold()); }

TEST(published_slots_bind, slots_that_reach_the_error_word_the_ring_or_nothing_are_refused)
{ Bound       held;
  PublishedSlots slots(held.bytes);
  CHECK(!slots.bind(0u, 0u));
  CHECK(!slots.bind(1u, 1u));
  CHECK(!slots.bind(0u, 512u));
  CHECK(!slots.bind(sizeof control, 1u));
  CHECK(!slots.bind(GuestBytes::slot_bytes, 1u));
  CHECK(!slots.bound());
  CHECK(slots.invariants_hold()); }

TEST(published_slots_bind, a_failed_rebind_drops_the_old_binding)
{ Bound       held;
  PublishedSlots slots(held.bytes);
  CHECK(slots.bind(0u, 4u));
  CHECK(!slots.bind(1u, 4u));
  CHECK(!slots.bound()); }

TEST(published_slots_bind, with_no_control_segment_nothing_binds)
{ GuestBytes     bytes;
  PublishedSlots slots(bytes);
  CHECK(!slots.bind(0u, 1u));
  CHECK(!slots.publish_error_word(5u)); }

TEST(published_slots_write, a_payload_lands_in_its_slot_with_its_sequence_published_last)
{ Bound       held;
  PublishedSlots slots(held.bytes);
  CHECK(slots.bind(0u, 4u));
  const std::uint8_t payload[3] = {7u, 8u, 9u};
  CHECK(slots.write(1u, 5u, payload, sizeof payload));
  const std::uint64_t at = PublishedSlotLayout::slot_bytes;
  CHECK_EQ(word_at(at + PublishedSlotLayout::sequence_at), 5u);
  CHECK_EQ(control[at + PublishedSlotLayout::data_at + 2u], 9u);
  std::uint32_t size = 0u;
  std::memcpy(&size, control + at + PublishedSlotLayout::size_at, sizeof size);
  CHECK_EQ(size, 3u);
  CHECK(slots.write(5u, 6u, payload, 0u));
  CHECK_EQ(word_at(at + PublishedSlotLayout::sequence_at), 6u); }

TEST(published_slots_write, an_unbound_oversized_or_unwritten_sequence_write_is_refused_untouched)
{ Bound       held;
  PublishedSlots slots(held.bytes);
  const std::uint8_t payload[PublishedSlotLayout::data_bytes + 1u] = {};
  CHECK(!slots.write(0u, 1u, payload, 1u));
  CHECK(slots.bind(0u, 2u));
  CHECK(!slots.write(0u, 1u, payload, PublishedSlotLayout::data_bytes + 1u));
  CHECK(!slots.write(0u, PublishedSlotLayout::unwritten, payload, 1u));
  CHECK_EQ(word_at(0u), 0u);
  CHECK(slots.write(0u, 1u, payload, PublishedSlotLayout::data_bytes)); }

TEST(published_slots_write, a_slot_whose_segment_went_away_is_refused)
{ Bound       held;
  PublishedSlots slots(held.bytes);
  CHECK(slots.bind(0u, 2u));
  CHECK_STATUS(held.bytes.unbind(control), Status::ok);
  const std::uint8_t payload[1] = {};
  CHECK(!slots.write(0u, 1u, payload, 1u));
  CHECK(!slots.publish_error_word(1u)); }

TEST(published_slots_publish_error_word, the_value_lands_in_the_last_word_of_the_control_segment)
{ Bound       held;
  PublishedSlots slots(held.bytes);
  CHECK(slots.publish_error_word(0x8000000000000011ull));
  CHECK_EQ(word_at(sizeof control - 8u), 0x8000000000000011ull);
  CHECK(slots.publish_error_word(0u));
  CHECK_EQ(word_at(sizeof control - 8u), 0u); }

TEST(published_slots_unbind, forgets_the_slots_and_keeps_the_error_word_reachable)
{ Bound       held;
  PublishedSlots slots(held.bytes);
  CHECK(slots.bind(0u, 2u));
  slots.unbind();
  CHECK(!slots.bound());
  CHECK(slots.invariants_hold());
  CHECK(slots.publish_error_word(3u)); }
