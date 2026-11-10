#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opencl/cl_argument_type.hpp"
#include "icd/opencl/cl_kernel_shape.hpp"

#include <cstddef>

using av::MockPlatform;
using av::clicd::ClArgumentType;
using av::clicd::ClKernelShape;

namespace {

av::clicd::ClKernelLimits limits_of_a_gpu()
{
    av::clicd::ClKernelLimits limits = {};
    limits.group_size_ = 256u;
    limits.device_group_size_ = 1024u;
    limits.item_sizes_[0] = 1024u;
    limits.item_sizes_[1] = 1024u;
    limits.item_sizes_[2] = 64u;
    limits.item_dimensions_ = 3u;
    limits.device_local_bytes_ = 65536u;
    return limits;
}

}

TEST(cl_kernel_shape, an_argument_is_judged_by_the_kind_and_size_the_kernel_declared)
{ MockPlatform platform;
  ClKernelShape shape(platform);
  CHECK(shape.size_to(5u) == av::Status::ok);
  shape.describe(0u, ClKernelShape::ArgKind::buffer, 0u);
  shape.describe(1u, ClKernelShape::ArgKind::value, 4u);
  shape.describe(2u, ClKernelShape::ArgKind::local, 0u);
  shape.describe(3u, ClKernelShape::ArgKind::image, 0u);
  shape.describe(4u, ClKernelShape::ArgKind::sampler, 0u);
  CHECK_EQ(shape.count(), 5u);
  CHECK(shape.kind(1u) == ClKernelShape::ArgKind::value);
  const unsigned value = 7u;
  const void    *handle = &value;
  CHECK_EQ(shape.judge_argument(9u, 4u, &value), CL_INVALID_ARG_INDEX);
  CHECK_EQ(shape.judge_argument(1u, 4u, &value), CL_SUCCESS);
  CHECK_EQ(shape.judge_argument(1u, 2u, &value), CL_INVALID_ARG_SIZE);
  CHECK_EQ(shape.judge_argument(1u, 8u, &value), CL_INVALID_ARG_SIZE);
  CHECK_EQ(shape.judge_argument(1u, 4u, nullptr), CL_INVALID_ARG_VALUE);
  CHECK_EQ(shape.judge_argument(0u, 1u, &handle), CL_INVALID_ARG_SIZE);
  CHECK_EQ(shape.judge_argument(0u, sizeof(cl_mem), nullptr), CL_SUCCESS);
  CHECK_EQ(shape.judge_argument(2u, 64u, nullptr), CL_SUCCESS);
  CHECK_EQ(shape.judge_argument(2u, 0u, nullptr), CL_INVALID_ARG_SIZE);
  CHECK_EQ(shape.judge_argument(2u, 64u, &value), CL_INVALID_ARG_VALUE);
  CHECK_EQ(shape.judge_argument(3u, sizeof(cl_mem), nullptr), CL_INVALID_ARG_VALUE);
  CHECK_EQ(shape.judge_argument(4u, sizeof(cl_sampler), &handle), CL_SUCCESS);
  CHECK(shape.invariants_hold()); }

TEST(cl_kernel_shape, a_launch_needs_every_argument_set_and_a_sound_range)
{ MockPlatform platform;
  ClKernelShape shape(platform);
  CHECK(shape.size_to(2u) == av::Status::ok);
  shape.describe(0u, ClKernelShape::ArgKind::buffer, 0u);
  shape.describe(1u, ClKernelShape::ArgKind::local, 0u);
  shape.limit_by(limits_of_a_gpu());
  const std::size_t global[3] = {64u, 4u, 2u};
  const std::size_t local[3] = {8u, 2u, 1u};
  CHECK_EQ(shape.judge_launch(1u, nullptr, global, nullptr, false), CL_INVALID_KERNEL_ARGS);
  shape.note_set(0u, sizeof(cl_mem), nullptr);
  shape.note_set_pointer(0u);
  CHECK_EQ(shape.judge_launch(1u, nullptr, global, nullptr, false), CL_INVALID_KERNEL_ARGS);
  shape.note_set(1u, 128u, nullptr);
  CHECK_EQ(shape.judge_launch(1u, nullptr, global, nullptr, false), CL_SUCCESS);
  CHECK_EQ(shape.judge_launch(0u, nullptr, global, nullptr, false), CL_INVALID_WORK_DIMENSION);
  CHECK_EQ(shape.judge_launch(4u, nullptr, global, nullptr, false), CL_INVALID_WORK_DIMENSION);
  CHECK_EQ(shape.judge_launch(1u, nullptr, nullptr, nullptr, false), CL_INVALID_GLOBAL_WORK_SIZE);
  const std::size_t none[3] = {0u, 1u, 1u};
  CHECK_EQ(shape.judge_launch(1u, nullptr, none, nullptr, false), CL_INVALID_GLOBAL_WORK_SIZE);
  const std::size_t wrap[3] = {~std::size_t{0}, 0u, 0u};
  CHECK_EQ(shape.judge_launch(1u, wrap, global, nullptr, false), CL_INVALID_GLOBAL_OFFSET);
  CHECK_EQ(shape.judge_launch(3u, nullptr, global, local, false), CL_SUCCESS);
  const std::size_t uneven[3] = {10u, 1u, 1u};
  const std::size_t four[3] = {4u, 1u, 1u};
  CHECK_EQ(shape.judge_launch(1u, nullptr, uneven, four, false), CL_INVALID_WORK_GROUP_SIZE);
  CHECK_EQ(shape.judge_launch(1u, nullptr, uneven, four, true), CL_SUCCESS);
  const std::size_t zero_local[3] = {0u, 1u, 1u};
  CHECK_EQ(shape.judge_launch(1u, nullptr, global, zero_local, false), CL_INVALID_WORK_GROUP_SIZE);
  const std::size_t too_many[3] = {257u, 1u, 1u};
  const std::size_t big_global[3] = {257u, 1u, 1u};
  CHECK_EQ(shape.judge_launch(1u, nullptr, big_global, too_many, false), CL_INVALID_WORK_GROUP_SIZE);
  const std::size_t tall[3] = {1u, 2u, 65u};
  const std::size_t tall_global[3] = {1u, 2u, 65u};
  CHECK_EQ(shape.judge_launch(3u, nullptr, tall_global, tall, false), CL_INVALID_WORK_ITEM_SIZE);
  CHECK(shape.invariants_hold()); }

TEST(cl_kernel_shape, a_compiled_group_size_and_the_local_memory_bound_the_launch)
{ MockPlatform platform;
  ClKernelShape shape(platform);
  CHECK(shape.size_to(1u) == av::Status::ok);
  shape.describe(0u, ClKernelShape::ArgKind::local, 0u);
  ClKernelShape::Limits limits = limits_of_a_gpu();
  limits.compiled_group_[0] = 4u;
  limits.compiled_group_[1] = 1u;
  limits.compiled_group_[2] = 1u;
  shape.limit_by(limits);
  shape.note_set(0u, 1024u, nullptr);
  const std::size_t global[3] = {16u, 1u, 1u};
  const std::size_t right[3] = {4u, 1u, 1u};
  const std::size_t wrong[3] = {2u, 1u, 1u};
  CHECK_EQ(shape.judge_launch(1u, nullptr, global, right, false), CL_SUCCESS);
  CHECK_EQ(shape.judge_launch(1u, nullptr, global, wrong, false), CL_INVALID_WORK_GROUP_SIZE);
  shape.note_set(0u, 1u << 20u, nullptr);
  CHECK_EQ(shape.judge_launch(1u, nullptr, global, right, false), CL_OUT_OF_RESOURCES); }

TEST(cl_kernel_shape, a_copy_keeps_the_arguments_and_the_limits)
{ MockPlatform platform;
  ClKernelShape first(platform);
  ClKernelShape second(platform);
  CHECK(first.size_to(2u) == av::Status::ok);
  first.describe(0u, ClKernelShape::ArgKind::value, 4u);
  first.note_set_pointer(0u);
  first.limit_by(limits_of_a_gpu());
  CHECK(second.copy_from(first) == av::Status::ok);
  CHECK_EQ(second.count(), 2u);
  CHECK(second.kind(0u) == ClKernelShape::ArgKind::value);
  CHECK(second.kind(7u) == ClKernelShape::ArgKind::unknown); }

TEST(cl_argument_type, a_type_name_is_a_kind_and_a_size)
{ CHECK(ClArgumentType("uint", CL_KERNEL_ARG_ADDRESS_PRIVATE).kind() == ClKernelShape::ArgKind::value);
  CHECK_EQ(ClArgumentType("uint", CL_KERNEL_ARG_ADDRESS_PRIVATE).bytes(), 4u);
  CHECK_EQ(ClArgumentType("float4", CL_KERNEL_ARG_ADDRESS_PRIVATE).bytes(), 16u);
  CHECK_EQ(ClArgumentType("char3", CL_KERNEL_ARG_ADDRESS_PRIVATE).bytes(), 4u);
  CHECK_EQ(ClArgumentType("double16", CL_KERNEL_ARG_ADDRESS_PRIVATE).bytes(), 128u);
  CHECK_EQ(ClArgumentType("uint5", CL_KERNEL_ARG_ADDRESS_PRIVATE).bytes(), 0u);
  CHECK(ClArgumentType("struct pair", CL_KERNEL_ARG_ADDRESS_PRIVATE).kind() == ClKernelShape::ArgKind::unknown);
  CHECK(ClArgumentType("uint*", CL_KERNEL_ARG_ADDRESS_GLOBAL).kind() == ClKernelShape::ArgKind::buffer);
  CHECK(ClArgumentType("uint*", CL_KERNEL_ARG_ADDRESS_CONSTANT).kind() == ClKernelShape::ArgKind::buffer);
  CHECK(ClArgumentType("uint*", CL_KERNEL_ARG_ADDRESS_LOCAL).kind() == ClKernelShape::ArgKind::local);
  CHECK(ClArgumentType("image2d_t", CL_KERNEL_ARG_ADDRESS_GLOBAL).kind() == ClKernelShape::ArgKind::image);
  CHECK(ClArgumentType("sampler_t", CL_KERNEL_ARG_ADDRESS_PRIVATE).kind() == ClKernelShape::ArgKind::sampler);
  CHECK(ClArgumentType("", CL_KERNEL_ARG_ADDRESS_PRIVATE).kind() == ClKernelShape::ArgKind::unknown);
  CHECK(ClArgumentType(nullptr, CL_KERNEL_ARG_ADDRESS_PRIVATE).kind() == ClKernelShape::ArgKind::unknown); }
