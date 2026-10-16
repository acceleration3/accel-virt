#include "core/support/four_cc.hpp"
#include "device/display/scanout_format.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::FourCc;
using av::ScanoutFormat;

TEST(scanout_format, each_virtio_format_is_its_own_bytes_read_the_other_way_round)
{ struct Row { std::uint32_t format; std::uint32_t fourcc; };
  const Row rows[] = {
      {ScanoutFormat::b8g8r8a8, FourCc('A', 'R', '2', '4').value()},   /* bytes B G R A */
      {ScanoutFormat::b8g8r8x8, FourCc('X', 'R', '2', '4').value()},   /* bytes B G R X */
      {ScanoutFormat::a8r8g8b8, FourCc('B', 'A', '2', '4').value()},   /* bytes A R G B */
      {ScanoutFormat::x8r8g8b8, FourCc('B', 'X', '2', '4').value()},   /* bytes X R G B */
      {ScanoutFormat::r8g8b8a8, FourCc('A', 'B', '2', '4').value()},   /* bytes R G B A */
      {ScanoutFormat::x8b8g8r8, FourCc('R', 'X', '2', '4').value()},   /* bytes X B G R */
      {ScanoutFormat::a8b8g8r8, FourCc('R', 'A', '2', '4').value()},   /* bytes A B G R */
      {ScanoutFormat::r8g8b8x8, FourCc('X', 'B', '2', '4').value()},   /* bytes R G B X */
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
