#include "harness/framework/test_registry.hpp"
#include "icd/opencl/cl_buffer_rect.hpp"

#include <cstddef>
#include <cstdint>

using av::clicd::ClBufferRect;

namespace {

constexpr std::size_t corner[3] = {4u, 2u, 1u};
constexpr std::size_t region[3] = {16u, 8u, 2u};

}

TEST(cl_buffer_rect, a_rectangle_at_tight_pitches_is_accepted_and_its_host_corner_found)
{ const ClBufferRect rect(corner, corner, region, 0u, 0u, 0u, 0u);
  CHECK_EQ(rect.check(), CL_SUCCESS);
  CHECK_EQ(rect.host_offset(), 1u * 128u + 2u * 16u + 4u);
  CHECK_EQ(rect.application().row_pitch(), 16u);
  CHECK_EQ(rect.buffer_side().slice_pitch(), 128u);
  std::size_t at[3] = {};
  rect.buffer_side().origin_of(3u, 5u, 1u, at);
  CHECK_EQ(at[0], 7u);
  CHECK_EQ(at[1], 7u);
  CHECK_EQ(at[2], 2u); }

TEST(cl_buffer_rect, the_host_corner_lies_at_the_applications_own_pitches)
{ const ClBufferRect rect(corner, corner, region, 32u, 512u, 64u, 1024u);
  CHECK_EQ(rect.check(), CL_SUCCESS);
  CHECK_EQ(rect.host_offset(), 1u * 1024u + 2u * 64u + 4u);
  CHECK_EQ(rect.buffer_side().row_pitch(), 32u);
  CHECK_EQ(rect.application().slice_pitch(), 1024u); }

TEST(cl_buffer_rect, a_missing_corner_or_region_is_invalid_and_never_read)
{ CHECK_EQ(ClBufferRect(nullptr, corner, region, 0u, 0u, 0u, 0u).check(), CL_INVALID_VALUE);
  CHECK_EQ(ClBufferRect(corner, nullptr, region, 0u, 0u, 0u, 0u).check(), CL_INVALID_VALUE);
  CHECK_EQ(ClBufferRect(corner, corner, nullptr, 0u, 0u, 0u, 0u).check(), CL_INVALID_VALUE); }

TEST(cl_buffer_rect, an_empty_extent_or_a_pitch_too_small_is_invalid)
{ const std::size_t flat[3] = {16u, 0u, 1u};
  CHECK_EQ(ClBufferRect(corner, corner, flat, 0u, 0u, 0u, 0u).check(), CL_INVALID_VALUE);
  CHECK_EQ(ClBufferRect(corner, corner, region, 15u, 0u, 0u, 0u).check(), CL_INVALID_VALUE);
  CHECK_EQ(ClBufferRect(corner, corner, region, 0u, 0u, 15u, 0u).check(), CL_INVALID_VALUE);
  CHECK_EQ(ClBufferRect(corner, corner, region, 16u, 127u, 0u, 0u).check(), CL_INVALID_VALUE);
  CHECK_EQ(ClBufferRect(corner, corner, region, 0u, 0u, 16u, 127u).check(), CL_INVALID_VALUE); }

TEST(cl_buffer_rect, a_slice_that_is_not_whole_rows_is_invalid_on_either_side)
{ CHECK_EQ(ClBufferRect(corner, corner, region, 16u, 136u, 0u, 0u).check(), CL_INVALID_VALUE);
  CHECK_EQ(ClBufferRect(corner, corner, region, 0u, 0u, 16u, 136u).check(), CL_INVALID_VALUE);
  CHECK_EQ(ClBufferRect(corner, corner, region, 16u, 144u, 16u, 144u).check(), CL_SUCCESS); }

TEST(cl_buffer_rect, a_host_corner_past_what_a_pointer_reaches_is_invalid)
{ const std::size_t far[3] = {0u, 0u, SIZE_MAX / 64u};
  CHECK_EQ(ClBufferRect(corner, far, region, 0u, 0u, 0u, 0u).check(), CL_INVALID_VALUE);
  const std::size_t past[3] = {1u, 1u, 0u};
  const std::size_t one[3] = {1u, 1u, 1u};
  CHECK_EQ(ClBufferRect(corner, past, one, 0u, 0u, SIZE_MAX, SIZE_MAX).check(), CL_INVALID_VALUE);
  const std::size_t near[3] = {1u, 0u, 0u};
  CHECK_EQ(ClBufferRect(corner, near, one, 0u, 0u, 0u, 0u).check(), CL_SUCCESS); }

TEST(cl_buffer_rect, rows_longer_than_one_command_counts_are_out_of_resources)
{ const std::size_t long_rows[3] = {std::size_t{1} << 33u, 1u, 1u};
  CHECK_EQ(ClBufferRect(corner, corner, long_rows, 0u, 0u, 0u, 0u).check(), CL_OUT_OF_RESOURCES);
  const std::size_t many_rows[3] = {1u, std::size_t{1} << 33u, 1u};
  CHECK_EQ(ClBufferRect(corner, corner, many_rows, 0u, 0u, 0u, 0u).check(), CL_OUT_OF_RESOURCES);
  const std::size_t one_row[3] = {16u, 1u, 1u};
  CHECK_EQ(ClBufferRect(corner, corner, one_row, 0u, 0u, std::size_t{1} << 33u, 0u).check(), CL_OUT_OF_RESOURCES); }
