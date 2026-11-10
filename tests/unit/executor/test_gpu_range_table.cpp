#include "executor/gpu_range_table.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

using av::GpuRangeTable;

namespace {
constexpr std::uint64_t fallback = 0xF000u;
}

TEST(gpu_range_table_fill, lays_out_the_count_the_fallback_and_the_rows_the_patch_shader_reads)
{ const GpuRangeTable table(fallback);
  const GpuRangeTable::Row rows[2] = {{0x1000u, 0x100u, 0xA000u}, {0x4000u, 0x200u, 0xB000u}};
  std::vector<std::uint8_t> bytes(table.bytes_for(2u));
  CHECK(table.fill(bytes.data(), bytes.size(), rows, 2u));
  std::uint32_t count = 0u;
  std::uint64_t kept = 0u, second_host = 0u;
  std::memcpy(&count, bytes.data(), sizeof count);
  std::memcpy(&kept, bytes.data() + 8u, sizeof kept);
  std::memcpy(&second_host, bytes.data() + 16u + 24u + 16u, sizeof second_host);
  CHECK_EQ(count, 2u);
  CHECK(kept == fallback);
  CHECK(second_host == 0xB000u);
  CHECK(bytes.size() == 16u + 2u * 24u); }

TEST(gpu_range_table_fill, refuses_a_short_buffer_unsorted_overlapping_or_empty_rows)
{ const GpuRangeTable table(fallback);
  std::vector<std::uint8_t> bytes(256u);
  const GpuRangeTable::Row ok[1] = {{0x1000u, 0x100u, 0xA000u}};
  CHECK(!table.fill(bytes.data(), 16u, ok, 1u));
  const GpuRangeTable::Row overlapping[2] = {{0x1000u, 0x100u, 1u}, {0x1080u, 0x100u, 2u}};
  CHECK(!table.fill(bytes.data(), bytes.size(), overlapping, 2u));
  const GpuRangeTable::Row unsorted[2] = {{0x2000u, 0x10u, 1u}, {0x1000u, 0x10u, 2u}};
  CHECK(!table.fill(bytes.data(), bytes.size(), unsorted, 2u));
  const GpuRangeTable::Row empty[1] = {{0x1000u, 0u, 1u}};
  CHECK(!table.fill(bytes.data(), bytes.size(), empty, 1u));
  CHECK(table.fill(bytes.data(), bytes.size(), nullptr, 0u)); }

TEST(gpu_range_table_lookup, maps_inside_a_row_and_answers_the_fallback_with_no_room_outside_every_row)
{ const GpuRangeTable table(fallback);
  const GpuRangeTable::Row rows[2] = {{0x1000u, 0x100u, 0xA000u}, {0x4000u, 0x200u, 0xB000u}};
  std::vector<std::uint8_t> bytes(table.bytes_for(2u));
  CHECK(table.fill(bytes.data(), bytes.size(), rows, 2u));
  std::uint64_t host = 0u, room = 0u;
  CHECK(table.lookup(bytes.data(), 0x1010u, host, room));
  CHECK(host == 0xA010u);
  CHECK(room == 0xF0u);
  CHECK(table.lookup(bytes.data(), 0x41FFu, host, room));
  CHECK(host == 0xB1FFu);
  CHECK(room == 1u);
  CHECK(!table.lookup(bytes.data(), 0x1100u, host, room));
  CHECK(host == fallback);
  CHECK(room == 0u);
  CHECK(!table.lookup(bytes.data(), 0x0FFFu, host, room));
  CHECK(!table.lookup(bytes.data(), 0x4200u, host, room));
  CHECK(!table.lookup(bytes.data(), ~std::uint64_t{0}, host, room)); }
