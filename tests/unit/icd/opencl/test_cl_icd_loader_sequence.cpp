#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/cl_icd_rig.hpp"

#include <CL/cl_ext.h>

#include <cstring>

using av::ClIcdRig;
using av::FakeClLoader;
using av::MockKmt;

TEST(cl_icd_loader_sequence, a_loader_walking_its_own_steps_keeps_this_driver)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  CHECK_EQ(rig.loader().platforms(), 1u);
  CHECK_EQ(std::strcmp(rig.loader().suffix(), "AV"), 0);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_loader_sequence, deciding_whether_to_keep_the_driver_opens_nothing)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::enumerate_adapters), 0u);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.platform().live_allocations(), 0u); }

TEST(cl_icd_loader_sequence, a_library_that_exports_nothing_is_not_an_icd)
{ ClIcdRig rig;
  rig.loader().hide_export();
  CHECK(rig.loader().load() == FakeClLoader::Load::no_extension_function);
  CHECK_EQ(rig.loader().platforms(), 0u); }

TEST(cl_icd_loader_sequence, the_one_export_answers_the_enumeration_and_nothing_else)
{ ClIcdRig rig;
  CHECK(rig.root().extension_function("clIcdGetPlatformIDsKHR") != nullptr);
  CHECK(rig.root().extension_function("clGetPlatformIDs") == nullptr);
  CHECK(rig.root().extension_function("clIcdGetPlatformIDsKHRx") == nullptr);
  CHECK(rig.root().extension_function("clIcdGetPlatformIDsKH") == nullptr);
  CHECK(rig.root().extension_function("") == nullptr);
  CHECK(rig.root().extension_function(nullptr) == nullptr); }

TEST(cl_icd_loader_sequence, the_platform_carries_the_dispatch_table_in_its_first_word)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  CHECK(rig.loader().dispatch_of(rig.loader().platform(0u)) == &rig.root().dispatch()); }

TEST(cl_icd_loader_sequence, the_enumeration_answers_the_count_and_then_the_platforms)
{ ClIcdRig rig;
  cl_uint count = 0u;
  CHECK_EQ(rig.root().platform_ids(0u, nullptr, &count), CL_SUCCESS);
  CHECK_EQ(count, 1u);
  cl_platform_id platforms[2] = {};
  CHECK_EQ(rig.root().platform_ids(2u, platforms, nullptr), CL_SUCCESS);
  CHECK(platforms[0] != nullptr);
  CHECK_EQ(rig.root().platform_ids(0u, nullptr, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().platform_ids(0u, platforms, &count), CL_INVALID_VALUE); }

TEST(cl_icd_loader_sequence, the_extensions_are_answered_before_any_device_exists)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  CHECK(std::strstr(rig.loader().extensions(), "cl_khr_d3d11_sharing") != nullptr);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(cl_icd_loader_sequence, the_suffix_is_the_one_query_that_may_not_fail)
{ ClIcdRig rig;
  std::size_t bytes = 0u;
  CHECK_EQ(rig.root().platform_info(nullptr, CL_PLATFORM_ICD_SUFFIX_KHR, 0u, nullptr, &bytes),
           CL_SUCCESS);
  CHECK_EQ(bytes, 3u);
  char text[8] = {};
  CHECK_EQ(rig.root().platform_info(nullptr, CL_PLATFORM_ICD_SUFFIX_KHR, bytes, text, nullptr),
           CL_SUCCESS);
  CHECK_EQ(std::strcmp(text, "AV"), 0); }

TEST(cl_icd_loader_sequence, every_object_the_icd_hands_out_carries_the_same_table)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  const cl_icd_dispatch *const table = &rig.root().dispatch();
  CHECK(rig.loader().dispatch_of(rig.loader().platform(0u)) == table);
  cl_device_id device = nullptr;
  cl_uint      devices = 0u;
  if (rig.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device, &devices) == CL_SUCCESS) {
      CHECK_EQ(devices, 1u);
      CHECK(rig.loader().dispatch_of(device) == table);
  } }

TEST(cl_icd_loader_sequence, a_dispatch_table_leaves_no_slot_a_carried_command_could_fall_through)
{ ClIcdRig rig;
  const cl_icd_dispatch &table = rig.root().dispatch();
  CHECK(table.clGetPlatformIDs != nullptr);
  CHECK(table.clGetDeviceIDs != nullptr);
  CHECK(table.clCreateContext != nullptr);
  CHECK(table.clCreateCommandQueueWithProperties != nullptr);
  CHECK(table.clEnqueueNDRangeKernel != nullptr);
  CHECK(table.clEnqueueMapBuffer != nullptr);
  CHECK(table.clCreateImage != nullptr);
  CHECK(table.clCreateSampler != nullptr);
  CHECK(table.clSVMAlloc != nullptr); }

TEST(cl_icd_loader_sequence, a_command_this_icd_does_not_carry_refuses_rather_than_crashing)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  const cl_icd_dispatch &table = rig.root().dispatch();
  CHECK_EQ(table.clGetDeviceAndHostTimer(nullptr, nullptr, nullptr), CL_INVALID_OPERATION);
  CHECK_EQ(table.clEnqueueNativeKernel(nullptr, nullptr, nullptr, 0u, 0u, nullptr, nullptr, 0u, nullptr, nullptr),
           CL_INVALID_OPERATION); }
