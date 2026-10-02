#include "guest/probe/bmp_image.hpp"
#include "guest/probe/image_sink.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/directory_image_sink.hpp"
#include "harness/guest/scripted_image_sink.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

using av::BmpImage;
using av::Status;
using Rows = av::BmpImage::Rows;

namespace av {

struct BmpImageProbe {
    static void break_rows(BmpImage &image) { image.rows_ = static_cast<BmpImage::Rows>(7); }
};

}  // namespace av

namespace {

/* Two rows of two pixels, as a probe reads them: red, green over blue, white. */
constexpr std::uint8_t two_by_two[16] = {
    255, 0, 0, 255,   0, 255, 0, 255,
    0, 0, 255, 255,   255, 255, 255, 128,
};

std::uint32_t u32_at(const std::uint8_t *bytes, std::size_t offset)
{
    return static_cast<std::uint32_t>(bytes[offset]) | static_cast<std::uint32_t>(bytes[offset + 1u]) << 8u |
           static_cast<std::uint32_t>(bytes[offset + 2u]) << 16u | static_cast<std::uint32_t>(bytes[offset + 3u]) << 24u;
}

}  // namespace

/* The Windows BITMAPFILEHEADER and BITMAPINFOHEADER, byte by byte, and rows
 * stored bottom-up in BGR padded to four bytes. */
TEST(bmp_image, encodes_a_top_first_image_as_a_bottom_up_24_bit_file)
{ const BmpImage image(two_by_two, 2u, 2u, Rows::top_first);
  CHECK(image.invariants_hold());
  CHECK_EQ(image.encoded_size(), 54u + 2u * 8u);
  std::uint8_t file[80];
  std::memset(file, 0xEE, sizeof file);
  CHECK_STATUS(image.encode(file, sizeof file), Status::ok);
  CHECK(file[0] == 'B' && file[1] == 'M');
  CHECK_EQ(u32_at(file, 2u), 70u);
  CHECK_EQ(u32_at(file, 6u), 0u);
  CHECK_EQ(u32_at(file, 10u), 54u);
  CHECK_EQ(u32_at(file, 14u), 40u);
  CHECK_EQ(u32_at(file, 18u), 2u);
  CHECK_EQ(u32_at(file, 22u), 2u);
  CHECK_EQ(file[26], 1u);
  CHECK_EQ(file[27], 0u);
  CHECK_EQ(file[28], 24u);
  CHECK_EQ(u32_at(file, 30u), 0u);
  CHECK_EQ(u32_at(file, 34u), 16u);
  CHECK_EQ(u32_at(file, 38u), 2835u);
  CHECK_EQ(u32_at(file, 42u), 2835u);
  CHECK_EQ(u32_at(file, 46u), 0u);
  CHECK_EQ(u32_at(file, 50u), 0u);
  /* The file's first row is the image's bottom one: blue, white. */
  const std::uint8_t rows[16] = {255, 0, 0,   255, 255, 255,   0, 0,
                                 0, 0, 255,   0, 255, 0,       0, 0};
  CHECK(std::memcmp(file + 54u, rows, sizeof rows) == 0);
  CHECK_EQ(file[70], 0xEEu); }

TEST(bmp_image, a_bottom_first_image_keeps_its_row_order)
{ const BmpImage image(two_by_two, 2u, 2u, Rows::bottom_first);
  CHECK(image.invariants_hold());
  std::uint8_t file[70];
  CHECK_STATUS(image.encode(file, sizeof file), Status::ok);
  const std::uint8_t rows[16] = {0, 0, 255,   0, 255, 0,       0, 0,
                                 255, 0, 0,   255, 255, 255,   0, 0};
  CHECK(std::memcmp(file + 54u, rows, sizeof rows) == 0); }

/* A width whose BGR bytes are already a multiple of four needs no padding. */
TEST(bmp_image, pads_each_row_only_to_the_next_multiple_of_four)
{ std::uint8_t one_row[16] = {};
  for (std::uint8_t at = 0u; at < 16u; at++) one_row[at] = at;
  CHECK_EQ(BmpImage(one_row, 1u, 1u, Rows::top_first).encoded_size(), 54u + 4u);
  CHECK_EQ(BmpImage(one_row, 3u, 1u, Rows::top_first).encoded_size(), 54u + 12u);
  CHECK_EQ(BmpImage(one_row, 4u, 1u, Rows::top_first).encoded_size(), 54u + 12u);
  std::uint8_t file[66];
  CHECK_STATUS(BmpImage(one_row, 4u, 1u, Rows::top_first).encode(file, sizeof file), Status::ok);
  const std::uint8_t row[12] = {2, 1, 0, 6, 5, 4, 10, 9, 8, 14, 13, 12};
  CHECK(std::memcmp(file + 54u, row, sizeof row) == 0); }

TEST(bmp_image, refuses_an_image_it_cannot_encode_and_a_short_destination)
{ std::uint8_t file[80];
  std::memset(file, 0xEE, sizeof file);
  const BmpImage image(two_by_two, 2u, 2u, Rows::top_first);
  CHECK_STATUS(image.encode(file, 69u), Status::no_space);
  CHECK_EQ(file[0], 0xEEu);
  CHECK_STATUS(image.encode(nullptr, sizeof file), Status::invalid_parameter);
  const BmpImage none(nullptr, 2u, 2u, Rows::top_first);
  const BmpImage narrow(two_by_two, 0u, 2u, Rows::top_first);
  const BmpImage flat(two_by_two, 2u, 0u, Rows::top_first);
  const BmpImage wide(two_by_two, BmpImage::max_extent + 1u, 1u, Rows::top_first);
  const BmpImage tall(two_by_two, 1u, BmpImage::max_extent + 1u, Rows::top_first);
  const BmpImage *const refusals[] = {&none, &narrow, &flat, &wide, &tall};
  for (const BmpImage *refused : refusals) {
    CHECK_EQ(refused->encoded_size(), 0u);
    CHECK_STATUS(refused->encode(file, sizeof file), Status::invalid_parameter);
  }
  CHECK_EQ(file[0], 0xEEu);
  CHECK_EQ(BmpImage(two_by_two, BmpImage::max_extent, 1u, Rows::top_first).encoded_size(),
           54u + static_cast<std::size_t>(BmpImage::max_extent) * 3u); }

TEST(bmp_image_invariants, fail_for_a_row_order_that_is_neither)
{ BmpImage image(two_by_two, 2u, 2u, Rows::top_first);
  av::BmpImageProbe::break_rows(image);
  CHECK(!image.invariants_hold()); }

TEST(scripted_image_sink, keeps_what_a_probe_saved_through_the_sink)
{ av::ScriptedImageSink scripted;
  av::ImageSink &sink = scripted;
  sink.save("opengl-draw", BmpImage(two_by_two, 2u, 2u, Rows::bottom_first));
  sink.save("refused", BmpImage(nullptr, 2u, 2u, Rows::bottom_first));
  CHECK_EQ(scripted.saved(), 2u);
  CHECK_EQ(scripted.size("opengl-draw"), 70u);
  CHECK(scripted.bytes("opengl-draw") != nullptr);
  CHECK(scripted.bytes("refused") == nullptr);
  CHECK(scripted.bytes("absent") == nullptr);
  for (int more = 0; more < 4; more++) sink.save("extra", BmpImage(two_by_two, 1u, 1u, Rows::top_first));
  CHECK_EQ(scripted.saved(), av::ScriptedImageSink::max_images); }

TEST(directory_image_sink, writes_each_picture_as_a_file)
{ char directory[] = "/tmp/av-bmp-XXXXXX";
  CHECK(mkdtemp(directory) != nullptr);
  av::DirectoryImageSink sink(directory);
  sink.save("vulkan-draw", BmpImage(two_by_two, 2u, 2u, Rows::top_first));
  sink.save("refused", BmpImage(nullptr, 2u, 2u, Rows::top_first));
  CHECK_EQ(sink.written(), 1u);
  char path[128];
  std::snprintf(path, sizeof path, "%s/vulkan-draw.bmp", directory);
  std::FILE *const file = std::fopen(path, "rb");
  CHECK(file != nullptr);
  std::uint8_t bytes[80] = {};
  const std::size_t read = file != nullptr ? std::fread(bytes, 1u, sizeof bytes, file) : 0u;
  if (file != nullptr) std::fclose(file);
  CHECK_EQ(read, 70u);
  CHECK(bytes[0] == 'B' && bytes[1] == 'M');
  std::remove(path);
  rmdir(directory); }
