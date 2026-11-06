#include "core/support/bgra_rows.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct BgraRowsProbe {
    static void corrupt_bytes(BgraRows &rows, std::uint64_t bytes) { rows.bytes_ = bytes; }
    static void corrupt_base(BgraRows &rows, std::uint8_t *base) { rows.base_ = base; }
};

}

using av::BgraRows;
using av::BgraRowsProbe;
using av::Status;

TEST(bgra_rows, copies_rows_of_one_pitch_into_rows_of_another)
{ const std::uint32_t source[2][4] = {{1u, 2u, 3u, 0xEEEEEEEEu}, {4u, 5u, 6u, 0xEEEEEEEEu}};
  std::uint32_t destination[2][5] = {};
  const BgraRows from(reinterpret_cast<const std::uint8_t *>(source), sizeof source, 16u);
  BgraRows       to(reinterpret_cast<std::uint8_t *>(destination), sizeof destination, 20u);
  CHECK(from.invariants_hold());
  CHECK(to.invariants_hold());
  CHECK_STATUS(to.copy_from(from, 3u, 2u, BgraRows::Channels::unchanged), Status::ok);
  CHECK_EQ(destination[0][0], 1u);
  CHECK_EQ(destination[0][2], 3u);
  CHECK_EQ(destination[0][3], 0u);
  CHECK_EQ(destination[1][0], 4u);
  CHECK_EQ(destination[1][2], 6u);
  CHECK(to.invariants_hold()); }

TEST(bgra_rows, swaps_red_and_blue_and_keeps_green_and_alpha)
{ const std::uint32_t source[2] = {0x80112233u, 0xFF0000FFu};
  std::uint32_t destination[2] = {};
  const BgraRows from(reinterpret_cast<const std::uint8_t *>(source), sizeof source, sizeof source);
  BgraRows       to(reinterpret_cast<std::uint8_t *>(destination), sizeof destination, sizeof destination);
  CHECK_STATUS(to.copy_from(from, 2u, 1u, BgraRows::Channels::red_blue_swapped), Status::ok);
  CHECK_EQ(destination[0], 0x80332211u);
  CHECK_EQ(destination[1], 0xFFFF0000u); }

TEST(bgra_rows, refuses_what_either_side_cannot_hold_and_a_read_only_destination)
{ std::uint32_t source[4] = {1u, 2u, 3u, 4u};
  std::uint32_t destination[4] = {};
  const BgraRows from(static_cast<const std::uint8_t *>(static_cast<void *>(source)), sizeof source, 8u);
  BgraRows       to(reinterpret_cast<std::uint8_t *>(destination), sizeof destination, 8u);
  CHECK_STATUS(to.copy_from(from, 3u, 1u, BgraRows::Channels::unchanged), Status::invalid_parameter);
  CHECK_STATUS(to.copy_from(from, 0u, 1u, BgraRows::Channels::unchanged), Status::invalid_parameter);
  CHECK_STATUS(to.copy_from(from, 2u, 0u, BgraRows::Channels::unchanged), Status::invalid_parameter);
  CHECK_STATUS(to.copy_from(from, 2u, 3u, BgraRows::Channels::unchanged), Status::invalid_parameter);
  CHECK_STATUS(to.copy_from(from, 2u, 2u, BgraRows::Channels::unchanged), Status::ok);
  CHECK_EQ(destination[2], 3u);
  BgraRows readonly(static_cast<const std::uint8_t *>(static_cast<void *>(destination)), sizeof destination, 8u);
  CHECK_STATUS(readonly.copy_from(from, 2u, 1u, BgraRows::Channels::unchanged), Status::invalid_parameter);
  BgraRows none(static_cast<std::uint8_t *>(nullptr), 16u, 8u);
  CHECK(none.invariants_hold());
  CHECK_STATUS(none.copy_from(from, 2u, 1u, BgraRows::Channels::unchanged), Status::invalid_parameter);
  CHECK_STATUS(to.copy_from(none, 2u, 1u, BgraRows::Channels::unchanged), Status::invalid_parameter);
  std::uint32_t narrow[4] = {};
  BgraRows      tight(reinterpret_cast<std::uint8_t *>(narrow), sizeof narrow, 4u);
  CHECK_STATUS(tight.copy_from(from, 2u, 1u, BgraRows::Channels::unchanged), Status::invalid_parameter); }

TEST(bgra_rows, a_null_read_only_base_holds_nothing_and_stays_consistent)
{ const BgraRows nothing(static_cast<const std::uint8_t *>(nullptr), 64u, 8u);
  CHECK(nothing.invariants_hold());
  std::uint32_t destination[2] = {};
  BgraRows      to(reinterpret_cast<std::uint8_t *>(destination), sizeof destination, sizeof destination);
  CHECK_STATUS(to.copy_from(nothing, 1u, 1u, BgraRows::Channels::unchanged), Status::invalid_parameter); }

TEST(bgra_rows, bytes_without_a_source_or_a_base_apart_from_it_break_the_invariant)
{ std::uint32_t pixels[2] = {};
  std::uint32_t other[2] = {};
  BgraRows      rows(reinterpret_cast<std::uint8_t *>(pixels), sizeof pixels, 8u);
  CHECK(rows.invariants_hold());
  BgraRowsProbe::corrupt_base(rows, reinterpret_cast<std::uint8_t *>(other));
  CHECK(!rows.invariants_hold());
  BgraRows none(static_cast<std::uint8_t *>(nullptr), 0u, 8u);
  BgraRowsProbe::corrupt_bytes(none, 8u);
  CHECK(!none.invariants_hold()); }
