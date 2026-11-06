#include "guest/transport/published_slot_reader.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/published_slot_layout.hpp"

#include <cstdint>
#include <cstring>

using av::PublishedSlotLayout;
using av::PublishedSlotReader;

namespace {

constexpr std::uint32_t slots = 4u;

void publish(std::uint8_t *memory, std::uint32_t index, std::uint64_t sequence, std::uint8_t fill)
{
    std::uint8_t *const slot = memory + index * PublishedSlotLayout::slot_bytes;
    std::memset(slot + PublishedSlotLayout::data_at, fill, PublishedSlotLayout::data_bytes);
    std::memcpy(slot + PublishedSlotLayout::sequence_at, &sequence, sizeof sequence);
}

}

TEST(published_slot_reader, a_reader_with_no_slots_reads_nothing)
{ PublishedSlotReader reader;
  CHECK(!reader.reading());
  std::uint8_t payload[8] = {};
  CHECK(!reader.read(0u, 1u, payload, sizeof payload));
  CHECK(reader.invariants_hold()); }

TEST(published_slot_reader, taking_slots_clears_them_and_reading_starts)
{ alignas(8) std::uint8_t memory[slots * PublishedSlotLayout::slot_bytes];
  std::memset(memory, 0xAB, sizeof memory);
  PublishedSlotReader reader;
  reader.take(memory, slots);
  CHECK(reader.reading());
  CHECK(reader.invariants_hold());
  for (std::uint8_t byte : memory) CHECK_EQ(byte, 0u);
  reader.take(memory, 0u);
  CHECK(!reader.reading());
  CHECK(reader.invariants_hold());
  reader.take(nullptr, slots);
  CHECK(!reader.reading());
  CHECK(reader.invariants_hold()); }

TEST(published_slot_reader, a_slot_is_read_only_at_the_sequence_published_in_it)
{ alignas(8) std::uint8_t memory[slots * PublishedSlotLayout::slot_bytes];
  PublishedSlotReader reader;
  reader.take(memory, slots);
  publish(memory, 2u, 7u, 0x5Cu);
  std::uint8_t payload[16] = {};
  CHECK(reader.read(2u, 7u, payload, sizeof payload));
  for (std::uint8_t byte : payload) CHECK_EQ(byte, 0x5Cu);
  std::uint8_t other[16] = {};
  CHECK(!reader.read(2u, 8u, other, sizeof other));
  CHECK(!reader.read(2u, 6u, other, sizeof other));
  CHECK(!reader.read(1u, 7u, other, sizeof other));
  for (std::uint8_t byte : other) CHECK_EQ(byte, 0u); }

TEST(published_slot_reader, an_index_wraps_around_the_slots_and_an_unwritten_sequence_or_oversized_read_is_refused)
{ alignas(8) std::uint8_t memory[slots * PublishedSlotLayout::slot_bytes];
  PublishedSlotReader reader;
  reader.take(memory, slots);
  publish(memory, 1u, 9u, 0x11u);
  std::uint8_t payload[PublishedSlotLayout::data_bytes] = {};
  CHECK(reader.read(1u + slots, 9u, payload, sizeof payload));
  CHECK_EQ(payload[0], 0x11u);
  CHECK(!reader.read(1u, PublishedSlotLayout::unwritten, payload, sizeof payload));
  std::uint8_t too_much[PublishedSlotLayout::data_bytes + 1u] = {};
  CHECK(!reader.read(1u, 9u, too_much, sizeof too_much));
  CHECK(reader.read(1u, 9u, nullptr, 0u));
  std::uint8_t untouched[4] = {7u, 7u, 7u, 7u};
  CHECK(reader.read(1u, 9u, untouched, 0u));
  CHECK_EQ(untouched[0], 7u);
  CHECK(reader.read(1u, 9u, nullptr, PublishedSlotLayout::data_bytes)); }
