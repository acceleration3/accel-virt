#include "miniport/ddi/dxgkarg_set_pointer_shape_record.hpp"
#include "miniport/display/cursor_image.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>
#include <cstring>

using av::CursorImage;
using av::DxgkargSetPointerShapeRecord;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;

namespace {

constexpr std::uint32_t monochrome = 1u, color = 2u, masked_color = 4u;

/* A poisoned destination one row larger than the image. */
struct Canvas {
    Canvas() { std::memset(pixels, 0xEE, sizeof pixels); }
    const std::uint8_t *at(std::uint32_t x, std::uint32_t y) const { return &pixels[(y * CursorImage::side + x) * 4u]; }
    bool is(std::uint32_t x, std::uint32_t y, std::uint8_t b, std::uint8_t g, std::uint8_t r, std::uint8_t a) const
    {
        const std::uint8_t *p = at(x, y);
        return p[0] == b && p[1] == g && p[2] == r && p[3] == a;
    }
    bool untouched_past_the_image() const
    {
        for (std::size_t byte = CursorImage::pixel_bytes; byte < sizeof pixels; byte++)
            if (pixels[byte] != 0xEEu) return false;
        return true;
    }
    std::uint8_t pixels[CursorImage::pixel_bytes + CursorImage::side * 4u];
};

Status draw(const DxgkargSetPointerShapeRecord &shape, Canvas &canvas)
{
    MockPlatform platform;
    return CursorImage(platform, shape).draw(canvas.pixels);
}

}  // namespace

TEST(cursor_image_draw, colour_pixels_are_copied_with_their_alpha)
{ std::uint8_t argb[2 * 8u] = {};
  const std::uint8_t first[] = {0x10, 0x20, 0x30, 0x80}, second[] = {1, 2, 3, 4};
  std::memcpy(argb, first, 4u);
  std::memcpy(&argb[8], second, 4u);
  const DxgkargSetPointerShapeRecord shape = {color, 1u, 2u, 8u, 0u, 0u, argb, 0u, 1u};
  Canvas canvas;
  CHECK_STATUS(draw(shape, canvas), Status::ok);
  CHECK(canvas.is(0u, 0u, 0x10, 0x20, 0x30, 0x80));
  CHECK(canvas.is(0u, 1u, 1, 2, 3, 4));
  CHECK(canvas.is(1u, 0u, 0, 0, 0, 0));
  CHECK(canvas.is(63u, 63u, 0, 0, 0, 0));
  CHECK(canvas.untouched_past_the_image()); }

TEST(cursor_image_draw, a_masked_colour_is_opaque_unless_it_masks_black)
{ const std::uint8_t pixels[3 * 4u] = {9, 8, 7, 0x00,      /* opaque colour */
                                       0, 0, 0, 0xFF,      /* transparent */
                                       9, 8, 7, 0xFF};     /* an XOR, shown opaque */
  const DxgkargSetPointerShapeRecord shape = {masked_color, 3u, 1u, 12u, 0u, 0u, pixels, 2u, 0u};
  Canvas canvas;
  CHECK_STATUS(draw(shape, canvas), Status::ok);
  CHECK(canvas.is(0u, 0u, 9, 8, 7, 0xFF));
  CHECK(canvas.is(1u, 0u, 0, 0, 0, 0));
  CHECK(canvas.is(2u, 0u, 9, 8, 7, 0xFF)); }

TEST(cursor_image_draw, a_monochrome_pointer_reads_and_then_xor_masks)
{ /* Four pixels in one row, pitch 1: AND 0b0101, XOR 0b0011 (most significant bit first). */
  const std::uint8_t masks[2] = {0x50, 0x30};
  const DxgkargSetPointerShapeRecord shape = {monochrome, 4u, 1u, 1u, 0u, 0u, masks, 0u, 0u};
  Canvas canvas;
  CHECK_STATUS(draw(shape, canvas), Status::ok);
  CHECK(canvas.is(0u, 0u, 0, 0, 0, 0xFF));          /* AND 0 XOR 0: black */
  CHECK(canvas.is(1u, 0u, 0, 0, 0, 0));             /* AND 1 XOR 0: transparent */
  CHECK(canvas.is(2u, 0u, 0xFF, 0xFF, 0xFF, 0xFF)); /* AND 0 XOR 1: white */
  CHECK(canvas.is(3u, 0u, 0, 0, 0, 0xFF)); }        /* AND 1 XOR 1: invert, shown black */

TEST(cursor_image_draw, rows_are_top_down_at_the_pitch)
{ std::uint8_t masks[2 * 2u * 2u] = {};
  masks[0] = 0x00; masks[2] = 0x80;   /* AND rows */
  masks[4] = 0x80; masks[6] = 0x00;   /* XOR rows */
  const DxgkargSetPointerShapeRecord shape = {monochrome, 1u, 2u, 2u, 0u, 0u, masks, 0u, 1u};
  Canvas canvas;
  CHECK_STATUS(draw(shape, canvas), Status::ok);
  CHECK(canvas.is(0u, 0u, 0xFF, 0xFF, 0xFF, 0xFF));
  CHECK(canvas.is(0u, 1u, 0, 0, 0, 0)); }

TEST(cursor_image_draw, the_largest_shape_fills_the_image)
{ std::uint8_t pixels[CursorImage::side * CursorImage::side * 4u];
  std::memset(pixels, 0x77, sizeof pixels);
  const DxgkargSetPointerShapeRecord shape = {color, 64u, 64u, 256u, 0u, 0u, pixels, 63u, 63u};
  Canvas canvas;
  CHECK_STATUS(draw(shape, canvas), Status::ok);
  CHECK(canvas.is(63u, 63u, 0x77, 0x77, 0x77, 0x77));
  CHECK(canvas.untouched_past_the_image()); }

TEST(cursor_image_draw, a_shape_the_adapter_cannot_show_writes_nothing)
{ const std::uint8_t pixels[4] = {};
  const DxgkargSetPointerShapeRecord good = {color, 1u, 1u, 4u, 0u, 0u, pixels, 0u, 0u};
  DxgkargSetPointerShapeRecord shapes[12];
  for (DxgkargSetPointerShapeRecord &shape : shapes) shape = good;
  shapes[0].Flags = 0u;
  shapes[1].Flags = color | monochrome;
  shapes[2].Flags = 8u;
  shapes[3].Width = 0u;
  shapes[4].Height = 0u;
  shapes[5].Width = 65u;
  shapes[6].Height = 65u;
  shapes[7].Pitch = 3u;
  shapes[8].pPixels = nullptr;
  shapes[9].XHot = 1u;
  shapes[10].YHot = 1u;
  shapes[11].Flags = monochrome;
  shapes[11].Width = 9u;
  shapes[11].Pitch = 1u;
  for (const DxgkargSetPointerShapeRecord &shape : shapes) {
      Canvas canvas;
      CHECK_STATUS(draw(shape, canvas), Status::invalid_parameter);
      CHECK_EQ(canvas.pixels[0], 0xEEu);
  } }

TEST(cursor_image, is_passive_only)
{ MockPlatform platform;
  const DxgkargSetPointerShapeRecord shape = {};
  Canvas canvas;
  platform.set_irql(Irql::dispatch);
  static_cast<void>(CursorImage(platform, shape).draw(canvas.pixels));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling)); }
