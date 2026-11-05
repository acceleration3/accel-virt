#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/wire/guest_bytes.hpp"

#include <cstdint>

using av::GuestBytes;
using av::Status;

namespace {

constexpr std::uint64_t slot = GuestBytes::slot_bytes;
constexpr std::uint64_t mib = std::uint64_t{1} << 20u;

std::uint8_t *fake(std::uintptr_t address)
{
    return reinterpret_cast<std::uint8_t *>(address);
}

std::uint8_t *const first_memory = fake(0x10000000u);
std::uint8_t *const second_memory = fake(0x20000000u);
std::uint8_t *const third_memory = fake(0x30000000u);

}

TEST(guest_bytes_bind, an_empty_table_resolves_nothing_and_holds_its_invariants)
{ GuestBytes bytes;
  std::uint8_t *out = fake(1u);
  CHECK_STATUS(bytes.resolve(0u, 0u, out), Status::not_found);
  CHECK(out == nullptr);
  CHECK(bytes.at(0u, 1u) == nullptr);
  CHECK_EQ(bytes.segments(), 0u);
  CHECK(!bytes.holds(first_memory));
  CHECK(bytes.invariants_hold()); }

TEST(guest_bytes_bind, a_segment_resolves_every_range_inside_it_by_slot)
{ GuestBytes bytes;
  CHECK_STATUS(bytes.bind(0u, first_memory, 16u * mib), Status::ok);
  CHECK(bytes.invariants_hold());
  CHECK_STATUS(bytes.bind(slot, second_memory, 4096u), Status::ok);
  CHECK(bytes.invariants_hold());
  CHECK_EQ(bytes.segments(), 2u);
  CHECK(bytes.at(0u, 16u * mib) == first_memory);
  CHECK(bytes.at(100u, 50u) == first_memory + 100u);
  CHECK(bytes.at(slot, 4096u) == second_memory);
  CHECK(bytes.at(slot + 4000u, 96u) == second_memory + 4000u);
  CHECK_EQ(bytes.segment_bytes(0u), 16u * mib);
  CHECK_EQ(bytes.segment_bytes(1u), 4096u);
  CHECK_EQ(bytes.segment_bytes(2u), 0u); }

TEST(guest_bytes_resolve, a_range_must_lie_inside_one_backed_segment)
{ GuestBytes bytes;
  CHECK_STATUS(bytes.bind(0u, first_memory, 4096u), Status::ok);
  CHECK_STATUS(bytes.bind(slot, second_memory, 4096u), Status::ok);
  std::uint8_t *out = nullptr;
  CHECK_STATUS(bytes.resolve(4090u, 7u, out), Status::invalid_parameter);
  CHECK(out == nullptr);
  CHECK_STATUS(bytes.resolve(4090u, 6u, out), Status::ok);
  CHECK_STATUS(bytes.resolve(slot - 8u, 16u, out), Status::invalid_parameter);
  CHECK_STATUS(bytes.resolve(4096u, 1u, out), Status::invalid_parameter);
  CHECK_STATUS(bytes.resolve(2u * slot, 1u, out), Status::not_found);
  CHECK_STATUS(bytes.resolve(slot * GuestBytes::slot_count, 1u, out), Status::invalid_parameter);
  CHECK_STATUS(bytes.resolve(~std::uint64_t{0}, 1u, out), Status::invalid_parameter); }

TEST(guest_bytes_resolve, a_zero_length_range_resolves_up_to_the_end_of_its_segment)
{ GuestBytes bytes;
  CHECK_STATUS(bytes.bind(0u, first_memory, 4096u), Status::ok);
  std::uint8_t *out = nullptr;
  CHECK_STATUS(bytes.resolve(4096u, 0u, out), Status::ok);
  CHECK(out == first_memory + 4096u);
  CHECK_STATUS(bytes.resolve(4097u, 0u, out), Status::invalid_parameter); }

TEST(guest_bytes_resolve, offset_and_length_that_overflow_are_refused)
{ GuestBytes bytes;
  CHECK_STATUS(bytes.bind(0u, first_memory, 4096u), Status::ok);
  std::uint8_t *out = nullptr;
  CHECK_STATUS(bytes.resolve(8u, ~std::uint64_t{0}, out), Status::invalid_parameter);
  CHECK_STATUS(bytes.resolve(~std::uint64_t{0} - 3u, 8u, out), Status::invalid_parameter);
  CHECK_STATUS(bytes.resolve(0u, 4097u, out), Status::invalid_parameter); }

TEST(guest_bytes_bind, a_segment_longer_than_a_slot_owns_the_slots_it_covers)
{ GuestBytes bytes;
  CHECK_STATUS(bytes.bind(slot, first_memory, slot + 5u * mib), Status::ok);
  CHECK(bytes.invariants_hold());
  CHECK_EQ(bytes.segments(), 1u);
  CHECK(bytes.at(slot + slot, 5u * mib) == first_memory + slot);
  CHECK(bytes.at(slot, slot + 5u * mib) == first_memory);
  CHECK(bytes.at(slot + slot + 5u * mib, 1u) == nullptr);
  CHECK_EQ(bytes.base_at(2u * slot + 17u), slot);
  CHECK_STATUS(bytes.bind(2u * slot, second_memory, 4096u), Status::conflict);
  CHECK_STATUS(bytes.bind(3u * slot, second_memory, 4096u), Status::ok);
  CHECK(bytes.invariants_hold());
  CHECK_STATUS(bytes.unbind(first_memory), Status::ok);
  CHECK(bytes.invariants_hold());
  CHECK(bytes.at(slot, 1u) == nullptr);
  CHECK(bytes.at(2u * slot, 1u) == nullptr);
  CHECK(bytes.at(3u * slot, 4096u) == second_memory); }

TEST(guest_bytes_bind, hostile_bindings_are_refused_and_change_nothing)
{ GuestBytes bytes;
  CHECK_STATUS(bytes.bind(0u, first_memory, 4096u), Status::ok);
  CHECK_STATUS(bytes.bind(slot, nullptr, 4096u), Status::invalid_parameter);
  CHECK_STATUS(bytes.bind(slot, second_memory, 0u), Status::invalid_parameter);
  CHECK_STATUS(bytes.bind(slot + 4096u, second_memory, 4096u), Status::invalid_parameter);
  CHECK_STATUS(bytes.bind(slot * GuestBytes::slot_count, second_memory, 4096u), Status::invalid_parameter);
  CHECK_STATUS(bytes.bind(slot * (GuestBytes::slot_count - 1u), second_memory, slot + 1u), Status::invalid_parameter);
  CHECK_STATUS(bytes.bind(slot * (GuestBytes::slot_count - 1u), second_memory, ~std::uint64_t{0}),
               Status::invalid_parameter);
  CHECK_STATUS(bytes.bind(0u, second_memory, 4096u), Status::conflict);
  CHECK_STATUS(bytes.bind(slot, first_memory, 4096u), Status::conflict);
  CHECK_EQ(bytes.segments(), 1u);
  CHECK(bytes.invariants_hold()); }

TEST(guest_bytes_bind, the_last_slot_takes_a_whole_slot_and_nothing_more)
{ GuestBytes bytes;
  const std::uint64_t last = slot * (GuestBytes::slot_count - 1u);
  CHECK_STATUS(bytes.bind(last, first_memory, slot), Status::ok);
  CHECK(bytes.at(last, slot) == first_memory);
  CHECK(bytes.invariants_hold()); }

TEST(guest_bytes_unbind, only_a_bound_segment_is_released_and_by_its_memory)
{ GuestBytes bytes;
  CHECK_STATUS(bytes.bind(0u, first_memory, 4096u), Status::ok);
  CHECK_STATUS(bytes.bind(slot, second_memory, 4096u), Status::ok);
  CHECK_STATUS(bytes.unbind(third_memory), Status::not_found);
  CHECK_STATUS(bytes.unbind(nullptr), Status::not_found);
  CHECK_STATUS(bytes.unbind(first_memory), Status::ok);
  CHECK(bytes.invariants_hold());
  CHECK_STATUS(bytes.unbind(first_memory), Status::not_found);
  CHECK(bytes.at(0u, 1u) == nullptr);
  CHECK(bytes.at(slot, 1u) == second_memory);
  CHECK_EQ(bytes.segments(), 1u);
  CHECK_STATUS(bytes.bind(0u, third_memory, 8192u), Status::ok);
  CHECK(bytes.at(0u, 8192u) == third_memory);
  CHECK(bytes.invariants_hold()); }

TEST(guest_bytes_holds, a_pointer_is_held_only_inside_a_segment)
{ GuestBytes bytes;
  CHECK_STATUS(bytes.bind(0u, first_memory, 4096u), Status::ok);
  CHECK_STATUS(bytes.bind(slot, second_memory, 100u), Status::ok);
  CHECK(bytes.holds(first_memory));
  CHECK(bytes.holds(first_memory + 4095u));
  CHECK(!bytes.holds(first_memory + 4096u));
  CHECK(!bytes.holds(first_memory - 1u));
  CHECK(bytes.holds(second_memory + 99u));
  CHECK(!bytes.holds(second_memory + 100u)); }

TEST(guest_bytes_base, the_base_of_a_memory_or_an_offset_is_the_slot_it_was_bound_at)
{ GuestBytes bytes;
  CHECK_STATUS(bytes.bind(3u * slot, first_memory, 4096u), Status::ok);
  CHECK_EQ(bytes.base_of(first_memory), 3u * slot);
  CHECK_EQ(bytes.base_at(3u * slot + 4000u), 3u * slot);
  CHECK(bytes.base_of(second_memory) == GuestBytes::no_base);
  CHECK(bytes.base_of(nullptr) == GuestBytes::no_base);
  CHECK(bytes.base_at(2u * slot) == GuestBytes::no_base);
  CHECK(bytes.base_at(slot * GuestBytes::slot_count) == GuestBytes::no_base); }

TEST(guest_bytes_segment, a_slot_outside_the_table_has_no_bytes)
{ GuestBytes bytes;
  CHECK_STATUS(bytes.bind(0u, first_memory, 4096u), Status::ok);
  CHECK_EQ(bytes.segment_bytes(0u), 4096u);
  CHECK_EQ(bytes.segment_bytes(GuestBytes::slot_count), 0u);
  CHECK_EQ(bytes.segment_bytes(0xFFFFFFFFu), 0u); }

TEST(guest_bytes_table, every_slot_can_be_bound_and_released_in_any_order)
{ GuestBytes bytes;
  std::uint8_t *memories[GuestBytes::slot_count];
  for (std::uint32_t at = 0u; at < GuestBytes::slot_count; at++) {
      memories[at] = fake(0x40000000u + at * 0x10000u);
      CHECK_STATUS(bytes.bind(std::uint64_t{at} * slot, memories[at], 4096u), Status::ok);
      CHECK(bytes.invariants_hold());
  }
  CHECK_EQ(bytes.segments(), GuestBytes::slot_count);
  for (std::uint32_t at = GuestBytes::slot_count; at-- > 0u;) {
      CHECK_STATUS(bytes.unbind(memories[(at * 7u) % GuestBytes::slot_count]), Status::ok);
      CHECK(bytes.invariants_hold());
  }
  CHECK_EQ(bytes.segments(), 0u); }
