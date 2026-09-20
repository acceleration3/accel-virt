/* WGL_ARB_pixel_format and its multisample and sRGB attributes, answered from
 * the ICD's 24 rows (research.md §7, data-model.md): every attribute for every
 * row, choosing by exact and minimum criteria in the ARB's order, and every
 * list, format and output the contract refuses. The constants are wgl.xml's,
 * written out so a wrong one in the implementation fails here. */
#include "core/support/status.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "icd/opengl/pixel_format.hpp"
#include "icd/opengl/pixel_format_query.hpp"
#include "icd/opengl/pixel_format_table.hpp"

#include <cstdint>
#include <cstring>

using av::Status;
using av::glicd::PixelFormat;
using av::glicd::PixelFormatQuery;
using av::glicd::PixelFormatTable;

namespace av::glicd {

struct PixelFormatTableProbe {
    static void repeat_row(PixelFormatTable &table) { table.rows_[5] = table.rows_[1]; }
};

}  // namespace av::glicd

namespace {

enum : std::int32_t {
    number_pixel_formats = 0x2000, draw_to_window = 0x2001, draw_to_bitmap = 0x2002, acceleration = 0x2003,
    need_palette = 0x2004, need_system_palette = 0x2005, swap_layer_buffers = 0x2006, swap_method = 0x2007,
    number_overlays = 0x2008, number_underlays = 0x2009, transparent = 0x200A, share_depth = 0x200C,
    share_stencil = 0x200D, share_accum = 0x200E, support_gdi = 0x200F, support_opengl = 0x2010,
    double_buffer = 0x2011, stereo = 0x2012, pixel_type = 0x2013, color_bits = 0x2014, red_bits = 0x2015,
    red_shift = 0x2016, green_bits = 0x2017, green_shift = 0x2018, blue_bits = 0x2019, blue_shift = 0x201A,
    alpha_bits = 0x201B, alpha_shift = 0x201C, accum_bits = 0x201D, accum_red_bits = 0x201E,
    accum_green_bits = 0x201F, accum_blue_bits = 0x2020, accum_alpha_bits = 0x2021, depth_bits = 0x2022,
    stencil_bits = 0x2023, aux_buffers = 0x2024, full_acceleration = 0x2027, swap_undefined = 0x202A,
    type_rgba = 0x202B, draw_to_pbuffer = 0x202D, transparent_red_value = 0x2037,
    transparent_index_value = 0x203B, sample_buffers = 0x2041, samples = 0x2042,
    framebuffer_srgb_capable = 0x20A9,
};

constexpr std::uint32_t rows = 24u;

/* What wglGetPixelFormatAttribivARB answers for `row`, decided from the
 * contract rather than read off the implementation. */
std::int32_t expected_value(const PixelFormat &row, std::int32_t name)
{
    switch (name) {
    case number_pixel_formats: return static_cast<std::int32_t>(rows);
    case draw_to_window: return 1;
    case acceleration: return full_acceleration;
    case swap_method: return swap_undefined;
    case support_opengl: return 1;
    case double_buffer: return row.double_buffer() ? 1 : 0;
    case pixel_type: return type_rgba;
    case color_bits: return 32;
    case red_bits: case green_bits: case blue_bits: case alpha_bits: return 8;
    case red_shift: return 16;
    case green_shift: return 8;
    case blue_shift: return 0;
    case alpha_shift: return 24;
    case depth_bits: return row.depth_bits();
    case stencil_bits: return row.stencil_bits();
    case sample_buffers: return row.samples() != 0u ? 1 : 0;
    case samples: return row.samples();
    case framebuffer_srgb_capable: return row.srgb() ? 1 : 0;
    default: return 0;
    }
}

constexpr std::int32_t every_name[] = {
    number_pixel_formats, draw_to_window, draw_to_bitmap, acceleration, need_palette, need_system_palette,
    swap_layer_buffers, swap_method, number_overlays, number_underlays, transparent, share_depth, share_stencil,
    share_accum, support_gdi, support_opengl, double_buffer, stereo, pixel_type, color_bits, red_bits, red_shift,
    green_bits, green_shift, blue_bits, blue_shift, alpha_bits, alpha_shift, accum_bits, accum_red_bits,
    accum_green_bits, accum_blue_bits, accum_alpha_bits, depth_bits, stencil_bits, aux_buffers, draw_to_pbuffer,
    transparent_red_value, 0x2038, 0x2039, 0x203A, transparent_index_value, sample_buffers, samples,
    framebuffer_srgb_capable,
};
constexpr std::uint32_t name_count = sizeof every_name / sizeof every_name[0];

struct Chosen {
    Status        status;
    std::int32_t  formats[32];
    std::uint32_t count;
};

Chosen choose(const PixelFormatQuery &query, const std::int32_t *ints, const float *floats = nullptr,
              std::uint32_t max = 32u)
{
    Chosen chosen{Status::ok, {}, 0xEEEEu};
    for (std::int32_t &format : chosen.formats) format = -7;
    chosen.status = query.choose(ints, floats, max, chosen.formats, &chosen.count);
    return chosen;
}

bool reject_list(const std::uint8_t *bytes, std::size_t length)
{
    static const PixelFormatTable table;
    const PixelFormatQuery        query(table);
    std::int32_t                  list[260] = {};
    const std::size_t             copied = length < sizeof list ? length : sizeof list;
    std::memcpy(list, bytes, copied);
    std::int32_t  formats[32] = {};
    std::uint32_t count = 0u;
    const Status  chosen = query.choose(list, nullptr, 32u, formats, &count);
    if (chosen != Status::ok) return true;
    CHECK(count <= rows);
    for (std::uint32_t index = 0u; index < (count < 32u ? count : 32u); index++) CHECK(table.contains(formats[index]));
    return false;
}

}  // namespace

TEST(pixel_format_query_attribs, every_attribute_of_every_row_is_the_contract_s)
{ const PixelFormatTable table;
  const PixelFormatQuery query(table);
  for (std::int32_t format = 1; format <= static_cast<std::int32_t>(rows); format++) {
    std::int32_t values[name_count];
    for (std::int32_t &value : values) value = -9;
    CHECK_STATUS(query.attribs(format, 0, name_count, every_name, values, nullptr), Status::ok);
    float floats[name_count];
    CHECK_STATUS(query.attribs(format, 0, name_count, every_name, nullptr, floats), Status::ok);
    for (std::uint32_t at = 0u; at < name_count; at++) {
      const std::int32_t want = expected_value(table.format(format), every_name[at]);
      CHECK_EQ(values[at], want);
      CHECK(floats[at] == static_cast<float>(want));
    }
  }
  CHECK(query.invariants_hold()); }

TEST(pixel_format_query_attribs, the_rows_cover_srgb_samples_and_every_depth)
{ const PixelFormatTable table;
  const PixelFormatQuery query(table);
  const std::int32_t names[] = {framebuffer_srgb_capable, samples, sample_buffers, depth_bits, double_buffer};
  unsigned srgb = 0u, multisampled = 0u, d24 = 0u, d32 = 0u, none = 0u, doubled = 0u;
  for (std::int32_t format = 1; format <= 24; format++) {
    std::int32_t values[5] = {};
    CHECK_STATUS(query.attribs(format, 0, 5u, names, values, nullptr), Status::ok);
    srgb += values[0] == 1 ? 1u : 0u;
    multisampled += values[1] == 4 && values[2] == 1 ? 1u : 0u;
    d24 += values[3] == 24 ? 1u : 0u;
    d32 += values[3] == 32 ? 1u : 0u;
    none += values[3] == 0 ? 1u : 0u;
    doubled += values[4] == 1 ? 1u : 0u;
  }
  CHECK_EQ(srgb, 12u);
  CHECK_EQ(multisampled, 12u);
  CHECK_EQ(d24, 8u);
  CHECK_EQ(d32, 8u);
  CHECK_EQ(none, 8u);
  CHECK_EQ(doubled, 12u); }

/* WGL_NUMBER_PIXEL_FORMATS_ARB is asked with format 0, which no other
 * attribute accepts. */
TEST(pixel_format_query_attribs, the_format_count_answers_for_index_zero_only_alone)
{ const PixelFormatTable table;
  const PixelFormatQuery query(table);
  const std::int32_t count_only[] = {number_pixel_formats, number_pixel_formats};
  std::int32_t values[2] = {-1, -1};
  CHECK_STATUS(query.attribs(0, 0, 2u, count_only, values, nullptr), Status::ok);
  CHECK_EQ(values[0], 24);
  CHECK_EQ(values[1], 24);
  const std::int32_t mixed[] = {number_pixel_formats, depth_bits};
  std::int32_t untouched[2] = {-1, -1};
  CHECK_STATUS(query.attribs(0, 0, 2u, mixed, untouched, nullptr), Status::not_found);
  CHECK_EQ(untouched[0], -1);
  CHECK_EQ(untouched[1], -1); }

TEST(pixel_format_query_attribs, refuses_a_bad_format_layer_name_or_output_and_writes_nothing)
{ const PixelFormatTable table;
  const PixelFormatQuery query(table);
  const std::int32_t names[] = {depth_bits, 0x1FFF};
  const std::int32_t one[] = {depth_bits};
  std::int32_t values[2] = {-1, -1};
  float floats[2] = {-1.0f, -1.0f};
  CHECK_STATUS(query.attribs(1, 0, 2u, names, values, nullptr), Status::invalid_parameter);
  CHECK_EQ(values[0], -1);
  const std::int32_t outside[] = {-1, 25, 0x7FFFFFFF, static_cast<std::int32_t>(0x80000000u)};
  for (const std::int32_t format : outside) {
    CHECK_STATUS(query.attribs(format, 0, 1u, one, values, nullptr), Status::not_found);
  }
  CHECK_STATUS(query.attribs(0, 0, 1u, one, values, nullptr), Status::not_found);
  CHECK_STATUS(query.attribs(1, 1, 1u, one, values, nullptr), Status::invalid_parameter);
  CHECK_STATUS(query.attribs(1, -1, 1u, one, values, nullptr), Status::invalid_parameter);
  CHECK_STATUS(query.attribs(1, 0, 1u, nullptr, values, nullptr), Status::invalid_parameter);
  CHECK_STATUS(query.attribs(1, 0, 1u, one, nullptr, nullptr), Status::invalid_parameter);
  CHECK_STATUS(query.attribs(1, 0, 1u, one, values, floats), Status::invalid_parameter);
  CHECK_STATUS(query.attribs(1, 0, 129u, one, values, nullptr), Status::invalid_parameter);
  CHECK_STATUS(query.attribs(1, 0, 0u, one, values, nullptr), Status::ok);
  CHECK_EQ(values[0], -1);
  CHECK_EQ(values[1], -1);
  CHECK(floats[0] == -1.0f); }

TEST(pixel_format_query_attribs, a_list_of_128_names_is_the_longest_answered)
{ const PixelFormatTable table;
  const PixelFormatQuery query(table);
  std::int32_t names[128];
  std::int32_t values[128];
  for (std::int32_t &name : names) name = depth_bits;
  CHECK_STATUS(query.attribs(1, 0, 128u, names, values, nullptr), Status::ok);
  CHECK_EQ(values[127], 24); }

TEST(pixel_format_query_choose, nothing_asked_is_every_format_in_table_order)
{ const PixelFormatTable table;
  const PixelFormatQuery query(table);
  const Chosen all = choose(query, nullptr);
  CHECK_STATUS(all.status, Status::ok);
  CHECK_EQ(all.count, 24u);
  for (std::int32_t index = 0; index < 24; index++) CHECK_EQ(all.formats[index], index + 1);
  CHECK_EQ(all.formats[24], -7);
  const std::int32_t empty[] = {0};
  const Chosen terminated = choose(query, empty);
  CHECK_EQ(terminated.count, 24u);
  CHECK_EQ(terminated.formats[23], 24); }

/* What a typical application asks: a window, OpenGL, double buffered, RGBA
 * with 24 depth bits - the exact D24S8 rows first, D32 after (more bits). */
TEST(pixel_format_query_choose, exact_matches_come_before_ones_with_more_bits)
{ const PixelFormatTable table;
  const PixelFormatQuery query(table);
  const std::int32_t asked[] = {draw_to_window, 1, support_opengl, 1, double_buffer, 1, pixel_type, type_rgba,
                                color_bits, 24, depth_bits, 24, stencil_bits, 8, 0};
  const Chosen chosen = choose(query, asked);
  CHECK_STATUS(chosen.status, Status::ok);
  CHECK_EQ(chosen.count, 4u);
  std::int32_t depth[4] = {};
  std::int32_t sample[4] = {};
  for (std::uint32_t at = 0u; at < 4u; at++) {
    const PixelFormat &row = table.format(chosen.formats[at]);
    CHECK(row.double_buffer());
    CHECK_EQ(row.stencil_bits(), 8u);
    depth[at] = row.depth_bits();
    sample[at] = row.samples();
  }
  /* No D32 row has stencil, so the four are D24S8, equally close: they come
   * in table order, which lists single-sampled rows first. */
  CHECK_EQ(depth[0], 24);
  CHECK_EQ(depth[3], 24);
  CHECK_EQ(sample[0], 0);
  CHECK_EQ(sample[1], 0);
  CHECK_EQ(sample[2], 4);
  CHECK_EQ(sample[3], 4);
  CHECK(chosen.formats[0] < chosen.formats[1]);
  const std::int32_t deeper[] = {depth_bits, 24, 0};
  const Chosen any_depth = choose(query, deeper);
  CHECK_EQ(any_depth.count, 16u);
  CHECK_EQ(table.format(any_depth.formats[0]).depth_bits(), 24u);
  CHECK_EQ(table.format(any_depth.formats[15]).depth_bits(), 32u); }

TEST(pixel_format_query_choose, srgb_and_multisample_criteria_select_their_rows)
{ const PixelFormatTable table;
  const PixelFormatQuery query(table);
  const std::int32_t srgb_ms[] = {framebuffer_srgb_capable, 1, sample_buffers, 1, samples, 2, 0};
  const Chosen chosen = choose(query, srgb_ms);
  CHECK_EQ(chosen.count, 6u);
  for (std::uint32_t at = 0u; at < chosen.count; at++) {
    CHECK(table.format(chosen.formats[at]).srgb());
    CHECK_EQ(table.format(chosen.formats[at]).samples(), 4u);
  }
  const std::int32_t plain[] = {framebuffer_srgb_capable, 0, samples, 0, 0};
  CHECK_EQ(choose(query, plain).count, 12u);
  const std::int32_t too_many[] = {samples, 8, 0};
  const Chosen none = choose(query, too_many);
  CHECK_STATUS(none.status, Status::ok);
  CHECK_EQ(none.count, 0u);
  CHECK_EQ(none.formats[0], -7);
  const std::int32_t unmet[] = {draw_to_bitmap, 1, 0};
  CHECK_EQ(choose(query, unmet).count, 0u);
  const std::int32_t accum[] = {accum_bits, 1, 0};
  CHECK_EQ(choose(query, accum).count, 0u);
  const std::int32_t pixel_index[] = {pixel_type, 0x202C, 0};
  CHECK_EQ(choose(query, pixel_index).count, 0u);
  const std::int32_t ignored[] = {number_pixel_formats, 3, red_shift, 99, 0};
  CHECK_EQ(choose(query, ignored).count, 24u); }

/* The float list carries the same names as floats; a name that is not a
 * whole number is no attribute. */
TEST(pixel_format_query_choose, the_float_list_constrains_like_the_integer_list)
{ const PixelFormatTable table;
  const PixelFormatQuery query(table);
  const float floats[] = {static_cast<float>(depth_bits), 32.0f, static_cast<float>(double_buffer), 0.0f, 0.0f};
  const Chosen chosen = choose(query, nullptr, floats);
  CHECK_EQ(chosen.count, 4u);
  for (std::uint32_t at = 0u; at < 4u; at++) {
    CHECK_EQ(table.format(chosen.formats[at]).depth_bits(), 32u);
    CHECK(!table.format(chosen.formats[at]).double_buffer());
  }
  const float fractional[] = {8226.5f, 1.0f, 0.0f};
  CHECK_STATUS(choose(query, nullptr, fractional).status, Status::invalid_parameter);
  const float negative[] = {-1.0f, 1.0f, 0.0f};
  CHECK_STATUS(choose(query, nullptr, negative).status, Status::invalid_parameter);
  const float huge[] = {1.0e10f, 1.0f, 0.0f};
  CHECK_STATUS(choose(query, nullptr, huge).status, Status::invalid_parameter);
  const float partial[] = {static_cast<float>(depth_bits), 23.5f, 0.0f};
  CHECK_EQ(choose(query, nullptr, partial).count, 16u); }

TEST(pixel_format_query_choose, max_limits_what_is_written_not_what_is_counted)
{ const PixelFormatTable table;
  const PixelFormatQuery query(table);
  const Chosen zero = choose(query, nullptr, nullptr, 0u);
  CHECK_STATUS(zero.status, Status::ok);
  CHECK_EQ(zero.count, 24u);
  CHECK_EQ(zero.formats[0], -7);
  const Chosen three = choose(query, nullptr, nullptr, 3u);
  CHECK_EQ(three.count, 24u);
  CHECK_EQ(three.formats[2], 3);
  CHECK_EQ(three.formats[3], -7);
  std::uint32_t count = 5u;
  CHECK_STATUS(query.choose(nullptr, nullptr, 0u, nullptr, &count), Status::ok);
  CHECK_EQ(count, 24u); }

TEST(pixel_format_query_choose, refuses_unknown_names_long_lists_and_missing_outputs)
{ const PixelFormatTable table;
  const PixelFormatQuery query(table);
  const std::int32_t unknown[] = {depth_bits, 24, 0x1234, 1, 0};
  const Chosen refused = choose(query, unknown);
  CHECK_STATUS(refused.status, Status::invalid_parameter);
  CHECK_EQ(refused.count, 0xEEEEu);
  CHECK_EQ(refused.formats[0], -7);
  std::int32_t unterminated[129 * 2];
  for (std::uint32_t at = 0u; at < 129u; at++) {
    unterminated[2 * at] = depth_bits;
    unterminated[2 * at + 1] = 0;
  }
  CHECK_STATUS(choose(query, unterminated).status, Status::invalid_parameter);
  std::int32_t longest[128 * 2 + 1];
  for (std::uint32_t at = 0u; at < 128u; at++) {
    longest[2 * at] = depth_bits;
    longest[2 * at + 1] = 0;
  }
  longest[256] = 0;
  CHECK_STATUS(choose(query, longest).status, Status::ok);
  std::int32_t formats[4] = {};
  std::uint32_t count = 0u;
  CHECK_STATUS(query.choose(nullptr, nullptr, 4u, nullptr, &count), Status::invalid_parameter);
  CHECK_STATUS(query.choose(nullptr, nullptr, 4u, formats, nullptr), Status::invalid_parameter);
  float long_floats[129 * 2];
  for (std::uint32_t at = 0u; at < 129u; at++) {
    long_floats[2 * at] = static_cast<float>(depth_bits);
    long_floats[2 * at + 1] = 0.0f;
  }
  CHECK_STATUS(choose(query, nullptr, long_floats).status, Status::invalid_parameter);
  const float unknown_float[] = {4660.0f, 1.0f, 0.0f};
  CHECK_STATUS(choose(query, nullptr, unknown_float).status, Status::invalid_parameter); }

TEST(pixel_format_query_choose, hostile_lists_answer_only_formats_that_exist)
{ const std::int32_t valid[] = {draw_to_window, 1, depth_bits, 24, samples, 4, framebuffer_srgb_capable, 1, 0};
  std::uint8_t bytes[sizeof valid];
  std::memcpy(bytes, valid, sizeof bytes);
  av::HostileSweep sweep(&reject_list, bytes, sizeof bytes);
  sweep.run();
  CHECK(sweep.cases_run() > sizeof bytes);
  /* An attribute list is name-value pairs ended by a zero name, and only the
   * names are judged: every value is a criterion the query can hold, so all 128
   * value bits are acceptable. Of the four names, eleven flips land on another
   * attribute WGL defines - seven of draw_to_window, three of depth_bits, one
   * of samples and none of the sRGB name - and the terminator's flip to 0x2000
   * is another name too, with a zero value behind it. A cut inside a name is
   * only a partial name at its first byte, the other two bytes of each name
   * being zero, so 32 of the 36 truncations end in a list that still reads. An
   * empty list asks for nothing and matches every format, which is why all-zero
   * is accepted; all-ones is a name nobody defined. */
  const std::uint32_t acceptable = 128u + 11u + 1u + 32u + 1u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }

TEST(pixel_format_query_invariants, fail_for_a_table_that_is_not_whole)
{ PixelFormatTable table;
  const PixelFormatQuery query(table);
  CHECK(query.invariants_hold());
  av::glicd::PixelFormatTableProbe::repeat_row(table);
  CHECK(!query.invariants_hold()); }
