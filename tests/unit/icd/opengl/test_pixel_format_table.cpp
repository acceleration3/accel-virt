/* The ICD's pixel formats as opengl32 reads them through
 * DrvDescribePixelFormat: the count for any index, a descriptor only for an
 * index in range and room for one, every row a composited window format with
 * no palette or bitmap bits, and indices that name the same format in every
 * table. */
#include "harness/framework/test_registry.hpp"
#include "icd/opengl/pixel_format_table.hpp"

#include <climits>
#include <cstdint>
#include <cstring>

using av::glicd::PixelFormatTable;

namespace {

constexpr std::uint32_t rows = 24u;
constexpr std::uint32_t pfd_type_rgba = 0u;
constexpr std::uint32_t pfd_main_plane = 0u;
/* Every PFD_ flag a hardware OpenGL format may not carry here. */
constexpr std::uint32_t forbidden_flags = 0x00000002u /* PFD_STEREO */ | 0x00000008u /* PFD_DRAW_TO_BITMAP */ |
                                         0x00000010u /* PFD_SUPPORT_GDI */ | 0x00000040u /* PFD_GENERIC_FORMAT */ |
                                         0x00000080u /* PFD_NEED_PALETTE */ |
                                         0x00000100u /* PFD_NEED_SYSTEM_PALETTE */ |
                                         0x00001000u /* PFD_GENERIC_ACCELERATED */;

std::uint16_t u16(const std::uint8_t *bytes, std::uint32_t at)
{ return static_cast<std::uint16_t>(bytes[at] | (bytes[at + 1u] << 8u)); }

std::uint32_t u32(const std::uint8_t *bytes, std::uint32_t at)
{ return std::uint32_t{bytes[at]} | (std::uint32_t{bytes[at + 1u]} << 8u) | (std::uint32_t{bytes[at + 2u]} << 16u) |
         (std::uint32_t{bytes[at + 3u]} << 24u); }

}  // namespace

TEST(pixel_format_table, answers_the_count_for_any_index_and_describes_only_rows_in_range)
{ const PixelFormatTable table;
  CHECK(table.invariants_hold());
  CHECK_EQ(table.count(), rows);
  const std::int32_t indices[] = {0, 1, static_cast<std::int32_t>(rows), static_cast<std::int32_t>(rows) + 1,
                                  INT_MAX, -1, INT_MIN};
  for (const std::int32_t index : indices) {
      std::uint8_t descriptor[48] = {};
      std::memset(descriptor, 0xee, sizeof descriptor);
      CHECK_EQ(table.describe(index, sizeof descriptor, descriptor), rows);
      const bool in_range = index >= 1 && index <= static_cast<std::int32_t>(rows);
      CHECK_EQ(table.contains(index), in_range);
      CHECK_EQ(descriptor[0] == 0xee, !in_range);
      /* Nothing past the descriptor's own 40 bytes is written. */
      CHECK_EQ(descriptor[40], 0xeeu);
  }
  CHECK_EQ(table.describe(1, 0u, nullptr), rows); }

TEST(pixel_format_table, writes_nothing_into_room_smaller_than_a_descriptor)
{ const PixelFormatTable table;
  const std::uint32_t too_small[] = {0u, 1u, 39u};
  for (const std::uint32_t bytes : too_small) {
      std::uint8_t descriptor[40] = {};
      std::memset(descriptor, 0xee, sizeof descriptor);
      CHECK_EQ(table.describe(1, bytes, descriptor), rows);
      for (const std::uint8_t byte : descriptor) CHECK_EQ(byte, 0xeeu);
  }
  std::uint8_t exact[40] = {};
  CHECK_EQ(table.describe(1, 40u, exact), rows);
  CHECK_EQ(u16(exact, 0u), 40u); }

TEST(pixel_format_table, every_row_is_a_composited_rgba8_window_format_with_no_palette_or_bitmap)
{ const PixelFormatTable table;
  for (std::int32_t index = 1; index <= static_cast<std::int32_t>(rows); index++) {
      std::uint8_t d[40] = {};
      CHECK_EQ(table.describe(index, sizeof d, d), rows);
      const av::glicd::PixelFormat &format = table.format(index);
      CHECK_EQ(u16(d, 0u), 40u);                     /* nSize */
      CHECK_EQ(u16(d, 2u), 1u);                      /* nVersion */
      const std::uint32_t flags = u32(d, 4u);
      CHECK_EQ(flags & 0x4u, 0x4u);                  /* PFD_DRAW_TO_WINDOW */
      CHECK_EQ(flags & 0x20u, 0x20u);                /* PFD_SUPPORT_OPENGL */
      CHECK_EQ(flags & 0x8000u, 0x8000u);            /* PFD_SUPPORT_COMPOSITION */
      CHECK_EQ(flags & forbidden_flags, 0u);
      CHECK_EQ((flags & 0x1u) != 0u, format.double_buffer());
      CHECK_EQ(flags, format.flags());
      CHECK_EQ(d[8], pfd_type_rgba);                 /* iPixelType */
      CHECK_EQ(d[9], 32u);                           /* cColorBits */
      CHECK_EQ(d[10], 8u);  CHECK_EQ(d[11], 16u);    /* red bits and shift: BGRA */
      CHECK_EQ(d[12], 8u);  CHECK_EQ(d[13], 8u);
      CHECK_EQ(d[14], 8u);  CHECK_EQ(d[15], 0u);
      CHECK_EQ(d[16], 8u);  CHECK_EQ(d[17], 24u);
      for (std::uint32_t accumulation = 18u; accumulation <= 22u; accumulation++) CHECK_EQ(d[accumulation], 0u);
      CHECK_EQ(d[23], format.depth_bits());
      CHECK_EQ(d[24], format.stencil_bits());
      CHECK_EQ(d[25], 0u);                           /* cAuxBuffers */
      CHECK_EQ(d[26], pfd_main_plane);
      CHECK_EQ(d[27], 0u);
      CHECK_EQ(u32(d, 28u), 0u);
      CHECK_EQ(u32(d, 32u), 0u);
      CHECK_EQ(u32(d, 36u), 0u);
  } }

TEST(pixel_format_table, holds_each_combination_exactly_once)
{ const PixelFormatTable table;
  std::uint32_t seen[3][2][2][2] = {};
  for (std::int32_t index = 1; index <= static_cast<std::int32_t>(rows); index++) {
      const av::glicd::PixelFormat &format = table.format(index);
      std::uint32_t depth = 3u;
      if (format.depth_bits() == 0u && format.stencil_bits() == 0u) depth = 0u;
      if (format.depth_bits() == 24u && format.stencil_bits() == 8u) depth = 1u;
      if (format.depth_bits() == 32u && format.stencil_bits() == 0u) depth = 2u;
      CHECK(depth < 3u);
      CHECK(format.samples() == 0u || format.samples() == 4u);
      if (depth < 3u) seen[depth][format.double_buffer()][format.srgb()][format.samples() == 4u]++;
  }
  for (const auto &by_depth : seen)
      for (const auto &by_buffer : by_depth)
          for (const auto &by_srgb : by_buffer)
              for (const std::uint32_t count : by_srgb) CHECK_EQ(count, 1u);
  /* The first row is what an application asking for nothing special gets:
   * double buffered, D24S8, no sRGB, no samples. */
  const av::glicd::PixelFormat &first = table.format(1);
  CHECK(first.double_buffer());
  CHECK_EQ(first.depth_bits(), 24u);
  CHECK_EQ(first.stencil_bits(), 8u);
  CHECK(!first.srgb());
  CHECK_EQ(first.samples(), 0u); }

TEST(pixel_format_table, two_tables_name_the_same_format_by_each_index)
{ const PixelFormatTable one;
  const PixelFormatTable two;
  for (std::int32_t index = 1; index <= static_cast<std::int32_t>(rows); index++) {
      std::uint8_t a[40] = {};
      std::uint8_t b[40] = {};
      CHECK_EQ(one.describe(index, 40u, a), rows);
      CHECK_EQ(two.describe(index, 40u, b), rows);
      CHECK(std::memcmp(a, b, sizeof a) == 0);
      CHECK_EQ(one.format(index).srgb(), two.format(index).srgb());
      CHECK_EQ(one.format(index).samples(), two.format(index).samples());
  } }

namespace av::glicd {

struct PixelFormatTableProbe {
    static void set(PixelFormatTable &table, std::uint32_t row, const av::glicd::PixelFormat &format)
    { table.rows_[row] = format; }
};

}  // namespace av::glicd

TEST(pixel_format_table, invariants_refuse_a_repeated_row_or_a_depth_it_does_not_offer)
{ using av::glicd::PixelFormatTableProbe;
  using Format = av::glicd::PixelFormat;
  { PixelFormatTable table;
    PixelFormatTableProbe::set(table, 5u, table.format(1));
    CHECK(!table.invariants_hold()); }
  { PixelFormatTable table;
    PixelFormatTableProbe::set(table, 0u, Format(16u, 0u, true, false, 0u));
    CHECK(!table.invariants_hold()); }
  { PixelFormatTable table;
    PixelFormatTableProbe::set(table, 0u, Format(24u, 8u, true, false, 2u));
    CHECK(!table.invariants_hold()); }
  { PixelFormatTable table;
    PixelFormatTableProbe::set(table, 0u, Format(24u, 0u, true, false, 0u));
    CHECK(!table.invariants_hold()); }
  { PixelFormatTable table;
    PixelFormatTableProbe::set(table, 0u, Format(0u, 8u, true, false, 0u));
    CHECK(!table.invariants_hold()); }
  { PixelFormatTable table;
    PixelFormatTableProbe::set(table, 0u, Format(32u, 8u, true, false, 0u));
    CHECK(!table.invariants_hold()); } }
