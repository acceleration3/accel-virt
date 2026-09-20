#include "device/display/scanout_format.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::ScanoutFormat;

namespace {

/* A DRM fourcc is the four characters of its name, low byte first. The rule
 * the table has to satisfy: a virtio-gpu format names its bytes in memory, a
 * DRM fourcc names the same bytes read as one little-endian word, so the two
 * names are each other reversed. */
constexpr std::uint32_t fourcc(char a, char b, char c, char d)
{
    return static_cast<std::uint32_t>(a) | (static_cast<std::uint32_t>(b) << 8) |
           (static_cast<std::uint32_t>(c) << 16) | (static_cast<std::uint32_t>(d) << 24);
}

}  // namespace

TEST(scanout_format, each_virtio_format_is_its_own_bytes_read_the_other_way_round)
{ struct Row { std::uint32_t format; std::uint32_t fourcc; };
  const Row rows[] = {
      {ScanoutFormat::b8g8r8a8, fourcc('A', 'R', '2', '4')},   /* bytes B G R A */
      {ScanoutFormat::b8g8r8x8, fourcc('X', 'R', '2', '4')},   /* bytes B G R X */
      {ScanoutFormat::a8r8g8b8, fourcc('B', 'A', '2', '4')},   /* bytes A R G B */
      {ScanoutFormat::x8r8g8b8, fourcc('B', 'X', '2', '4')},   /* bytes X R G B */
      {ScanoutFormat::r8g8b8a8, fourcc('A', 'B', '2', '4')},   /* bytes R G B A */
      {ScanoutFormat::x8b8g8r8, fourcc('R', 'X', '2', '4')},   /* bytes X B G R */
      {ScanoutFormat::a8b8g8r8, fourcc('R', 'A', '2', '4')},   /* bytes A B G R */
      {ScanoutFormat::r8g8b8x8, fourcc('X', 'B', '2', '4')},   /* bytes R G B X */
  };
  for (const Row &row : rows) {
      const ScanoutFormat format(row.format);
      CHECK(format.scannable());
      CHECK_EQ(format.fourcc(), row.fourcc);
      CHECK_EQ(format.pixel_bytes(), 4u);
      CHECK_EQ(format.format(), row.format);
  } }

TEST(scanout_format, every_other_id_is_not_one_this_device_shows)
{ const std::uint32_t refused[] = {0u, 5u, 66u, 69u, 120u, 122u, 133u, 135u, 0xFFFFFFFFu};
  for (std::uint32_t id : refused) {
      const ScanoutFormat format(id);
      CHECK(!format.scannable());
      CHECK_EQ(format.fourcc(), 0u);
      CHECK_EQ(format.pixel_bytes(), 0u);
  } }

TEST(scanout_format, no_two_formats_answer_the_same_fourcc)
{ const std::uint32_t ids[] = {ScanoutFormat::b8g8r8a8, ScanoutFormat::b8g8r8x8, ScanoutFormat::a8r8g8b8,
                               ScanoutFormat::x8r8g8b8, ScanoutFormat::r8g8b8a8, ScanoutFormat::x8b8g8r8,
                               ScanoutFormat::a8b8g8r8, ScanoutFormat::r8g8b8x8};
  for (std::size_t left = 0u; left < sizeof ids / sizeof ids[0]; left++)
      for (std::size_t right = left + 1u; right < sizeof ids / sizeof ids[0]; right++)
          CHECK(ScanoutFormat(ids[left]).fourcc() != ScanoutFormat(ids[right]).fourcc()); }
