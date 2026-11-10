#include "harness/framework/test_registry.hpp"
#include "icd/opengl/pixel_store.hpp"

#include <cstddef>
#include <cstdint>

using av::glicd::PixelStore;
using Field = PixelStore::Field;

namespace av::glicd {

struct PixelStoreProbe {
    static void break_alignment(PixelStore &store) { store.values_[0] = 3; }
    static void break_skip(PixelStore &store) { store.values_[4] = -1; }
};

}

namespace {

constexpr std::uint32_t rgba = 0x1908u, rgb = 0x1907u, red = 0x1903u, rg = 0x8227u, bgra = 0x80E1u,
                        depth_stencil = 0x84F9u, alpha = 0x1906u, luminance = 0x1909u, luminance_alpha = 0x190Au,
                        color_index = 0x1900u, alpha_integer = 0x8D97u, abgr = 0x8000u;
constexpr std::uint32_t ubyte = 0x1401u, sbyte = 0x1400u, ushort = 0x1403u, sshort = 0x1402u, uint = 0x1405u,
                        sint = 0x1404u, gl_float = 0x1406u, half = 0x140Bu, gl_double = 0x140Au;

struct Case {
    std::uint32_t format;
    std::uint32_t type;
    std::size_t   pixel;
};

}

TEST(pixel_store, a_pixel_is_its_components_times_its_value_bytes)
{ const PixelStore store;
  const Case cases[] = {
      {red, ubyte, 1u},  {red, sbyte, 1u},   {rg, ushort, 4u},   {rgb, sshort, 6u},  {rgba, uint, 16u},
      {bgra, sint, 16u}, {rgb, gl_float, 12u}, {alpha, half, 2u}, {0x1901u, ubyte, 1u}, {0x1902u, gl_float, 4u},
      {0x8D94u, ubyte, 1u}, {0x8D95u, ubyte, 1u}, {0x8D96u, ubyte, 1u}, {0x8228u, ubyte, 2u},
      {0x8D98u, ubyte, 3u}, {0x8D99u, ubyte, 4u}, {0x8D9Au, ubyte, 3u}, {0x8D9Bu, ubyte, 4u},
      {0x80E0u, ubyte, 3u}, {0x1904u, ubyte, 1u}, {0x1905u, ubyte, 1u},
      {luminance, ubyte, 1u}, {luminance, gl_float, 4u}, {luminance_alpha, ubyte, 2u},
      {luminance_alpha, half, 4u}, {color_index, ushort, 2u}, {alpha_integer, sint, 4u},
  };
  for (const Case &row : cases) {
      CHECK_EQ(store.span(row.format, row.type, 1, 1, 1), row.pixel);
  }
  CHECK(store.invariants_hold()); }

TEST(pixel_store, a_packed_type_is_one_value_per_pixel_and_only_with_its_format)
{ const PixelStore store;
  const Case packed[] = {
      {rgb, 0x8032u, 1u},  {rgb, 0x8362u, 1u},  {rgba, 0x8033u, 2u}, {rgba, 0x8034u, 2u}, {rgb, 0x8363u, 2u},
      {rgb, 0x8364u, 2u},  {bgra, 0x8365u, 2u}, {rgba, 0x8366u, 2u}, {rgba, 0x8035u, 4u}, {rgba, 0x8036u, 4u},
      {bgra, 0x8367u, 4u}, {rgba, 0x8368u, 4u}, {depth_stencil, 0x84FAu, 4u}, {rgb, 0x8C3Bu, 4u},
      {rgb, 0x8C3Eu, 4u},  {depth_stencil, 0x8DADu, 8u},
  };
  for (const Case &row : packed) CHECK_EQ(store.span(row.format, row.type, 1, 1, 1), row.pixel);
  const Case refused[] = {
      {rgba, 0x8032u, 0u}, {rgb, 0x8033u, 0u}, {rgba, 0x84FAu, 0u}, {depth_stencil, 0x8035u, 0u},
      {rgba, 0x8C3Eu, 0u}, {red, 0x8363u, 0u}, {depth_stencil, ubyte, 0u},
  };
  for (const Case &row : refused) CHECK_EQ(store.span(row.format, row.type, 1, 1, 1), 0u); }

TEST(pixel_store, nothing_is_sized_for_an_unknown_format_or_type_or_an_empty_transfer)
{ const PixelStore store;
  CHECK_EQ(store.span(abgr, ubyte, 4, 4, 1), 0u);
  CHECK_EQ(store.span(rgba, gl_double, 4, 4, 1), 0u);
  CHECK_EQ(store.span(0u, ubyte, 4, 4, 1), 0u);
  CHECK_EQ(store.span(rgba, 0u, 4, 4, 1), 0u);
  CHECK_EQ(store.span(rgba, ubyte, 0, 4, 1), 0u);
  CHECK_EQ(store.span(rgba, ubyte, 4, 0, 1), 0u);
  CHECK_EQ(store.span(rgba, ubyte, 4, 4, 0), 0u);
  CHECK_EQ(store.span(rgba, ubyte, -1, 4, 1), 0u);
  CHECK_EQ(store.span(rgba, ubyte, 4, -1, 1), 0u);
  CHECK_EQ(store.span(rgba, ubyte, 4, 4, -1), 0u); }

TEST(pixel_store, rows_pad_to_the_alignment_only_when_a_value_is_narrower)
{ PixelStore store;
  CHECK_EQ(store.span(rgb, ubyte, 3, 2, 1), 21u);
  CHECK(store.set(Field::alignment, 1));
  CHECK_EQ(store.span(rgb, ubyte, 3, 2, 1), 18u);
  CHECK(store.invariants_hold());
  CHECK(store.set(Field::alignment, 2));
  CHECK_EQ(store.span(rgb, ubyte, 3, 2, 1), 19u);
  CHECK(store.invariants_hold());
  CHECK(store.set(Field::alignment, 8));
  CHECK_EQ(store.span(rgb, ubyte, 3, 2, 1), 25u);
  CHECK(store.invariants_hold());
  CHECK_EQ(store.span(red, gl_float, 3, 2, 1), 16u + 12u);
  CHECK(store.set(Field::alignment, 4));
  CHECK_EQ(store.span(red, gl_float, 3, 2, 1), 24u);
  CHECK_EQ(store.span(rgb, ubyte, 4, 2, 1), 24u);
  CHECK(store.invariants_hold()); }

TEST(pixel_store, row_length_and_skips_move_the_span)
{ PixelStore store;
  CHECK(store.set(Field::alignment, 1));
  CHECK(store.set(Field::row_length, 10));
  CHECK_EQ(store.span(rgba, ubyte, 2, 3, 1), 2u * 40u + 8u);
  CHECK(store.set(Field::skip_rows, 2));
  CHECK(store.set(Field::skip_pixels, 5));
  CHECK_EQ(store.span(rgba, ubyte, 2, 3, 1), 4u * 40u + 7u * 4u);
  CHECK_EQ(store.get(Field::skip_rows), 2);
  CHECK_EQ(store.get(Field::skip_pixels), 5);
  CHECK_EQ(store.get(Field::row_length), 10);
  CHECK(store.invariants_hold()); }

TEST(pixel_store, a_volume_spans_its_images)
{ PixelStore store;
  CHECK(store.set(Field::alignment, 1));
  CHECK_EQ(store.span(red, ubyte, 2, 3, 4), 3u * 6u + 6u);
  CHECK(store.set(Field::image_height, 5));
  CHECK(store.set(Field::skip_images, 2));
  CHECK_EQ(store.span(red, ubyte, 2, 3, 4), 5u * 10u + 6u);
  CHECK_EQ(store.span(red, ubyte, 2, 3, 1), 6u);
  CHECK_EQ(store.get(Field::image_height), 5);
  CHECK_EQ(store.get(Field::skip_images), 2); }

TEST(pixel_store, a_value_the_host_would_refuse_is_not_kept)
{ PixelStore store;
  const std::int32_t alignments[] = {0, 3, 5, 16, -4};
  for (const std::int32_t alignment : alignments) {
      CHECK(!store.set(Field::alignment, alignment));
      CHECK_EQ(store.get(Field::alignment), 4);
  }
  const Field lengths[] = {Field::row_length, Field::image_height, Field::skip_pixels, Field::skip_rows,
                           Field::skip_images};
  for (const Field field : lengths) {
      CHECK(!store.set(field, -1));
      CHECK_EQ(store.get(field), 0);
      CHECK(store.set(field, 0));
  }
  CHECK(store.invariants_hold()); }

TEST(pixel_store, a_span_past_size_t_sizes_nothing)
{ PixelStore store;
  const std::int32_t most = 0x7FFFFFFF;
  CHECK(store.set(Field::alignment, 1));
  CHECK_EQ(store.span(rgba, gl_float, most, most, most), 0u);
  CHECK(store.set(Field::row_length, most));
  CHECK(store.set(Field::skip_rows, most));
  CHECK(store.set(Field::skip_pixels, most));
  CHECK(store.set(Field::skip_images, most));
  CHECK(store.set(Field::image_height, most));
  CHECK_EQ(store.span(rgba, gl_float, 1, most, 1), 0u);
  CHECK_EQ(store.span(rgba, gl_float, 1, 1, most), 0u);
  PixelStore wide;
  CHECK(wide.set(Field::alignment, 1));
  CHECK_EQ(wide.span(red, ubyte, most, 1, 1), 0x7FFFFFFFu);
  PixelStore padded;
  CHECK(padded.set(Field::alignment, 8));
  CHECK(padded.set(Field::row_length, most));
  CHECK_EQ(padded.span(rgb, ubyte, 1, 2, 1), static_cast<std::size_t>(3u * 0x7FFFFFFFull + 7u) / 8u * 8u + 3u); }

TEST(pixel_store, each_wrap_in_the_span_sizes_nothing)
{ constexpr std::uint32_t d24s8_float = 0x8DADu;
  const std::int32_t most = 0x7FFFFFFF;
  PixelStore plane;
  CHECK(plane.set(Field::row_length, most));
  CHECK(plane.set(Field::skip_rows, 1 << 30));
  CHECK_EQ(plane.span(depth_stencil, d24s8_float, 1, 1, 1), (std::size_t{1} << 30u) * 8u * 0x7FFFFFFFu + 8u);
  CHECK(plane.set(Field::skip_pixels, most));
  CHECK_EQ(plane.span(depth_stencil, d24s8_float, 1, 1, 1), 0u);
  PixelStore rows;
  CHECK(rows.set(Field::row_length, most));
  CHECK_EQ(rows.span(depth_stencil, d24s8_float, 1, most, 1), 0u);
  PixelStore image;
  CHECK(image.set(Field::row_length, most));
  CHECK(image.set(Field::image_height, most));
  CHECK_EQ(image.span(depth_stencil, d24s8_float, 1, 1, 2), 0u);
  PixelStore images;
  CHECK(images.set(Field::row_length, most));
  CHECK(images.set(Field::skip_images, most));
  CHECK_EQ(images.span(depth_stencil, d24s8_float, 1, 1, 2), 0u);
  PixelStore sum;
  CHECK(sum.set(Field::row_length, most));
  CHECK(sum.set(Field::skip_images, (1 << 30) - 1));
  CHECK_EQ(sum.span(depth_stencil, d24s8_float, 1, 1, 2),
           (std::size_t{1} << 30u) * 8u * 0x7FFFFFFFu + 8u);
  CHECK(sum.set(Field::skip_pixels, most));
  CHECK_EQ(sum.span(depth_stencil, d24s8_float, 1, 1, 2), 0u); }

TEST(pixel_store_invariants, fail_for_an_alignment_or_skip_no_set_accepts)
{ PixelStore alignment;
  av::glicd::PixelStoreProbe::break_alignment(alignment);
  CHECK(!alignment.invariants_hold());
  PixelStore skip;
  av::glicd::PixelStoreProbe::break_skip(skip);
  CHECK(!skip.invariants_hold()); }

TEST(pixel_store, the_layout_names_the_start_the_row_bytes_and_the_stride)
{ PixelStore store;
  std::uint64_t start = 0u, row = 0u, stride = 0u;
  CHECK(store.set(PixelStore::Field::alignment, 8));
  CHECK(store.layout(0x1907u, 0x1401u, 5, 2, start, row, stride));
  CHECK_EQ(start, 0u);
  CHECK_EQ(row, 15u);
  CHECK_EQ(stride, 16u);
  CHECK(store.set(PixelStore::Field::alignment, 1));
  CHECK(store.set(PixelStore::Field::row_length, 8));
  CHECK(store.set(PixelStore::Field::skip_rows, 1));
  CHECK(store.set(PixelStore::Field::skip_pixels, 2));
  CHECK(store.layout(0x1908u, 0x1401u, 3, 2, start, row, stride));
  CHECK_EQ(row, 12u);
  CHECK_EQ(stride, 32u);
  CHECK_EQ(start, 32u + 8u);
  CHECK(!store.layout(0x1908u, 0x1401u, 0, 2, start, row, stride));
  CHECK(!store.layout(0x1234u, 0x1401u, 3, 2, start, row, stride)); }
