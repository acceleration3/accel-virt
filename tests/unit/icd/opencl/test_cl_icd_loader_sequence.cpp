/* The seven steps the Khronos loader takes before it will keep this driver,
 * driven through a loader that dispatches the way the real one does.
 *
 * Every one of them happens in every process that ever calls clGetPlatformIDs,
 * including processes with no interest in this adapter, which is why the whole
 * sequence has to complete without opening a D3DKMT adapter, device or
 * context. And step seven is the one an ICD cannot decline: a platform whose
 * CL_PLATFORM_ICD_SUFFIX_KHR fails is dropped without a word, taking the
 * driver with it. */
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
  /* Not one D3DKMT call. A driver that opened an adapter here would open one
   * in every process on the machine that ever asks for an OpenCL platform. */
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
  /* Everything else the loader wants it reaches through the dispatch table,
   * so answering a name here would only invite a caller to bypass it. */
  CHECK(rig.root().extension_function("clGetPlatformIDs") == nullptr);
  CHECK(rig.root().extension_function("clIcdGetPlatformIDsKHRx") == nullptr);
  CHECK(rig.root().extension_function("clIcdGetPlatformIDsKH") == nullptr);
  CHECK(rig.root().extension_function("") == nullptr);
  CHECK(rig.root().extension_function(nullptr) == nullptr); }

TEST(cl_icd_loader_sequence, the_platform_carries_the_dispatch_table_in_its_first_word)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  /* This is the whole of the ICD ABI: the loader reads that word and jumps. */
  CHECK(rig.loader().dispatch_of(rig.loader().platform(0u)) == &rig.root().dispatch()); }

TEST(cl_icd_loader_sequence, the_enumeration_answers_the_count_and_then_the_platforms)
{ ClIcdRig rig;
  cl_uint count = 0u;
  CHECK_EQ(rig.root().platform_ids(0u, nullptr, &count), CL_SUCCESS);
  CHECK_EQ(count, 1u);
  cl_platform_id platforms[2] = {};
  CHECK_EQ(rig.root().platform_ids(2u, platforms, nullptr), CL_SUCCESS);
  CHECK(platforms[0] != nullptr);
  /* Asking for neither is the caller's mistake, and so is room for none. */
  CHECK_EQ(rig.root().platform_ids(0u, nullptr, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().platform_ids(0u, platforms, &count), CL_INVALID_VALUE); }

TEST(cl_icd_loader_sequence, the_extensions_are_answered_before_any_device_exists)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  /* Only what every device of the platform carries, which DaVinci Resolve
   * reads before it asks which device a DXGI adapter is (cf5e9ca8) - and the
   * loader reads it either way while it is still deciding. */
  CHECK(std::strstr(rig.loader().extensions(), "cl_khr_d3d11_sharing") != nullptr);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(cl_icd_loader_sequence, the_suffix_is_the_one_query_that_may_not_fail)
{ ClIcdRig rig;
  /* Asked the way the loader asks it: size, then bytes. */
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
  /* The device is the next object the loader is handed, and the application
   * dispatches through it exactly as the loader did through the platform. */
  cl_device_id device = nullptr;
  cl_uint      devices = 0u;
  if (rig.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device, &devices) == CL_SUCCESS) {
      CHECK_EQ(devices, 1u);
      CHECK(rig.loader().dispatch_of(device) == table);
  } }

TEST(cl_icd_loader_sequence, a_dispatch_table_leaves_no_slot_a_carried_command_could_fall_through)
{ ClIcdRig rig;
  const cl_icd_dispatch &table = rig.root().dispatch();
  /* The ones this ICD claims. */
  CHECK(table.clGetPlatformIDs != nullptr);
  CHECK(table.clGetDeviceIDs != nullptr);
  CHECK(table.clCreateContext != nullptr);
  CHECK(table.clCreateCommandQueueWithProperties != nullptr);
  CHECK(table.clEnqueueNDRangeKernel != nullptr);
  /* And ones it does not, which carry the generated refusal rather than
   * nothing: the loader calls through a slot without checking it. */
  CHECK(table.clEnqueueMapBuffer != nullptr);
  CHECK(table.clCreateImage != nullptr);
  CHECK(table.clCreateSampler != nullptr);
  CHECK(table.clSVMAlloc != nullptr); }

TEST(cl_icd_loader_sequence, a_command_this_icd_does_not_carry_refuses_rather_than_crashing)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  const cl_icd_dispatch &table = rig.root().dispatch();
  /* An application asking for something this driver does not implement is
   * told so, and the process survives it. */
  CHECK_EQ(table.clGetDeviceAndHostTimer(nullptr, nullptr, nullptr), CL_INVALID_OPERATION);
  cl_int error = CL_SUCCESS;
  CHECK(table.clCreatePipe(nullptr, 0u, 4u, 16u, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_OPERATION); }
