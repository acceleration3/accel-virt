#include "harness/framework/test_registry.hpp"
#include "icd/opencl/cl_format_cache.hpp"
#include "icd/opencl/cl_image_judge.hpp"
#include "icd/opencl/cl_mem_extent.hpp"
#include "icd/opencl/cl_option_judge.hpp"

using av::clicd::ClFormatCache;
using av::clicd::ClImageJudge;
using av::clicd::ClImageLimits;
using av::clicd::ClMemExtent;
using av::clicd::ClOptionJudge;

namespace {

ClImageLimits limits()
{
    ClImageLimits made = {};
    made.width_2d_ = 16384u;
    made.height_2d_ = 16384u;
    made.width_3d_ = 2048u;
    made.height_3d_ = 2048u;
    made.depth_3d_ = 2048u;
    made.array_size_ = 2048u;
    made.buffer_size_ = 65536u;
    made.pitch_alignment_ = 4u;
    return made;
}

cl_image_desc described(cl_mem_object_type type, size_t width, size_t height = 1u)
{
    cl_image_desc desc = {};
    desc.image_type = type;
    desc.image_width = width;
    desc.image_height = height;
    desc.image_depth = 1u;
    desc.image_array_size = 1u;
    return desc;
}

}

TEST(cl_image_judge, a_size_past_what_the_device_makes_or_a_type_nobody_knows_is_refused)
{ const ClImageJudge judge(limits());
  CHECK_EQ(judge.judge(described(CL_MEM_OBJECT_IMAGE2D, 16384u, 4u), 4u, false, 0u, false), CL_SUCCESS);
  CHECK_EQ(judge.judge(described(CL_MEM_OBJECT_IMAGE2D, 16385u, 4u), 4u, false, 0u, false), CL_INVALID_IMAGE_SIZE);
  CHECK_EQ(judge.judge(described(CL_MEM_OBJECT_IMAGE2D, 0u, 4u), 4u, false, 0u, false), CL_INVALID_IMAGE_SIZE);
  CHECK_EQ(judge.judge(described(CL_MEM_OBJECT_IMAGE1D, 16385u), 4u, false, 0u, false), CL_INVALID_IMAGE_SIZE);
  CHECK_EQ(judge.judge(described(CL_MEM_OBJECT_IMAGE3D, 2049u, 1u), 4u, false, 0u, false), CL_INVALID_IMAGE_SIZE);
  cl_image_desc layers = described(CL_MEM_OBJECT_IMAGE1D_ARRAY, 8u);
  layers.image_array_size = 2049u;
  CHECK_EQ(judge.judge(layers, 4u, false, 0u, false), CL_INVALID_IMAGE_SIZE);
  CHECK_EQ(judge.judge(described(0x7777u, 8u), 4u, false, 0u, false), CL_INVALID_IMAGE_DESCRIPTOR); }

TEST(cl_image_judge, an_image_over_a_buffer_must_fit_it_and_follow_the_pitch_rules)
{ const ClImageJudge judge(limits());
  cl_image_desc line = described(CL_MEM_OBJECT_IMAGE1D_BUFFER, 64u);
  CHECK_EQ(judge.judge(line, 4u, true, 256u, false), CL_SUCCESS);
  CHECK_EQ(judge.judge(line, 4u, true, 255u, false), CL_INVALID_IMAGE_DESCRIPTOR);
  CHECK_EQ(judge.judge(line, 4u, false, 0u, false), CL_INVALID_IMAGE_DESCRIPTOR);
  CHECK_EQ(judge.judge(described(CL_MEM_OBJECT_IMAGE1D_BUFFER, 65537u), 1u, true, 1u << 20u, false), CL_INVALID_IMAGE_SIZE);
  CHECK_EQ(judge.judge(described(CL_MEM_OBJECT_IMAGE3D, 4u, 4u), 4u, true, 4096u, false), CL_INVALID_IMAGE_DESCRIPTOR);
  cl_image_desc flat = described(CL_MEM_OBJECT_IMAGE2D, 8u, 4u);
  flat.image_row_pitch = 16u;
  CHECK_EQ(judge.judge(flat, 4u, true, 4096u, false), CL_INVALID_IMAGE_DESCRIPTOR);
  flat.image_row_pitch = 34u;
  CHECK_EQ(judge.judge(flat, 4u, true, 4096u, false), CL_INVALID_IMAGE_DESCRIPTOR);
  flat.image_row_pitch = 64u;
  CHECK_EQ(judge.judge(flat, 4u, true, 4096u, false), CL_SUCCESS);
  CHECK_EQ(judge.judge(flat, 4u, true, 128u, false), CL_INVALID_IMAGE_DESCRIPTOR);
  flat.image_row_pitch = 16u;
  CHECK_EQ(judge.judge(flat, 4u, false, 0u, true), CL_INVALID_IMAGE_DESCRIPTOR);
  flat.image_row_pitch = 40u;
  CHECK_EQ(judge.judge(flat, 4u, false, 0u, true), CL_SUCCESS);
  CHECK_EQ(judge.judge(flat, 4u, false, 0u, false), CL_INVALID_IMAGE_DESCRIPTOR); }

TEST(cl_extent_pitches, a_buffer_backed_image_reports_the_pitches_the_specification_defines)
{ const cl_image_format format = {CL_RGBA, CL_UNORM_INT8};
  ClMemExtent line;
  line.as_image(CL_MEM_OBJECT_IMAGE1D_BUFFER, 32u, 1u, 1u, 1u, format);
  CHECK_EQ(line.row_pitch(), 128u);
  CHECK_EQ(line.slice_pitch(), 0u);
  ClMemExtent flat;
  flat.as_image(CL_MEM_OBJECT_IMAGE2D, 8u, 4u, 1u, 1u, format);
  flat.with_pitches(64u, 0u);
  CHECK_EQ(flat.row_pitch(), 64u);
  CHECK(flat.type() == CL_MEM_OBJECT_IMAGE2D);
  ClMemExtent volume;
  volume.as_image(CL_MEM_OBJECT_IMAGE3D, 8u, 4u, 2u, 1u, format);
  CHECK_EQ(volume.slice_pitch(), 32u * 4u); }

TEST(cl_option_judge, build_options_are_known_ones_and_a_standard_the_api_defines)
{ CHECK(ClOptionJudge(nullptr).valid());
  CHECK(ClOptionJudge("").valid());
  CHECK(ClOptionJudge("-D A=1 -DB=2 -I C:\\inc -IC:\\more -w -Werror -cl-fast-relaxed-math").valid());
  CHECK(ClOptionJudge("-cl-std=CL1.2 -cl-kernel-arg-info -x spir -spir-std=1.2 -create-library").valid());
  CHECK(ClOptionJudge("-cl-nv-verbose").valid());
  CHECK(!ClOptionJudge("-cl-std=CL9.9").valid());
  CHECK(!ClOptionJudge("-no-such-option").valid());
  CHECK(!ClOptionJudge("-D A -bogus").valid());
  CHECK(ClOptionJudge("-I -bogus-directory-name").valid()); }

TEST(cl_format_cache, a_list_is_kept_by_the_access_and_type_it_answered)
{ ClFormatCache cache;
  const cl_image_format formats[2] = {{CL_RGBA, CL_UNORM_INT8}, {CL_R, CL_FLOAT}};
  uint32_t count = 0u;
  CHECK(cache.find(CL_MEM_READ_ONLY, CL_MEM_OBJECT_IMAGE2D, count) == nullptr);
  CHECK(cache.keep(CL_MEM_READ_ONLY, CL_MEM_OBJECT_IMAGE2D, formats, 2u));
  const cl_image_format *const found = cache.find(CL_MEM_READ_ONLY, CL_MEM_OBJECT_IMAGE2D, count);
  CHECK(found != nullptr);
  CHECK_EQ(count, 2u);
  CHECK_EQ(found[1].image_channel_data_type, static_cast<cl_channel_type>(CL_FLOAT));
  CHECK(cache.find(CL_MEM_WRITE_ONLY, CL_MEM_OBJECT_IMAGE2D, count) == nullptr);
  CHECK(!cache.keep(CL_MEM_READ_ONLY, CL_MEM_OBJECT_IMAGE2D, formats, ClFormatCache::most_formats + 1u)); }
