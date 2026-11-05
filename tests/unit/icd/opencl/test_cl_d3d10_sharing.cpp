#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "harness/icd/fake_cl_loader.hpp"
#include "icd/opencl/cl_dispatch_table.hpp"
#include "icd/opencl/cl_extension_policy.hpp"
#include "icd/opencl/cl_icd_binding.hpp"
#include "icd/opencl/cl_icd_root.hpp"

#include <cstdint>
#include <cstring>

using av::ClIcdRig;
using av::FakeClLoader;
using av::clicd::ClIcdRoot;

namespace {

constexpr std::uint64_t host_platform = 0x6200u;
constexpr std::uint64_t host_device = 0x6300u;
constexpr std::uint64_t host_context = 0x6400u;
constexpr std::uint64_t host_queue = 0x6500u;

using DeviceIds = cl_int(CL_API_CALL *)(cl_platform_id, cl_uint, void *, cl_uint, cl_uint, cl_device_id *, cl_uint *);
using FromBuffer = cl_mem(CL_API_CALL *)(cl_context, cl_mem_flags, void *, cl_int *);
using FromTexture = cl_mem(CL_API_CALL *)(cl_context, cl_mem_flags, void *, cl_uint, cl_int *);
using Objects = cl_int(CL_API_CALL *)(cl_command_queue, cl_uint, const cl_mem *, cl_uint, const cl_event *, cl_event *);

const cl_icd_dispatch *table_of(const void *object)
{
    return object != nullptr ? *reinterpret_cast<const cl_icd_dispatch *const *>(object) : nullptr;
}

DeviceIds device_ids_of(const cl_icd_dispatch &table)
{
    return reinterpret_cast<DeviceIds>(table.clGetDeviceIDsFromD3D10KHR);
}

cl_device_id opened_device(ClIcdRig &rig)
{
    if (rig.loader().load() != FakeClLoader::Load::kept) return nullptr;
    rig.host().arena_import(0x77u, 3u);
    rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
    rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
    rig.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
    rig.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
    rig.host().cl_info(CL_SUCCESS, "", 1u);
    cl_device_id device = nullptr;
    cl_uint      devices = 0u;
    return rig.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device, &devices) == CL_SUCCESS ? device : nullptr;
}

void context_opens(ClIcdRig &rig)
{
    rig.host().arena_import(0x77u, 3u);
    rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
    rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
    rig.host().cl_handle(host_context, CL_SUCCESS);
}

}

TEST(cl_d3d10_sharing, a_dxgi_adapter_or_d3d10_device_on_the_avgpu_adapter_is_this_device)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);
  if (device == nullptr) return;
  int adapter = 0, d3d10_device = 0;
  rig.d3d_adapters().name_adapter(&adapter, ClIcdRig::adapter_luid);
  rig.d3d_adapters().name_device(&d3d10_device, ClIcdRig::adapter_luid);
  const cl_platform_id platform = rig.loader().platform(0u);
  const DeviceIds      device_ids = device_ids_of(*table_of(platform));
  CHECK(device_ids != nullptr);

  cl_uint      count = 0u;
  CHECK_EQ(device_ids(platform, ClIcdRoot::d3d10_adapter_source, &adapter, ClIcdRoot::d3d10_preferred_devices, 0u,
                      nullptr, &count),
           CL_SUCCESS);
  CHECK_EQ(count, 1u);
  cl_device_id found = nullptr;
  CHECK_EQ(device_ids(platform, ClIcdRoot::d3d10_adapter_source, &adapter, ClIcdRoot::d3d10_preferred_devices, 1u,
                      &found, nullptr),
           CL_SUCCESS);
  CHECK(found == device);

  found = nullptr;
  count = 0u;
  CHECK_EQ(device_ids(platform, ClIcdRoot::d3d10_device_source, &d3d10_device, ClIcdRoot::d3d10_all_devices, 1u,
                      &found, &count),
           CL_SUCCESS);
  CHECK(found == device);
  CHECK_EQ(count, 1u);

  av::clicd::ClExtensionPolicy policy(av::clicd::reported_extensions, av::clicd::reported_extension_count);
  char                         reported[256] = {};
  CHECK(policy.report("", 0u, reported, sizeof reported) > 1u);
  CHECK(std::strstr(reported, "cl_khr_d3d10_sharing") != nullptr);
  CHECK(std::strstr(reported, "cl_khr_gl_sharing") == nullptr);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_d3d10_sharing, an_object_on_another_adapter_or_on_none_finds_no_device)
{ ClIcdRig rig;
  CHECK(opened_device(rig) != nullptr);
  int other = 0, stranger = 0;
  rig.d3d_adapters().name_adapter(&other, ClIcdRig::adapter_luid + 1u);
  const cl_platform_id platform = rig.loader().platform(0u);
  cl_device_id         found = reinterpret_cast<cl_device_id>(&stranger);
  cl_uint              count = 9u;
  CHECK_EQ(rig.root().device_ids_from_d3d10(platform, ClIcdRoot::d3d10_adapter_source, &other,
                                            ClIcdRoot::d3d10_all_devices, 1u, &found, &count),
           CL_DEVICE_NOT_FOUND);
  CHECK_EQ(count, 0u);
  CHECK(found == reinterpret_cast<cl_device_id>(&stranger));
  CHECK_EQ(rig.root().device_ids_from_d3d10(platform, ClIcdRoot::d3d10_device_source, &stranger,
                                            ClIcdRoot::d3d10_all_devices, 1u, &found, nullptr),
           CL_DEVICE_NOT_FOUND);
  CHECK_EQ(rig.d3d_adapters().asked(), 2u); }

TEST(cl_d3d10_sharing, a_device_that_cannot_be_opened_is_found_on_no_adapter)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  int adapter = 0;
  rig.d3d_adapters().name_adapter(&adapter, ClIcdRig::adapter_luid);
  rig.host().cl_ids(CL_INVALID_PLATFORM, &host_platform, 0u, 0u);
  cl_uint count = 9u;
  CHECK_EQ(rig.root().device_ids_from_d3d10(rig.loader().platform(0u), ClIcdRoot::d3d10_adapter_source, &adapter,
                                            ClIcdRoot::d3d10_all_devices, 0u, nullptr, &count),
           CL_DEVICE_NOT_FOUND);
  CHECK_EQ(count, 0u); }

TEST(cl_d3d10_sharing, a_question_the_api_refuses_is_refused_before_dxgi_is_asked)
{ ClIcdRig rig;
  CHECK(opened_device(rig) != nullptr);
  int adapter = 0;
  rig.d3d_adapters().name_adapter(&adapter, ClIcdRig::adapter_luid);
  const cl_platform_id platform = rig.loader().platform(0u);
  cl_device_id         found = nullptr;
  cl_uint              count = 0u;
  CHECK_EQ(rig.root().device_ids_from_d3d10(reinterpret_cast<cl_platform_id>(&adapter), ClIcdRoot::d3d10_adapter_source,
                                            &adapter, ClIcdRoot::d3d10_all_devices, 1u, &found, &count),
           CL_INVALID_PLATFORM);
  CHECK_EQ(rig.root().device_ids_from_d3d10(platform, 0x4015u, &adapter, ClIcdRoot::d3d10_all_devices, 1u, &found,
                                            &count),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().device_ids_from_d3d10(platform, ClIcdRoot::d3d10_adapter_source, &adapter, 0x4011u, 1u, &found,
                                            &count),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().device_ids_from_d3d10(platform, ClIcdRoot::d3d10_adapter_source, &adapter,
                                            ClIcdRoot::d3d10_all_devices, 1u, nullptr, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().device_ids_from_d3d10(platform, ClIcdRoot::d3d10_adapter_source, &adapter,
                                            ClIcdRoot::d3d10_all_devices, 0u, &found, &count),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.d3d_adapters().asked(), 0u);
  CHECK(found == nullptr); }

TEST(cl_d3d10_sharing, a_d3d10_device_to_share_with_is_one_this_device_cannot_and_a_null_one_is_none)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);
  if (device == nullptr) return;
  int d3d10_device = 0;
  const cl_context_properties sharing[] = {
      static_cast<cl_context_properties>(ClIcdRoot::d3d10_context_device),
      reinterpret_cast<cl_context_properties>(&d3d10_device), 0};
  cl_int                      error = CL_SUCCESS;
  const std::uint32_t         sent = rig.kmt().commands();
  CHECK(rig.root().create_context(sharing, &device, 1u, &error) == nullptr);
  CHECK_EQ(error, ClIcdRoot::invalid_d3d10_device);
  CHECK_EQ(rig.kmt().commands(), sent);
  const cl_context_properties none[] = {static_cast<cl_context_properties>(ClIcdRoot::d3d10_context_device), 0, 0};
  context_opens(rig);
  cl_context context = rig.root().create_context(none, &device, 1u, &error);
  CHECK(context != nullptr);
  CHECK_EQ(error, CL_SUCCESS); }

TEST(cl_d3d10_sharing, no_context_here_shares_with_direct3d_10_so_its_objects_are_refused)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);
  if (device == nullptr) return;
  context_opens(rig);
  cl_context context = rig.loader().create_context(device, nullptr);
  CHECK(context != nullptr);
  rig.host().cl_handle(host_queue, CL_SUCCESS);
  cl_command_queue queue = rig.loader().create_queue(context, device, nullptr);
  CHECK(queue != nullptr);
  if (context == nullptr || queue == nullptr) return;
  const cl_icd_dispatch &table = *table_of(context);
  int                    resource = 0;
  cl_int                 error = CL_SUCCESS;
  const std::uint32_t    sent = rig.kmt().commands();
  CHECK(reinterpret_cast<FromBuffer>(table.clCreateFromD3D10BufferKHR)(context, CL_MEM_READ_WRITE, &resource,
                                                                       &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  error = CL_SUCCESS;
  CHECK(reinterpret_cast<FromTexture>(table.clCreateFromD3D10Texture2DKHR)(context, CL_MEM_READ_WRITE, &resource, 0u,
                                                                          &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  error = CL_SUCCESS;
  CHECK(reinterpret_cast<FromTexture>(table.clCreateFromD3D10Texture3DKHR)(context, CL_MEM_READ_WRITE, &resource, 0u,
                                                                          &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  CHECK(reinterpret_cast<FromBuffer>(table.clCreateFromD3D10BufferKHR)(context, CL_MEM_READ_WRITE, &resource,
                                                                       nullptr) == nullptr);
  cl_mem none = nullptr;
  CHECK_EQ(reinterpret_cast<Objects>(table.clEnqueueAcquireD3D10ObjectsKHR)(queue, 1u, &none, 0u, nullptr, nullptr),
           CL_INVALID_CONTEXT);
  CHECK_EQ(reinterpret_cast<Objects>(table.clEnqueueReleaseD3D10ObjectsKHR)(queue, 1u, &none, 0u, nullptr, nullptr),
           CL_INVALID_CONTEXT);
  CHECK_EQ(rig.root().enqueue_d3d10_objects(reinterpret_cast<cl_command_queue>(context)), CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.kmt().commands(), sent); }

TEST(cl_d3d10_sharing, its_entries_reached_before_a_root_exists_refuse)
{ CHECK(!av::clicd::cl_icd_binding.bound());
  av::clicd::ClDispatchTable table;
  table.fill();
  const cl_icd_dispatch &slots = table.table();
  cl_uint                count = 7u;
  CHECK_EQ(device_ids_of(slots)(nullptr, ClIcdRoot::d3d10_adapter_source, nullptr, ClIcdRoot::d3d10_all_devices, 0u,
                                nullptr, &count),
           CL_INVALID_PLATFORM);
  cl_int error = 7;
  CHECK(reinterpret_cast<FromBuffer>(slots.clCreateFromD3D10BufferKHR)(nullptr, 0u, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  error = 7;
  CHECK(reinterpret_cast<FromTexture>(slots.clCreateFromD3D10Texture2DKHR)(nullptr, 0u, nullptr, 0u, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  CHECK(reinterpret_cast<FromBuffer>(slots.clCreateFromD3D10BufferKHR)(nullptr, 0u, nullptr, nullptr) == nullptr);
  CHECK(reinterpret_cast<FromTexture>(slots.clCreateFromD3D10Texture3DKHR)(nullptr, 0u, nullptr, 0u, nullptr) ==
        nullptr);
  CHECK_EQ(reinterpret_cast<Objects>(slots.clEnqueueAcquireD3D10ObjectsKHR)(nullptr, 0u, nullptr, 0u, nullptr,
                                                                            nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(reinterpret_cast<Objects>(slots.clEnqueueReleaseD3D10ObjectsKHR)(nullptr, 0u, nullptr, 0u, nullptr,
                                                                            nullptr),
           CL_INVALID_COMMAND_QUEUE); }

TEST(cl_d3d10_sharing, its_numbers_are_the_specifications)
{ CHECK_EQ(ClIcdRoot::d3d10_device_source, 0x4010u);
  CHECK_EQ(ClIcdRoot::d3d10_adapter_source, 0x4011u);
  CHECK_EQ(ClIcdRoot::d3d10_preferred_devices, 0x4012u);
  CHECK_EQ(ClIcdRoot::d3d10_all_devices, 0x4013u);
  CHECK_EQ(ClIcdRoot::d3d10_context_device, 0x4014u);
  CHECK_EQ(ClIcdRoot::invalid_d3d10_device, -1002); }
