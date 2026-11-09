#include "executor/d3d/stream_output_declaration.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

namespace {

struct Entry {
    std::uint32_t stream;
    std::uint32_t output_slot;
    std::uint32_t register_index;
    std::uint32_t mask;
};

using Declaration = av::StreamOutputDeclaration<Entry>;
constexpr std::uint32_t none_stream = 0xFFFFFFFFu;

}

TEST(stream_output_declaration, a_declaration_of_one_buffer_holds_and_converts_its_masks)
{ const Entry         entries[2] = {{0u, 0u, 0u, 0xFu}, {0u, 0u, 1u, 0x6u}};
  const std::uint32_t strides[1] = {24u};
  const Declaration   declaration(entries, 2u, strides, 1u, none_stream);
  CHECK(declaration.holds());
  CHECK_EQ(declaration.start_component(0u), 0u);
  CHECK_EQ(declaration.component_count(0u), 4u);
  CHECK_EQ(declaration.start_component(1u), 1u);
  CHECK_EQ(declaration.component_count(1u), 2u);
  CHECK_EQ(declaration.rasterized(), none_stream); }

TEST(stream_output_declaration, a_buffer_slot_or_stream_past_the_fourth_is_refused)
{ const std::uint32_t strides[4] = {16u, 16u, 16u, 16u};
  const Entry         slot[1] = {{0u, 200u, 0u, 0xFu}};
  const Entry         stream[1] = {{4u, 0u, 0u, 0xFu}};
  const Entry         edge[1] = {{3u, 3u, 0u, 0xFu}};
  CHECK(!Declaration(slot, 1u, strides, 4u, none_stream).holds());
  CHECK(!Declaration(slot, 1u, nullptr, 0u, none_stream).holds());
  CHECK(!Declaration(stream, 1u, strides, 4u, none_stream).holds());
  CHECK(Declaration(edge, 1u, strides, 4u, none_stream).holds()); }

TEST(stream_output_declaration, a_rasterized_stream_is_one_of_the_four_or_none)
{ const Entry entries[1] = {{0u, 0u, 0u, 0xFu}};
  const std::uint32_t strides[1] = {16u};
  CHECK(Declaration(entries, 1u, strides, 1u, 0u).holds());
  CHECK(Declaration(entries, 1u, strides, 1u, 3u).holds());
  CHECK(!Declaration(entries, 1u, strides, 1u, 4u).holds());
  CHECK(!Declaration(entries, 1u, strides, 1u, 0x80000000u).holds()); }

TEST(stream_output_declaration, strides_are_nonzero_words_of_at_most_2048_bytes)
{ const Entry entries[1] = {{0u, 0u, 0u, 0xFu}};
  const std::uint32_t zero[1] = {0u};
  const std::uint32_t odd[1] = {6u};
  const std::uint32_t largest[1] = {2048u};
  const std::uint32_t over[1] = {2052u};
  CHECK(!Declaration(entries, 1u, zero, 1u, none_stream).holds());
  CHECK(!Declaration(entries, 1u, odd, 1u, none_stream).holds());
  CHECK(Declaration(entries, 1u, largest, 1u, none_stream).holds());
  CHECK(!Declaration(entries, 1u, over, 1u, none_stream).holds());
  const std::uint32_t five[5] = {4u, 4u, 4u, 4u, 4u};
  CHECK(!Declaration(entries, 1u, five, 5u, none_stream).holds());
  CHECK(!Declaration(entries, 1u, nullptr, 1u, none_stream).holds()); }

TEST(stream_output_declaration, a_buffer_serves_one_stream_and_every_used_buffer_has_a_stride)
{ const std::uint32_t strides[2] = {16u, 16u};
  const Entry         mixed[2] = {{0u, 1u, 0u, 0xFu}, {1u, 1u, 1u, 0xFu}};
  const Entry         split[2] = {{0u, 0u, 0u, 0xFu}, {1u, 1u, 1u, 0xFu}};
  const Entry         unsized[1] = {{0u, 2u, 0u, 0xFu}};
  CHECK(!Declaration(mixed, 2u, strides, 2u, none_stream).holds());
  CHECK(Declaration(split, 2u, strides, 2u, none_stream).holds());
  CHECK(!Declaration(unsized, 1u, strides, 2u, none_stream).holds()); }

TEST(stream_output_declaration, a_missing_or_oversized_entry_list_is_refused)
{ const Entry entries[1] = {{0u, 0u, 0u, 0xFu}};
  CHECK(!Declaration(nullptr, 1u, nullptr, 0u, none_stream).holds());
  CHECK(!Declaration(entries, Declaration::most_entries + 1u, nullptr, 0u, none_stream).holds());
  CHECK(Declaration(nullptr, 0u, nullptr, 0u, none_stream).holds()); }

TEST(stream_output_declaration, a_mask_with_no_components_or_a_gap_in_them_is_refused)
{ const std::uint32_t strides[1] = {64u};
  const Entry         none[1] = {{0u, 0u, 0u, 0u}};
  const Entry         gap[1] = {{0u, 0u, 0u, 0xBu}};
  const Entry         middle[1] = {{0u, 0u, 0u, 0x6u}};
  CHECK(!Declaration(none, 1u, strides, 1u, none_stream).holds());
  CHECK(!Declaration(gap, 1u, strides, 1u, none_stream).holds());
  CHECK(Declaration(middle, 1u, strides, 1u, none_stream).holds()); }

TEST(stream_output_declaration, a_declaration_with_no_strides_serves_one_buffer_and_each_stride_holds_its_components)
{ const Entry first[2] = {{0u, 0u, 0u, 0xFu}, {0u, 0u, 1u, 0xFu}};
  const Entry second[1] = {{0u, 1u, 0u, 0xFu}};
  const std::uint32_t tight[1] = {32u};
  const std::uint32_t short_stride[1] = {28u};
  CHECK(Declaration(first, 2u, nullptr, 0u, none_stream).holds());
  CHECK(!Declaration(second, 1u, nullptr, 0u, none_stream).holds());
  CHECK(Declaration(first, 2u, tight, 1u, none_stream).holds());
  CHECK(!Declaration(first, 2u, short_stride, 1u, none_stream).holds()); }
