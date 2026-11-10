#include "core/trace/state_writer.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/kmt/memory_segment_group.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/kmt/pci_location.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "icd/opencl/cl_property_list.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"
#include "proto/submission/command_header.hpp"
#include "proto/submission/fault_code.hpp"
#include "proto/wire/wire_reader.hpp"

#include <CL/cl_ext.h>

#include <cstring>
#include <initializer_list>

using av::AvgpuAdapters;
using av::ClIcdRig;
using av::FakeClLoader;
using av::MockKmt;
using av::clicd::ClObject;
using av::clicd::ClPropertyList;

namespace {
constexpr std::uint64_t host_platform = 0x6200u;
constexpr std::uint64_t host_device = 0x6300u;
constexpr std::uint64_t host_context = 0x6400u;
constexpr std::uint64_t host_queue = 0x6500u;
constexpr std::uint64_t host_buffer = 0x6600u;
constexpr std::uint64_t host_program = 0x6700u;
constexpr std::uint64_t host_kernel = 0x6800u;

void device_opens(ClIcdRig &rig)
{
    rig.host().arena_import(0x77u, 3u);
    rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
    rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
    rig.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
    rig.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
    rig.host().cl_info(CL_SUCCESS, "", 1u);
}

cl_device_id opened_device(ClIcdRig &rig)
{
    if (rig.loader().load() != FakeClLoader::Load::kept) return nullptr;
    device_opens(rig);
    cl_device_id device = nullptr;
    cl_uint      devices = 0u;
    if (rig.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device, &devices) != CL_SUCCESS) {
        return nullptr;
    }
    return device;
}

}

TEST(cl_icd_root, a_new_root_is_bound_and_has_opened_nothing)
{ ClIcdRig rig;
  CHECK(rig.root().invariants_hold());
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  cl_uint count = 0u;
  CHECK_EQ(rig.root().platform_ids(0u, nullptr, &count), CL_SUCCESS);
  CHECK_EQ(count, 1u); }

TEST(cl_icd_root, asking_for_a_device_is_what_opens_the_adapter)
{ ClIcdRig rig;
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::enumerate_adapters), 0u);
  CHECK(opened_device(rig) != nullptr);
  CHECK(rig.kmt().calls(MockKmt::Call::enumerate_adapters) > 0u);
  CHECK(rig.kmt().open_objects() > 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_device_type_this_adapter_is_not_finds_nothing)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  cl_device_id device = nullptr;
  cl_uint      devices = 1u;
  CHECK_EQ(rig.loader().device_ids(CL_DEVICE_TYPE_CPU, 1u, &device, &devices), CL_DEVICE_NOT_FOUND);
  CHECK_EQ(devices, 0u);
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  CHECK_EQ(rig.loader().device_ids(0u, 1u, &device, &devices), CL_INVALID_DEVICE_TYPE); }

TEST(cl_icd_root, the_device_answers_the_adapters_identity_and_the_hosts_everything_else)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);

  cl_uint vendor = 0u;
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_VENDOR_ID, sizeof vendor, &vendor, nullptr),
           CL_SUCCESS);
  CHECK(vendor != 0u);
  std::uint8_t luid[CL_LUID_SIZE_KHR] = {};
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_LUID_KHR, sizeof luid, luid, nullptr),
           CL_SUCCESS);
  CHECK(luid[0] != 0u || luid[1] != 0u || luid[2] != 0u || luid[3] != 0u);
  CHECK(luid[4] != 0u || luid[5] != 0u || luid[6] != 0u || luid[7] != 0u);

  rig.host().cl_info(CL_SUCCESS, "a host gpu", 11u);
  char name[32] = {};
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_NAME, sizeof name, name, nullptr), CL_SUCCESS);
  CHECK_EQ(std::strcmp(name, "a host gpu"), 0); }

TEST(cl_icd_root, a_devices_platform_is_this_icds_and_it_has_no_parent)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);
  cl_platform_id ours = nullptr;
  CHECK_EQ(rig.root().platform_ids(1u, &ours, nullptr), CL_SUCCESS);
  const char host_handle[8] = {0, 0, 0, 0x7f, 0, 0, 0x7f, 0};
  rig.host().cl_info(CL_SUCCESS, host_handle, sizeof host_handle);
  cl_platform_id platform = nullptr;
  std::size_t    bytes = 0u;
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_PLATFORM, sizeof platform, &platform, &bytes), CL_SUCCESS);
  CHECK(platform == ours);
  CHECK_EQ(bytes, sizeof platform);
  cl_device_id parent = device;
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_PARENT_DEVICE, sizeof parent, &parent, nullptr), CL_SUCCESS);
  CHECK(parent == nullptr);
  char cramped[4] = {};
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_PLATFORM, sizeof cramped, cramped, nullptr), CL_INVALID_VALUE); }

TEST(cl_icd_root, the_extensions_are_the_policy_intersected_with_the_host_not_the_host_s_own)
{
  static const char plain[] = "cl_khr_spir cl_khr_spirv_queries cl_khr_spirv cl_khr_gl_shar";
  char              reported[256];
  std::size_t       answered = 0u;
  { ClIcdRig rig;
    cl_device_id device = opened_device(rig);
    CHECK(device != nullptr);
    rig.host().cl_info(CL_SUCCESS, plain, sizeof plain);
    std::memset(reported, 'x', sizeof reported);
    CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXTENSIONS, sizeof reported, reported, &answered),
             CL_SUCCESS);
    CHECK_EQ(std::strcmp(reported, "cl_khr_icd cl_khr_d3d10_sharing cl_khr_d3d11_sharing cl_khr_pci_bus_info"), 0);
    CHECK_EQ(answered, std::strlen("cl_khr_icd cl_khr_d3d10_sharing cl_khr_d3d11_sharing cl_khr_pci_bus_info") + 1u);
    CHECK(std::strstr(plain, "cl_khr_spir") != nullptr);

    const std::uint32_t sent = rig.kmt().commands();
    std::size_t         wanted = 0u;
    CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXTENSIONS, 0u, nullptr, &wanted), CL_SUCCESS);
    CHECK_EQ(wanted, std::strlen("cl_khr_icd cl_khr_d3d10_sharing cl_khr_d3d11_sharing cl_khr_pci_bus_info") + 1u);
    char cramped[1] = {'x'};
    CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXTENSIONS, 0u, cramped, nullptr), CL_INVALID_VALUE);
    CHECK_EQ(cramped[0], 'x');
    CHECK_EQ(rig.kmt().commands(), sent); }

  { ClIcdRig rig;
    cl_device_id device = opened_device(rig);
    static const char sharing[] = "cl_khr_spir cl_khr_gl_sharing cl_khr_il cl_khr_external_memory";
    rig.host().cl_info(CL_SUCCESS, sharing, sizeof sharing);
    std::memset(reported, 'x', sizeof reported);
    CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXTENSIONS, sizeof reported, reported, &answered),
             CL_SUCCESS);
    CHECK_EQ(std::strcmp(reported,
                         "cl_khr_icd cl_khr_external_memory cl_khr_external_memory_win32 cl_khr_gl_sharing cl_khr_d3d10_sharing cl_khr_d3d11_sharing "
                         "cl_khr_pci_bus_info"),
             0);
    CHECK_EQ(answered, std::strlen(reported) + 1u); }

  { ClIcdRig rig;
    cl_device_id device = opened_device(rig);
    static const char copies[] = "cl_khr_gl_sharing cl_khr_external_memory_opaque_fd";
    rig.host().cl_info(CL_SUCCESS, copies, sizeof copies);
    std::memset(reported, 'x', sizeof reported);
    CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXTENSIONS, sizeof reported, reported, &answered),
             CL_SUCCESS);
    CHECK_EQ(std::strcmp(reported, "cl_khr_icd cl_khr_gl_sharing cl_khr_d3d10_sharing cl_khr_d3d11_sharing cl_khr_pci_bus_info"),
             0); }

  { ClIcdRig rig;
    cl_device_id device = opened_device(rig);
    rig.host().cl_info(CL_OUT_OF_RESOURCES, nullptr, 0u);
    CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXTENSIONS, sizeof reported, reported, nullptr),
             CL_OUT_OF_RESOURCES);
    rig.host().cl_info(CL_SUCCESS, plain, sizeof plain);
    CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXTENSIONS, sizeof reported, reported, nullptr),
             CL_SUCCESS);
    CHECK_EQ(rig.kmt().queued_answers(), 0u); } }

TEST(cl_icd_root, the_import_handle_types_are_this_drivers_own_and_only_where_it_reports_them)
{ { ClIcdRig rig;
    cl_device_id device = opened_device(rig);
    CHECK(device != nullptr);
    static const char importing[] = "cl_khr_external_memory";
    rig.host().cl_info(CL_SUCCESS, importing, sizeof importing);
    cl_external_memory_handle_type_khr taken = 0u;
    std::size_t                        answered = 0u;
    CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR, sizeof taken,
                                      &taken, &answered),
             CL_SUCCESS);
    CHECK_EQ(taken, static_cast<cl_external_memory_handle_type_khr>(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR));
    CHECK_EQ(answered, sizeof taken);

    const std::uint32_t sent = rig.kmt().commands();
    std::size_t         wanted = 0u;
    CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR, 0u, nullptr,
                                      &wanted),
             CL_SUCCESS);
    CHECK_EQ(wanted, sizeof taken);
    char cramped[1] = {'x'};
    CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR, 1u, cramped,
                                      nullptr),
             CL_INVALID_VALUE);
    CHECK_EQ(cramped[0], 'x');
    CHECK_EQ(rig.kmt().commands(), sent); }

  cl_external_memory_handle_type_khr taken = 0u;
  { ClIcdRig rig;
    cl_device_id device = opened_device(rig);
    static const char plain[] = "cl_khr_fp64 cl_khr_gl_sharing";
    rig.host().cl_info(CL_SUCCESS, plain, sizeof plain);
    CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR, sizeof taken,
                                      &taken, nullptr),
             CL_INVALID_VALUE); }
  { ClIcdRig rig;
    cl_device_id device = opened_device(rig);
    rig.host().cl_info(CL_OUT_OF_RESOURCES, nullptr, 0u);
    CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR, sizeof taken,
                                      &taken, nullptr),
             CL_OUT_OF_RESOURCES); } }

TEST(cl_icd_root, a_device_property_is_asked_of_the_host_once)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);
  const cl_uint units = 64u;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&units), sizeof units);
  const std::uint32_t sent = rig.kmt().commands();
  std::size_t         wanted = 0u;
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_MAX_COMPUTE_UNITS, 0u, nullptr, &wanted), CL_SUCCESS);
  CHECK_EQ(wanted, sizeof units);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  for (int again = 0; again < 3; ++again) {
      cl_uint got = 0u;
      CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof got, &got, nullptr),
               CL_SUCCESS);
      CHECK_EQ(got, units);
  }
  CHECK_EQ(rig.kmt().commands(), sent + 1u);

  rig.host().cl_info(CL_INVALID_VALUE, nullptr, 0u);
  rig.host().cl_info(CL_INVALID_VALUE, nullptr, 0u);
  cl_uint unknown = 0u;
  CHECK_EQ(rig.loader().device_info(device, 0x7FFFu, sizeof unknown, &unknown, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent + 3u);
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&units), sizeof units);
  CHECK_EQ(rig.loader().device_info(device, 0x7FFFu, sizeof unknown, &unknown, nullptr), CL_SUCCESS);
  CHECK_EQ(unknown, units);
  CHECK_EQ(rig.kmt().queued_answers(), 0u); }

TEST(cl_icd_root, a_device_pointer_the_icd_never_made_is_an_invalid_device)
{ ClIcdRig rig;
  CHECK(opened_device(rig) != nullptr);
  int          elsewhere = 0;
  char         name[8] = {};
  CHECK_EQ(rig.root().device_info(reinterpret_cast<cl_device_id>(&elsewhere), CL_DEVICE_NAME,
                                  sizeof name, name, nullptr),
           CL_INVALID_DEVICE);
  CHECK_EQ(rig.root().device_info(nullptr, CL_DEVICE_NAME, sizeof name, name, nullptr),
           CL_INVALID_DEVICE); }

TEST(cl_icd_root, a_context_is_made_on_the_device_and_carries_the_dispatch_table)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);

  rig.host().arena_import(0x77u, 3u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(host_context, CL_SUCCESS);
  cl_int     error = CL_INVALID_VALUE;
  cl_context context = rig.loader().create_context(device, &error);
  CHECK(context != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(rig.loader().dispatch_of(context) == &rig.root().dispatch());
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_host_with_no_session_left_finds_no_device_and_says_out_of_resources_never_not_found)
{ std::uint32_t refused = 0u;
  for (std::uint32_t after = 0u; after < 6u; after++) {
      ClIcdRig rig;
      CHECK(rig.loader().load() == FakeClLoader::Load::kept);
      device_opens(rig);
      rig.kmt().time_out_faulted_with(static_cast<std::uint32_t>(av::FaultCode::no_executor), after);
      cl_device_id device = nullptr;
      cl_uint      devices = 7u;
      const cl_int answer = rig.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device, &devices);
      if (answer == CL_SUCCESS) continue;
      refused++;
      CHECK_EQ(answer, CL_OUT_OF_RESOURCES);
      CHECK_EQ(devices, 0u);
      CHECK(rig.root().invariants_hold());
  }
  CHECK(refused >= 1u); }

TEST(cl_icd_root, a_host_with_no_session_left_refuses_a_context_with_out_of_resources)
{ std::uint32_t refused = 0u;
  for (std::uint32_t after = 0u; after < 8u; after++) {
      ClIcdRig rig;
      cl_device_id device = opened_device(rig);
      CHECK(device != nullptr);
      rig.host().arena_import(0x77u, 3u);
      rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
      rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
      rig.host().cl_handle(host_context, CL_SUCCESS);
      rig.kmt().time_out_faulted_with(static_cast<std::uint32_t>(av::FaultCode::no_executor), after);
      cl_int     error = CL_SUCCESS;
      cl_context context = rig.loader().create_context(device, &error);
      if (context != nullptr) continue;
      refused++;
      CHECK_EQ(error, CL_OUT_OF_RESOURCES);
      CHECK(rig.root().invariants_hold());
  }
  CHECK(refused >= 1u); }

TEST(cl_icd_root, a_context_made_from_something_that_is_not_a_device_is_refused)
{ ClIcdRig rig;
  CHECK(opened_device(rig) != nullptr);
  void  *not_an_object = nullptr;
  cl_int error = CL_SUCCESS;
  CHECK(rig.loader().create_context(reinterpret_cast<cl_device_id>(&not_an_object), &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_DEVICE);
  const cl_device_id none = nullptr;
  error = CL_SUCCESS;
  CHECK(rig.root().create_context(nullptr, &none, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_DEVICE);
  error = CL_SUCCESS;
  CHECK(rig.root().create_context(nullptr, nullptr, 0u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE); }

TEST(cl_icd_root, retaining_a_context_keeps_it_until_as_many_releases)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  rig.host().arena_import(0x77u, 3u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(host_context, CL_SUCCESS);
  cl_context context = rig.loader().create_context(device, nullptr);
  CHECK(context != nullptr);

  CHECK_EQ(rig.root().retain_context(context), CL_SUCCESS);
  CHECK_EQ(rig.root().release_context(context), CL_SUCCESS);
  CHECK_EQ(rig.root().retain_context(context), CL_SUCCESS);
  CHECK_EQ(rig.root().release_context(context), CL_SUCCESS);
  CHECK_EQ(rig.root().release_context(context), CL_SUCCESS);
  CHECK_EQ(rig.root().release_context(context), CL_INVALID_CONTEXT);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, every_kind_has_its_own_refusal_rather_than_one_for_all_of_them)
{ ClIcdRig rig;
  int elsewhere = 0;
  void *const nothing = &elsewhere;
  CHECK_EQ(rig.root().retain(nothing, ClObject::Kind::context), CL_INVALID_CONTEXT);
  CHECK_EQ(rig.root().retain(nothing, ClObject::Kind::command_queue), CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().retain(nothing, ClObject::Kind::mem), CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().retain(nothing, ClObject::Kind::program), CL_INVALID_PROGRAM);
  CHECK_EQ(rig.root().retain(nothing, ClObject::Kind::kernel), CL_INVALID_KERNEL);
  CHECK_EQ(rig.root().retain(nothing, ClObject::Kind::event), CL_INVALID_EVENT);
  CHECK_EQ(rig.root().release(nothing, ClObject::Kind::mem), CL_INVALID_MEM_OBJECT); }

TEST(cl_icd_root, a_queue_a_buffer_a_program_and_a_kernel_are_objects_of_their_context)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  rig.host().arena_import(0x77u, 3u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(host_context, CL_SUCCESS);
  cl_context context = rig.loader().create_context(device, nullptr);
  CHECK(context != nullptr);

  rig.host().cl_handle(host_queue, CL_SUCCESS);
  cl_command_queue queue = rig.loader().create_queue(context, device, nullptr);
  CHECK(queue != nullptr);
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  cl_int error = CL_INVALID_VALUE;
  cl_mem buffer = rig.loader().create_buffer(context, CL_MEM_READ_WRITE, 256u, &error);
  CHECK(buffer != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  rig.host().cl_handle(host_program, CL_SUCCESS);
  cl_program program = rig.loader().create_program(context, "__kernel void nothing(void) {}", nullptr);
  CHECK(program != nullptr);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.loader().build_program(program, nullptr), CL_SUCCESS);
  rig.host().cl_handle(host_kernel, CL_SUCCESS);
  cl_kernel kernel = rig.loader().create_kernel(program, "nothing", nullptr);
  CHECK(kernel != nullptr);

  CHECK(rig.loader().dispatch_of(queue) == &rig.root().dispatch());
  CHECK(rig.loader().dispatch_of(buffer) == &rig.root().dispatch());
  CHECK(rig.loader().dispatch_of(kernel) == &rig.root().dispatch());
  CHECK(rig.root().invariants_hold());

  CHECK_EQ(rig.root().release_context(context), CL_SUCCESS);
  CHECK_EQ(rig.root().release_context(context), CL_INVALID_CONTEXT);
  cl_uint count = 0u;
  CHECK_EQ(rig.root().context_info(context, CL_CONTEXT_NUM_DEVICES, sizeof count, &count, nullptr),
           CL_INVALID_CONTEXT);
  CHECK_EQ(rig.root().set_kernel_argument(kernel, 0u, sizeof buffer, &buffer), CL_SUCCESS);
  CHECK_EQ(rig.root().release(queue, ClObject::Kind::command_queue), CL_SUCCESS);
  CHECK_EQ(rig.root().release(kernel, ClObject::Kind::kernel), CL_SUCCESS);
  CHECK_EQ(rig.root().release(program, ClObject::Kind::program), CL_SUCCESS);
  CHECK_EQ(rig.root().release(buffer, ClObject::Kind::mem), CL_SUCCESS);
  CHECK_EQ(rig.root().release(kernel, ClObject::Kind::kernel), CL_INVALID_KERNEL);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_flush_publishes_the_batch_it_queued)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  rig.host().arena_import(0x77u, 3u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(host_context, CL_SUCCESS);
  cl_context context = rig.loader().create_context(device, nullptr);
  rig.host().cl_handle(host_queue, CL_SUCCESS);
  cl_command_queue queue = rig.loader().create_queue(context, device, nullptr);
  CHECK(queue != nullptr);
  const std::uint32_t before = rig.kmt().commands();
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().flush(queue), CL_SUCCESS);
  bool published = false;
  for (std::uint32_t index = before; index < rig.kmt().commands(); index++)
      published = published || rig.kmt().command_id(index) == av::clproto::kCmd_clFlush;
  CHECK(published);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_buffer_of_no_bytes_is_refused_before_the_host_is_asked)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  rig.host().arena_import(0x77u, 3u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(host_context, CL_SUCCESS);
  cl_context context = rig.loader().create_context(device, nullptr);
  const std::uint32_t before = rig.kmt().commands();
  cl_int              error = CL_SUCCESS;
  CHECK(rig.loader().create_buffer(context, CL_MEM_READ_WRITE, 0u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_BUFFER_SIZE);
  CHECK_EQ(rig.kmt().commands(), before); }

TEST(cl_icd_root, a_queue_made_on_something_that_is_not_a_context_is_refused)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  int          elsewhere = 0;
  cl_int       error = CL_SUCCESS;
  CHECK(rig.root().create_queue(reinterpret_cast<cl_context>(&elsewhere), device, nullptr,
                                &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT); }

TEST(cl_icd_root, an_object_of_this_icd_is_never_confused_with_one_of_another_kind)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);
  CHECK_EQ(rig.root().retain(device, ClObject::Kind::context), CL_INVALID_CONTEXT);
  CHECK_EQ(rig.root().retain(device, ClObject::Kind::mem), CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().retain(device, ClObject::Kind::device), CL_SUCCESS);
  CHECK_EQ(rig.root().release(device, ClObject::Kind::device), CL_SUCCESS); }

TEST(cl_icd_root, a_program_built_from_nothing_is_refused_and_a_kernel_needs_a_name)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  rig.host().arena_import(0x77u, 3u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(host_context, CL_SUCCESS);
  cl_context context = rig.loader().create_context(device, nullptr);
  cl_int     error = CL_SUCCESS;
  CHECK(rig.root().create_program(context, 0u, nullptr, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  rig.host().cl_handle(host_program, CL_SUCCESS);
  cl_program program = rig.loader().create_program(context, "__kernel void k(void) {}", nullptr);
  CHECK(program != nullptr);
  error = CL_SUCCESS;
  CHECK(rig.root().create_kernel(program, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE); }

TEST(cl_icd_root, enqueues_and_waits_are_refused_for_anything_that_is_not_their_object)
{ ClIcdRig rig;
  int    elsewhere = 0;
  void  *nothing = &elsewhere;
  const std::size_t global = 64u;
  CHECK_EQ(rig.root().enqueue_kernel(static_cast<cl_command_queue>(nothing),
                                     static_cast<cl_kernel>(nothing), 1u, nullptr, &global, nullptr,
                                     nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().enqueue_read(static_cast<cl_command_queue>(nothing),
                                   static_cast<cl_mem>(nothing), CL_TRUE, 0u, 4u, &elsewhere,
                                   nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().enqueue_write(static_cast<cl_command_queue>(nothing),
                                    static_cast<cl_mem>(nothing), CL_TRUE, 0u, 4u, &elsewhere,
                                    nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().finish(static_cast<cl_command_queue>(nothing)), CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().flush(static_cast<cl_command_queue>(nothing)), CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().set_kernel_argument(static_cast<cl_kernel>(nothing), 0u, 4u, &elsewhere),
           CL_INVALID_KERNEL);
  CHECK_EQ(rig.root().build_program(static_cast<cl_program>(nothing), 0u, nullptr, nullptr), CL_INVALID_PROGRAM); }

TEST(cl_icd_root, a_state_dump_says_what_the_process_holds)
{ ClIcdRig rig;
  CHECK(opened_device(rig) != nullptr);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, svm_alloc_hands_back_a_mirror_and_svm_free_takes_it_back)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  rig.host().arena_import(0x77u, 3u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(host_context, CL_SUCCESS);
  cl_context context = rig.loader().create_context(device, nullptr);
  CHECK(context != nullptr);

  constexpr std::uint64_t host_svm = 0x0000'7F22'0000'3000ull;
  rig.host().cl_handle(host_svm, CL_SUCCESS);
  void *const mirror = rig.root().svm_alloc(context, 0u, 256u, 0u);
  CHECK(mirror != nullptr);
  CHECK(mirror != reinterpret_cast<void *>(host_svm));
  unsigned char *const bytes = static_cast<unsigned char *>(mirror);
  bytes[0] = 0x5Au;
  bytes[255] = 0xA3u;
  CHECK_EQ(bytes[0], 0x5Au);
  CHECK_EQ(bytes[255], 0xA3u);

  rig.host().cl_result(CL_SUCCESS);
  rig.root().svm_free(context, mirror);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, an_allocation_of_nothing_or_on_something_that_is_not_a_context_answers_nothing)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  rig.host().arena_import(0x77u, 3u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(host_context, CL_SUCCESS);
  cl_context context = rig.loader().create_context(device, nullptr);
  CHECK(rig.root().svm_alloc(context, 0u, 0u, 0u) == nullptr);
  void *not_an_object = nullptr;
  CHECK(rig.root().svm_alloc(reinterpret_cast<cl_context>(&not_an_object), 0u, 64u, 0u) == nullptr);
  int elsewhere = 0;
  rig.root().svm_free(context, &elsewhere);
  rig.root().svm_free(context, nullptr);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_kernel_argument_is_translated_and_an_invented_address_never_reaches_the_wire)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  rig.host().arena_import(0x77u, 3u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(host_context, CL_SUCCESS);
  cl_context context = rig.loader().create_context(device, nullptr);
  rig.host().cl_handle(host_program, CL_SUCCESS);
  cl_program program = rig.loader().create_program(context, "__kernel void k(void) {}", nullptr);
  rig.host().cl_handle(host_kernel, CL_SUCCESS);
  cl_kernel kernel = rig.loader().create_kernel(program, "k", nullptr);
  CHECK(kernel != nullptr);

  constexpr std::uint64_t host_svm = 0x0000'7F22'0000'4000ull;
  rig.host().cl_handle(host_svm, CL_SUCCESS);
  unsigned char *const mirror =
      static_cast<unsigned char *>(rig.root().svm_alloc(context, 0u, 256u, 0u));
  CHECK(mirror != nullptr);

  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().set_kernel_svm_argument(kernel, 0u, mirror + 64u), CL_SUCCESS);

  const std::uint32_t before = rig.kmt().commands();
  int                 elsewhere = 0;
  CHECK_EQ(rig.root().set_kernel_svm_argument(kernel, 0u, &elsewhere), CL_INVALID_ARG_VALUE);
  CHECK_EQ(rig.root().set_kernel_svm_argument(kernel, 0u, mirror + 256u), CL_INVALID_ARG_VALUE);
  CHECK_EQ(rig.root().set_kernel_svm_argument(kernel, 0u, nullptr), CL_INVALID_ARG_VALUE);
  CHECK_EQ(rig.kmt().commands(), before);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, mapping_and_unmapping_shared_memory_is_refused_for_an_address_of_no_allocation)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  rig.host().arena_import(0x77u, 3u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(host_context, CL_SUCCESS);
  cl_context context = rig.loader().create_context(device, nullptr);
  rig.host().cl_handle(host_queue, CL_SUCCESS);
  cl_command_queue queue = rig.loader().create_queue(context, device, nullptr);
  CHECK(queue != nullptr);

  int elsewhere = 0;
  CHECK_EQ(rig.root().svm_map(queue, CL_TRUE, CL_MAP_READ, &elsewhere, 4u, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.root().svm_unmap(queue, &elsewhere, nullptr), CL_INVALID_VALUE);
  void *not_an_object = nullptr;
  CHECK_EQ(rig.root().svm_map(reinterpret_cast<cl_command_queue>(&not_an_object), CL_TRUE,
                              CL_MAP_READ, &elsewhere, 4u, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(rig.root().svm_unmap(reinterpret_cast<cl_command_queue>(&not_an_object), &elsewhere,
                                nullptr),
           CL_INVALID_COMMAND_QUEUE); }

TEST(cl_icd_root, unmapping_a_buffer_pointer_no_mapping_answered_is_refused)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  rig.host().arena_import(0x77u, 3u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(host_context, CL_SUCCESS);
  cl_context context = rig.loader().create_context(device, nullptr);
  rig.host().cl_handle(host_queue, CL_SUCCESS);
  cl_command_queue queue = rig.loader().create_queue(context, device, nullptr);
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  cl_mem buffer = rig.loader().create_buffer(context, CL_MEM_READ_WRITE, 256u, nullptr);
  CHECK(buffer != nullptr);
  int elsewhere = 0;
  CHECK_EQ(rig.root().unmap(queue, buffer, &elsewhere, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().unmap(queue, buffer, nullptr, nullptr), CL_INVALID_VALUE); }

namespace av::clicd {
struct ClIcdRootProbe {
    static ClPlatform        &platform(ClIcdRoot &root) { return root.platform_; }
    static ClObjectTable     &objects_of(ClIcdRoot &root) { return root.objects_; }
    static ClDevice &first_device(ClIcdRoot &root)
    { root.learn_devices();
      return root.devices_.at(0u); }
    static ClExtensionPolicy &extensions(ClIcdRoot &root) { return first_device(root).extensions(); }
    static icd::IcdConnection &connection(ClIcdRoot &root) { return first_device(root).connection(); }
    static ClDeviceReport    &device(ClIcdRoot &root) { return first_device(root).decided(); }
    static ClPendingReads    &reads_of(ClIcdRoot &root) { return root.reads_; }
    static ClEventWatcher    &watcher(ClIcdRoot &root) { return root.watcher_; }

    static std::uint32_t live_of(ClIcdRoot &root, ClObject::Kind kind)
    { return root.objects_.live_of(kind); }
    static ClObject *made(ClIcdRoot &root, const void *pointer, ClObject::Kind kind)
    { return root.objects_.made(pointer, kind); }
    static std::uint32_t outstanding_reads(ClIcdRoot &root) { return root.reads_.outstanding(); }
    static ClObject *without_a_context(ClIcdRoot &root, ClObject::Kind kind, std::uint64_t wire)
    { ClObject *made = nullptr;
      return root.objects_.make(kind, wire, nullptr, nullptr, made) == Status::ok ? made
                                                                                  : nullptr; }
};

}

using av::clicd::ClIcdRootProbe;

namespace {
void nothing_ran(cl_event, cl_int, void *) {}

}

namespace {

constexpr std::uint64_t host_event = 0x6900u;

struct Callback {
    std::uint32_t times = 0u;
    cl_int        status = CL_QUEUED;
    cl_event      event = nullptr;
    void         *user_data = nullptr;
};

Callback callback;

void CL_CALLBACK note(cl_event event, cl_int status, void *user_data)
{
    ++callback.times;
    callback.status = status;
    callback.event = event;
    callback.user_data = user_data;
}

cl_context opened_context(ClIcdRig &rig, cl_device_id &device)
{
    device = opened_device(rig);
    if (device == nullptr) return nullptr;
    rig.host().arena_import(0x77u, 3u);
    rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
    rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
    rig.host().cl_handle(host_context, CL_SUCCESS);
    cl_int error = CL_INVALID_VALUE;
    return rig.loader().create_context(device, &error);
}

cl_event made_user_event(ClIcdRig &rig, cl_context context)
{
    rig.host().cl_handle(host_event, CL_SUCCESS);
    cl_int error = CL_INVALID_VALUE;
    cl_event event = rig.loader().create_user_event(context, &error);
    return error == CL_SUCCESS ? event : nullptr;
}

}

TEST(cl_icd_root, a_user_event_is_an_object_of_its_context_and_carries_the_table)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);

  cl_event event = made_user_event(rig, context);
  CHECK(event != nullptr);
  CHECK(rig.loader().dispatch_of(event) == &rig.root().dispatch());
  CHECK_EQ(ClIcdRootProbe::live_of(rig.root(), ClObject::Kind::event), 1u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_user_event_the_host_refuses_is_not_wrapped)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);

  rig.host().cl_handle(0u, CL_OUT_OF_HOST_MEMORY);
  cl_int error = CL_SUCCESS;
  CHECK(rig.loader().create_user_event(context, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);
  CHECK_EQ(ClIcdRootProbe::live_of(rig.root(), ClObject::Kind::event), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_user_event_made_on_something_that_is_not_a_context_is_refused)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);

  cl_int error = CL_SUCCESS;
  CHECK(rig.root().create_user_event(reinterpret_cast<cl_context>(device), &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  CHECK(rig.root().create_user_event(nullptr, nullptr) == nullptr);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, completing_a_user_event_is_sent_and_a_pointer_of_no_event_is_not)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  cl_event event = made_user_event(rig, context);
  CHECK(event != nullptr);

  const std::uint32_t before = rig.kmt().commands();
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.loader().set_user_event_status(event, CL_COMPLETE), CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), before + 1u);

  CHECK_EQ(rig.root().set_user_event_status(reinterpret_cast<cl_event>(context), CL_COMPLETE),
           CL_INVALID_EVENT);
  CHECK_EQ(rig.kmt().commands(), before + 1u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_wait_asks_what_completed_and_never_asks_the_host_to_block)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  cl_event first = made_user_event(rig, context);
  cl_event second = made_user_event(rig, context);
  CHECK(first != nullptr);
  CHECK(second != nullptr);

  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  const cl_event both[] = {first, second};
  const std::uint32_t before = rig.kmt().commands();
  CHECK_EQ(rig.loader().wait_for_events(2u, both), CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), before + 2u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_wait_ends_on_the_first_event_that_failed_and_asks_no_further)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  cl_event first = made_user_event(rig, context);
  cl_event second = made_user_event(rig, context);
  CHECK(first != nullptr);
  CHECK(second != nullptr);

  const cl_int failed = CL_INVALID_VALUE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&failed), sizeof failed);
  const cl_event both[] = {first, second};
  const std::uint32_t before = rig.kmt().commands();
  CHECK_EQ(rig.loader().wait_for_events(2u, both),
           CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST);
  CHECK_EQ(rig.kmt().commands(), before + 1u);

  rig.host().cl_info(CL_OUT_OF_RESOURCES, nullptr, 0u);
  CHECK_EQ(rig.loader().wait_for_events(1u, both), CL_OUT_OF_RESOURCES);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_wait_of_nothing_or_of_something_that_is_not_an_event_asks_the_host_nothing)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  cl_event event = made_user_event(rig, context);
  CHECK(event != nullptr);

  const std::uint32_t before = rig.kmt().commands();
  CHECK_EQ(rig.root().wait_for_events(0u, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().wait_for_events(1u, nullptr), CL_INVALID_VALUE);
  const cl_event mixed[] = {event, reinterpret_cast<cl_event>(context)};
  CHECK_EQ(rig.root().wait_for_events(2u, mixed), CL_INVALID_EVENT);
  cl_event many[av::clicd::ClHostNumbers::kept_inline + 1u] = {};
  for (cl_uint index = 0u; index < av::clicd::ClHostNumbers::kept_inline + 1u; ++index)
      many[index] = event;
  rig.platform().fail_allocation_at(1u);
  CHECK_EQ(rig.root().wait_for_events(av::clicd::ClHostNumbers::kept_inline + 1u, many),
           CL_OUT_OF_HOST_MEMORY);
  CHECK_EQ(rig.kmt().commands(), before);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, an_events_status_is_the_hosts_and_is_asked_for_over_its_own_context)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  cl_event event = made_user_event(rig, context);
  CHECK(event != nullptr);

  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  cl_int      status = CL_QUEUED;
  std::size_t answered = 0u;
  CHECK_EQ(rig.loader().event_info(event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status,
                                   &status, &answered),
           CL_SUCCESS);
  CHECK_EQ(status, CL_COMPLETE);
  CHECK_EQ(answered, sizeof status);
  CHECK_EQ(rig.root().event_info(reinterpret_cast<cl_event>(context),
                                 CL_EVENT_COMMAND_EXECUTION_STATUS, 0u, nullptr, nullptr),
           CL_INVALID_EVENT);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_callback_is_registered_with_the_host_and_watched_here)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  cl_event event = made_user_event(rig, context);
  CHECK(event != nullptr);

  callback = Callback();
  int user_data = 0;
  CHECK(!rig.watcher().running());
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.loader().set_event_callback(event, CL_COMPLETE, &note, &user_data), CL_SUCCESS);
  CHECK(rig.watcher().running());

  const cl_int queued = CL_QUEUED;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&queued), sizeof queued);
  CHECK(rig.watcher().step());
  CHECK_EQ(callback.times, 0u);

  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  CHECK(rig.watcher().step());
  CHECK_EQ(callback.times, 1u);
  CHECK_EQ(callback.status, CL_COMPLETE);
  CHECK(callback.event == event);
  CHECK(callback.user_data == &user_data);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_callback_still_runs_on_an_event_the_application_released_first)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  cl_event event = made_user_event(rig, context);
  CHECK(event != nullptr);

  callback = Callback();
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.loader().set_event_callback(event, CL_COMPLETE, &note, nullptr), CL_SUCCESS);
  CHECK_EQ(rig.loader().release_event(event), CL_SUCCESS);
  CHECK_EQ(ClIcdRootProbe::live_of(rig.root(), ClObject::Kind::event), 1u);

  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_result(CL_SUCCESS);
  CHECK(rig.watcher().step());
  CHECK_EQ(callback.times, 1u);
  CHECK(callback.event == event);
  CHECK_EQ(ClIcdRootProbe::live_of(rig.root(), ClObject::Kind::event), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_callback_of_nothing_or_on_something_that_is_not_an_event_is_refused)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  cl_event event = made_user_event(rig, context);
  CHECK(event != nullptr);

  const std::uint32_t before = rig.kmt().commands();
  CHECK_EQ(rig.root().watch_event(reinterpret_cast<cl_event>(context), CL_COMPLETE, &note, nullptr),
           CL_INVALID_EVENT);
  rig.host().cl_result(CL_INVALID_OPERATION);
  CHECK_EQ(rig.root().watch_event(event, CL_COMPLETE, &note, nullptr), CL_INVALID_OPERATION);
  CHECK(!rig.watcher().running());
  CHECK_EQ(rig.kmt().commands(), before + 1u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_context_released_under_a_watched_event_lives_for_its_callback)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  cl_event event = made_user_event(rig, context);
  CHECK(event != nullptr);

  callback = Callback();
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.loader().set_event_callback(event, CL_COMPLETE, &note, nullptr), CL_SUCCESS);

  CHECK_EQ(rig.loader().release_context(context), CL_SUCCESS);
  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  CHECK(rig.watcher().step());
  CHECK_EQ(callback.times, 1u);
  CHECK(callback.event == event);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, the_root_is_what_a_watcher_gives_a_delivered_event_back_to)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  cl_event event = made_user_event(rig, context);
  CHECK(event != nullptr);

  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.loader().set_event_callback(event, CL_COMPLETE, &note, nullptr), CL_SUCCESS);
  CHECK_EQ(rig.loader().release_event(event), CL_SUCCESS);
  CHECK_EQ(ClIcdRootProbe::live_of(rig.root(), ClObject::Kind::event), 1u);

  av::clicd::ClEventOwner &owner = rig.root();
  ClObject *const held = ClIcdRootProbe::made(rig.root(), event, ClObject::Kind::event);
  CHECK(held != nullptr);
  const std::uint32_t before = rig.kmt().commands();
  rig.host().cl_result(CL_SUCCESS);
  owner.release_watched(*held);
  CHECK_EQ(ClIcdRootProbe::live_of(rig.root(), ClObject::Kind::event), 0u);
  CHECK_EQ(rig.kmt().commands(), before + 1u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_context_property_this_driver_does_not_implement_is_refused)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);

  const std::uint32_t before = rig.kmt().commands();
  const cl_context_properties unknown[] = {0x4242, 1, 0};
  cl_int              error = CL_SUCCESS;
  CHECK(rig.root().create_context(unknown, &device, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PROPERTY);

  const cl_context_properties twice[] = {CL_CONTEXT_INTEROP_USER_SYNC, 0, CL_CONTEXT_INTEROP_USER_SYNC, 1, 0};
  CHECK(rig.root().create_context(twice, &device, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PROPERTY);

  cl_context_properties unterminated[ClPropertyList::max_pairs * 2u + 3u] = {};
  for (std::uint32_t pair = 0u; pair <= ClPropertyList::max_pairs; ++pair)
      unterminated[pair * 2u] = CL_CONTEXT_INTEROP_USER_SYNC + static_cast<cl_context_properties>(pair);
  CHECK(rig.root().create_context(unterminated, &device, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PROPERTY);
  CHECK_EQ(rig.kmt().commands(), before);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_context_named_on_another_platform_is_refused_and_on_this_one_is_made)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);

  cl_int              error = CL_SUCCESS;
  int                 elsewhere = 0;
  const cl_context_properties other[] = {CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(&elsewhere), 0};
  CHECK(rig.root().create_context(other, &device, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PLATFORM);

  cl_platform_id      ours = nullptr;
  cl_uint             platforms = 0u;
  CHECK_EQ(rig.root().platform_ids(1u, &ours, &platforms), CL_SUCCESS);
  const cl_context_properties named[] = {CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(ours),
                                         CL_CONTEXT_INTEROP_USER_SYNC, 0, 0};
  rig.host().arena_import(0x77u, 3u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(host_context, CL_SUCCESS);
  CHECK(rig.root().create_context(named, &device, 1u, &error) != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_queue_takes_the_two_properties_the_api_names_and_a_buffer_takes_none)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);

  const std::uint64_t known[] = {static_cast<std::uint64_t>(CL_QUEUE_PROPERTIES),
                                 CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0u};
  rig.host().cl_handle(host_queue, CL_SUCCESS);
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().create_queue(context, device, known, &error) != nullptr);
  CHECK_EQ(error, CL_SUCCESS);

  const std::uint32_t before = rig.kmt().commands();
  const std::uint64_t unknown[] = {0x4242u, 1u, 0u};
  CHECK(rig.root().create_queue(context, device, unknown, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PROPERTY);
  const std::uint64_t any[] = {1u, 1u, 0u};
  CHECK(rig.root().create_buffer(context, CL_MEM_READ_WRITE, 64u, nullptr, any, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PROPERTY);
  CHECK_EQ(rig.kmt().commands(), before);
  CHECK(rig.root().invariants_hold()); }

namespace {
int host_queue_list(const MockKmt &kmt, std::uint64_t (&pairs)[4])
{
    for (std::uint32_t index = kmt.commands(); index-- > 0u;) {
        if (kmt.command_id(index) != av::clproto::kCmd_clCreateCommandQueueWithProperties) continue;
        av::WireReader payload(kmt.command_bytes(index) + av::CommandHeader::encoded_size,
                               MockKmt::journal_bytes - av::CommandHeader::encoded_size);
        std::uint64_t context = 0u;
        std::uint64_t device = 0u;
        std::uint8_t  listed = 0u;
        std::uint64_t count = 0u;
        if (!payload.read_pod(context) || !payload.read_pod(device) || !payload.read_pod(listed)) return -2;
        if (listed == 0u) return -1;
        if (!payload.read_varint(count) || count > 4u) return -2;
        for (std::uint64_t at = 0u; at < count; ++at)
            if (!payload.read_pod(pairs[at])) return -2;
        return static_cast<int>(count);
    }
    return -2;
}

}

TEST(cl_icd_root, a_profiling_queue_is_asked_of_the_host_with_profiling_and_nothing_else)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  if (context == nullptr) return;
  std::uint64_t pairs[4] = {};

  const std::uint64_t both[] = {static_cast<std::uint64_t>(CL_QUEUE_PROPERTIES),
                                CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0u};
  rig.host().cl_handle(host_queue, CL_SUCCESS);
  cl_int error = CL_INVALID_VALUE;
  CHECK(rig.root().create_queue(context, device, both, &error) != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK_EQ(host_queue_list(rig.kmt(), pairs), 2);
  CHECK_EQ(pairs[0], static_cast<std::uint64_t>(CL_QUEUE_PROPERTIES));
  CHECK_EQ(pairs[1], static_cast<std::uint64_t>(CL_QUEUE_PROFILING_ENABLE));

  const std::uint64_t in_any_order[] = {static_cast<std::uint64_t>(CL_QUEUE_PROPERTIES),
                                        CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0u};
  rig.host().cl_handle(host_queue + 1u, CL_SUCCESS);
  CHECK(rig.root().create_queue(context, device, in_any_order, &error) != nullptr);
  CHECK_EQ(host_queue_list(rig.kmt(), pairs), -1);

  rig.host().cl_handle(host_queue + 2u, CL_SUCCESS);
  pairs[1] = 0u;
  CHECK(rig.root().dispatch().clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &error) != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK_EQ(host_queue_list(rig.kmt(), pairs), 2);
  CHECK_EQ(pairs[1], static_cast<std::uint64_t>(CL_QUEUE_PROFILING_ENABLE));
  rig.host().cl_handle(host_queue + 3u, CL_SUCCESS);
  CHECK(rig.root().dispatch().clCreateCommandQueue(context, device, 0u, &error) != nullptr);
  CHECK_EQ(host_queue_list(rig.kmt(), pairs), -1);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, an_events_timestamps_are_the_hosts_and_a_refusal_is_its_too)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  cl_event event = made_user_event(rig, context);
  CHECK(event != nullptr);
  if (event == nullptr) return;
  const cl_icd_dispatch &table = rig.root().dispatch();

  const cl_ulong start = 0x1234'5678'9ABCull;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&start), sizeof start);
  cl_ulong    stamp = 0u;
  std::size_t answered = 0u;
  CHECK_EQ(table.clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof stamp, &stamp, &answered),
           CL_SUCCESS);
  CHECK_EQ(stamp, start);
  CHECK_EQ(answered, sizeof stamp);
  rig.host().cl_info(CL_PROFILING_INFO_NOT_AVAILABLE, "", 0u);
  stamp = 7u;
  CHECK_EQ(table.clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof stamp, &stamp, nullptr),
           CL_PROFILING_INFO_NOT_AVAILABLE);
  CHECK_EQ(stamp, 7u);
  const std::uint32_t before = rig.kmt().commands();
  CHECK_EQ(table.clGetEventProfilingInfo(reinterpret_cast<cl_event>(context), CL_PROFILING_COMMAND_END,
                                         sizeof stamp, &stamp, nullptr),
           CL_INVALID_EVENT);
  CHECK_EQ(rig.kmt().commands(), before);
  CHECK(rig.root().invariants_hold()); }

namespace {
cl_command_queue queue_on(ClIcdRig &rig, cl_context context, cl_device_id device)
{
    rig.host().cl_handle(host_queue, CL_SUCCESS);
    return rig.loader().create_queue(context, device, nullptr);
}

cl_mem buffer_on(ClIcdRig &rig, cl_context context)
{
    rig.host().cl_handle(host_buffer, CL_SUCCESS);
    return rig.loader().create_buffer(context, CL_MEM_READ_WRITE, 64u, nullptr);
}

}

TEST(cl_icd_root, a_non_blocking_read_returns_before_the_bytes_and_leaves_the_destination_alone)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  cl_command_queue queue = queue_on(rig, context, device);
  cl_mem buffer = buffer_on(rig, context);
  CHECK(queue != nullptr && buffer != nullptr);

  unsigned char into[16];
  for (unsigned char &byte : into) byte = 0xA5u;
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  cl_event event = nullptr;
  const std::uint32_t before = rig.kmt().commands();
  CHECK_EQ(rig.root().enqueue_read(queue, buffer, CL_FALSE, 0u, sizeof into, into, &event),
           CL_SUCCESS);
  CHECK(event != nullptr);
  CHECK_EQ(rig.kmt().commands(), before + 1u);
  for (const unsigned char byte : into) CHECK_EQ(byte, 0xA5u);

  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  const unsigned char answered[16] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u,
                                      9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u};
  rig.host().cl_bytes(CL_SUCCESS, answered, sizeof answered);
  CHECK_EQ(rig.loader().wait_for_events(1u, &event), CL_SUCCESS);
  for (std::size_t index = 0u; index < sizeof into; ++index)
      CHECK_EQ(into[index], answered[index]);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_blocking_read_is_not_deferred_and_has_its_bytes_when_it_returns)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  cl_command_queue queue = queue_on(rig, context, device);
  cl_mem buffer = buffer_on(rig, context);
  CHECK(queue != nullptr && buffer != nullptr);

  unsigned char into[8] = {};
  const unsigned char answered[8] = {9u, 8u, 7u, 6u, 5u, 4u, 3u, 2u};
  rig.host().cl_bytes(CL_SUCCESS, answered, sizeof answered);
  CHECK_EQ(rig.root().enqueue_read(queue, buffer, CL_TRUE, 0u, sizeof into, into, nullptr),
           CL_SUCCESS);
  for (std::size_t index = 0u; index < sizeof into; ++index)
      CHECK_EQ(into[index], answered[index]);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_read_the_application_wanted_no_event_for_keeps_the_marker_until_it_delivers)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  cl_command_queue queue = queue_on(rig, context, device);
  cl_mem buffer = buffer_on(rig, context);
  CHECK(queue != nullptr && buffer != nullptr);

  unsigned char into[8] = {};
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  CHECK_EQ(rig.root().enqueue_read(queue, buffer, CL_FALSE, 0u, sizeof into, into, nullptr),
           CL_SUCCESS);
  CHECK_EQ(ClIcdRootProbe::outstanding_reads(rig.root()), 1u);

  const cl_int complete = CL_COMPLETE;
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 1u);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_result(CL_SUCCESS);
  const unsigned char answered[8] = {3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u};
  rig.host().cl_bytes(CL_SUCCESS, answered, sizeof answered);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.loader().finish(queue), CL_SUCCESS);
  for (const unsigned char byte : into) CHECK_EQ(byte, 3u);
  CHECK_EQ(ClIcdRootProbe::outstanding_reads(rig.root()), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_finish_on_a_context_the_driver_wakes_sleeps_until_it_does)
{ ClIcdRig rig;
  rig.watcher().offer_wake(0x2A0u);
  rig.kmt().accept_wake_events();
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  CHECK_EQ(rig.kmt().wake_events(), 1u);
  CHECK_EQ(rig.kmt().wake_event(), 0x2A0u);
  cl_command_queue queue = queue_on(rig, context, device);
  cl_mem buffer = buffer_on(rig, context);
  CHECK(queue != nullptr && buffer != nullptr);
  unsigned char into[8] = {};
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  CHECK_EQ(rig.root().enqueue_read(queue, buffer, CL_FALSE, 0u, sizeof into, into, nullptr),
           CL_SUCCESS);

  const cl_int running = CL_RUNNING;
  const cl_int complete = CL_COMPLETE;
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 1u);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&running), sizeof running);
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&running), sizeof running);
  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_result(CL_SUCCESS);
  const unsigned char answered[8] = {3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u};
  rig.host().cl_bytes(CL_SUCCESS, answered, sizeof answered);
  rig.host().cl_result(CL_SUCCESS);
  const std::uint32_t before = rig.kmt().commands();
  CHECK_EQ(rig.loader().finish(queue), CL_SUCCESS);
  for (const unsigned char byte : into) CHECK_EQ(byte, 3u);
  std::uint32_t raised = 0u;
  for (std::uint32_t index = before; index < rig.kmt().commands(); ++index)
      if (rig.kmt().command_id(index) == av::clproto::kCmd_clSetEventCallback) ++raised;
  CHECK_EQ(raised, 1u);
  CHECK_EQ(rig.watcher().awaits(), 1u);
  CHECK(rig.watcher().running());
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, releasing_a_queue_forgets_the_reads_nobody_will_wait_for)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  cl_command_queue queue = queue_on(rig, context, device);
  cl_mem buffer = buffer_on(rig, context);
  CHECK(queue != nullptr && buffer != nullptr);

  unsigned char into[8] = {};
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  CHECK_EQ(rig.root().enqueue_read(queue, buffer, CL_FALSE, 0u, sizeof into, into, nullptr),
           CL_SUCCESS);
  CHECK_EQ(ClIcdRootProbe::outstanding_reads(rig.root()), 1u);

  rig.host().cl_result(CL_SUCCESS);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.loader().release_queue(queue), CL_SUCCESS);
  CHECK_EQ(ClIcdRootProbe::outstanding_reads(rig.root()), 0u);
  for (const unsigned char byte : into) CHECK_EQ(byte, 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, an_application_that_polls_the_status_gets_the_bytes_there)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  cl_command_queue queue = queue_on(rig, context, device);
  cl_mem buffer = buffer_on(rig, context);
  CHECK(queue != nullptr && buffer != nullptr);

  unsigned char into[4] = {};
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  cl_event event = nullptr;
  CHECK_EQ(rig.root().enqueue_read(queue, buffer, CL_FALSE, 0u, sizeof into, into, &event),
           CL_SUCCESS);

  const cl_int running = CL_RUNNING;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&running), sizeof running);
  cl_int      status = CL_QUEUED;
  std::size_t answered_bytes = 0u;
  CHECK_EQ(rig.loader().event_info(event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status,
                                   &status, &answered_bytes),
           CL_SUCCESS);
  CHECK_EQ(status, CL_RUNNING);
  for (const unsigned char byte : into) CHECK_EQ(byte, 0u);

  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  const unsigned char answered[4] = {7u, 7u, 7u, 7u};
  rig.host().cl_bytes(CL_SUCCESS, answered, sizeof answered);
  CHECK_EQ(rig.loader().event_info(event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status,
                                   &status, &answered_bytes),
           CL_SUCCESS);
  CHECK_EQ(status, CL_COMPLETE);
  for (const unsigned char byte : into) CHECK_EQ(byte, 7u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_callback_on_a_non_blocking_read_sees_the_bytes)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  cl_command_queue queue = queue_on(rig, context, device);
  cl_mem buffer = buffer_on(rig, context);
  CHECK(queue != nullptr && buffer != nullptr);

  static unsigned char into[4] = {};
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  cl_event event = nullptr;
  CHECK_EQ(rig.root().enqueue_read(queue, buffer, CL_FALSE, 0u, sizeof into, into, &event),
           CL_SUCCESS);

  callback = Callback();
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.loader().set_event_callback(event, CL_COMPLETE, &note, nullptr), CL_SUCCESS);

  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  const unsigned char answered[4] = {4u, 5u, 6u, 7u};
  rig.host().cl_bytes(CL_SUCCESS, answered, sizeof answered);
  CHECK(rig.watcher().step());
  CHECK_EQ(callback.times, 1u);
  for (std::size_t index = 0u; index < sizeof into; ++index)
      CHECK_EQ(into[index], answered[index]);
  CHECK(rig.root().invariants_hold()); }

namespace av {

struct ObjectPoolProbe {
    template <typename T>
    static void miscount(ObjectPool<T> &pool) { pool.live_ += 1u; }
};

}

namespace av::icd {

struct HandleTreeProbe {
    template <typename Node>
    static ObjectPool<Node> &pool(HandleTree<Node> &tree) { return tree.pool_; }
};

}

namespace av::clicd {
struct ClPlatformProbe {
    static void an_empty_version(ClPlatform &platform) { platform.version_[0] = '\0'; }
};

struct ClExtensionPolicyProbe {
    static void a_row_nothing_could_report(ClExtensionPolicy &policy)
    {
        static const ClExtensionPolicy::Row nameless[] = {{nullptr, 1u, "cl_icd_root.the_invariant_is_every_part_of_it"}};
        new (&policy.rows_) icd::ExtensionRows(nameless, 1u, "cl_", icd::ExtensionRows::Names(nullptr, 0u),
                                               icd::ExtensionRows::Names(nullptr, 0u));
    }
};

struct ClObjectTableProbe {
    static void miscount(ClObjectTable &table) { ObjectPoolProbe::miscount(icd::HandleTreeProbe::pool(table.tree_)); }
};

struct ClDeviceReportProbe {
    static void a_version_that_runs_off_its_room(ClDeviceReport &device)
    { for (char &byte : device.version_text_) byte = 'x'; }
};

struct ClPendingReadsProbe {
    static void miscount(ClPendingReads &reads) { reads.held_ += 1u; }
};

struct ClEventWatcherProbe {
    static void miscount(ClEventWatcher &watcher) { watcher.held_ += 1u; }
};

}

namespace av::icd {

struct IcdConnectionProbe {
    static void lose_adapter(IcdConnection &connection) { static_cast<void>(connection.search_.close()); }
};

}

TEST(cl_icd_root, the_invariant_is_every_part_of_it)
{ { ClIcdRig rig;
    CHECK(rig.root().invariants_hold());
    av::clicd::ClPlatformProbe::an_empty_version(av::clicd::ClIcdRootProbe::platform(rig.root()));
    CHECK(!rig.root().invariants_hold()); }
  { ClIcdRig rig;
    av::clicd::ClExtensionPolicyProbe::a_row_nothing_could_report(
        av::clicd::ClIcdRootProbe::extensions(rig.root()));
    CHECK(!rig.root().invariants_hold()); }
  { ClIcdRig rig;
    av::clicd::ClObjectTableProbe::miscount(av::clicd::ClIcdRootProbe::objects_of(rig.root()));
    CHECK(!rig.root().invariants_hold()); }
  { ClIcdRig rig;
    av::icd::DeviceLink *link = nullptr;
    CHECK_STATUS(av::clicd::ClIcdRootProbe::connection(rig.root()).acquire(link), av::Status::ok);
    CHECK(rig.root().invariants_hold());
    av::icd::IcdConnectionProbe::lose_adapter(av::clicd::ClIcdRootProbe::connection(rig.root()));
    CHECK(!rig.root().invariants_hold()); }
  { ClIcdRig rig;
    av::clicd::ClDeviceReportProbe::a_version_that_runs_off_its_room(
        av::clicd::ClIcdRootProbe::device(rig.root()));
    CHECK(!rig.root().invariants_hold()); }
  { ClIcdRig rig;
    av::clicd::ClPendingReadsProbe::miscount(av::clicd::ClIcdRootProbe::reads_of(rig.root()));
    CHECK(!rig.root().invariants_hold()); }
  { ClIcdRig rig;
    av::clicd::ClEventWatcherProbe::miscount(av::clicd::ClIcdRootProbe::watcher(rig.root()));
    CHECK(!rig.root().invariants_hold()); } }

TEST(cl_icd_root, every_entry_refuses_an_object_whose_context_is_gone)
{ ClIcdRig rig;
  ClObject *const queue =
      ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::command_queue, 0x6500u);
  ClObject *const buffer =
      ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::mem, 0x6600u);
  ClObject *const kernel =
      ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::kernel, 0x6800u);
  ClObject *const context =
      ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::context, 0x6400u);
  ClObject *const event =
      ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::event, 0x6900u);
  ClObject *const device =
      ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::device, 0x6300u);
  CHECK(queue != nullptr && buffer != nullptr && kernel != nullptr);
  CHECK(context != nullptr && event != nullptr && device != nullptr);
  if (queue == nullptr || buffer == nullptr || kernel == nullptr) return;
  if (context == nullptr || event == nullptr || device == nullptr) return;

  auto *const as_queue = reinterpret_cast<cl_command_queue>(queue);
  auto *const as_buffer = reinterpret_cast<cl_mem>(buffer);
  auto *const as_kernel = reinterpret_cast<cl_kernel>(kernel);
  auto *const as_context = reinterpret_cast<cl_context>(context);
  auto *const as_event = reinterpret_cast<cl_event>(event);
  auto *const as_device = reinterpret_cast<cl_device_id>(device);

  const std::uint32_t sent = rig.kmt().commands();
  std::uint8_t        bytes[8] = {};

  cl_int error = CL_SUCCESS;
  CHECK(rig.root().map_buffer(as_queue, as_buffer, CL_TRUE, CL_MAP_READ, 0u, sizeof bytes,
                              nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().unmap(as_queue, as_buffer, bytes, nullptr), CL_INVALID_CONTEXT);

  CHECK(rig.root().svm_alloc(as_context, CL_MEM_READ_WRITE, sizeof bytes, 0u) == nullptr);
  rig.root().svm_free(as_context, bytes);
  CHECK_EQ(rig.root().svm_map(as_queue, CL_TRUE, CL_MAP_READ, bytes, sizeof bytes, nullptr),
           CL_INVALID_CONTEXT);
  CHECK_EQ(rig.root().svm_unmap(as_queue, bytes, nullptr), CL_INVALID_CONTEXT);
  CHECK_EQ(rig.root().set_kernel_svm_argument(as_kernel, 0u, bytes), CL_INVALID_CONTEXT);

  CHECK_EQ(rig.root().wait_for_events(1u, &as_event), CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().event_info(as_event, CL_EVENT_COMMAND_EXECUTION_STATUS, 0u, nullptr,
                                 nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().watch_event(as_event, CL_COMPLETE, nullptr, nullptr), CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().set_user_event_status(as_event, CL_COMPLETE), CL_OUT_OF_RESOURCES);

  char        name[8] = {};
  std::size_t answered = 0u;
  CHECK_EQ(rig.root().device_info(as_device, CL_DEVICE_NAME, sizeof name, name, &answered),
           CL_INVALID_DEVICE);
  CHECK_EQ(rig.root().device_info(as_device, CL_DEVICE_EXTENSIONS, sizeof name, name, &answered),
           CL_INVALID_DEVICE);

  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_watched_event_with_no_context_delivers_nothing)
{ ClIcdRig rig;
  ClObject *const event =
      ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::event, 0x6900u);
  CHECK(event != nullptr);
  if (event == nullptr) return;
  const std::uint32_t sent = rig.kmt().commands();
  rig.root().deliver_watched(*event);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, releasing_an_object_with_no_context_tells_the_host_nothing)
{ ClIcdRig rig;
  ClObject *const buffer =
      ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::mem, 0x6600u);
  ClObject *const context =
      ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::context, 0x6400u);
  CHECK(buffer != nullptr && context != nullptr);
  if (buffer == nullptr || context == nullptr) return;
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().release(buffer, ClObject::Kind::mem), CL_SUCCESS);
  CHECK_EQ(rig.root().release(context, ClObject::Kind::context), CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK_EQ(ClIcdRootProbe::live_of(rig.root(), ClObject::Kind::mem), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_wait_over_two_contexts_is_refused_before_anything_is_asked)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context   context = opened_context(rig, device);
  CHECK(context != nullptr);
  if (context == nullptr) return;
  rig.host().cl_handle(host_event, CL_SUCCESS);
  cl_event mine = rig.loader().create_user_event(context, nullptr);
  CHECK(mine != nullptr);
  if (mine == nullptr) return;

  ClObject *const stranger =
      ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::event, 0x6901u);
  CHECK(stranger != nullptr);
  if (stranger == nullptr) return;
  const cl_event both[] = {mine, reinterpret_cast<cl_event>(stranger)};
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().wait_for_events(2u, both), CL_INVALID_CONTEXT);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_status_query_with_no_room_for_a_status_delivers_nothing)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context   context = opened_context(rig, device);
  CHECK(context != nullptr);
  if (context == nullptr) return;
  rig.host().cl_handle(host_event, CL_SUCCESS);
  cl_event event = rig.loader().create_user_event(context, nullptr);
  CHECK(event != nullptr);
  if (event == nullptr) return;

  const char byte_of_a_status = 0;
  rig.host().cl_info(CL_SUCCESS, &byte_of_a_status, 1u);
  unsigned char cramped[1] = {0u};
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().event_info(event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof cramped,
                                 cramped, nullptr),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_callback_the_watcher_cannot_keep_gives_its_reference_back)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context   context = opened_context(rig, device);
  CHECK(context != nullptr);
  if (context == nullptr) return;
  rig.host().cl_handle(host_event, CL_SUCCESS);
  cl_event event = rig.loader().create_user_event(context, nullptr);
  CHECK(event != nullptr);
  if (event == nullptr) return;

  const std::uint32_t live = ClIcdRootProbe::live_of(rig.root(), ClObject::Kind::event);
  rig.host().cl_result(CL_SUCCESS);
  rig.watcher().refuse_start();
  CHECK_EQ(rig.root().watch_event(event, CL_COMPLETE, &nothing_ran, nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(ClIcdRootProbe::live_of(rig.root(), ClObject::Kind::event), live);
  CHECK(rig.root().invariants_hold());
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().release(event, ClObject::Kind::event), CL_SUCCESS);
  CHECK_EQ(ClIcdRootProbe::live_of(rig.root(), ClObject::Kind::event), live - 1u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_callback_registration_with_no_callback_is_an_invalid_value)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context   context = opened_context(rig, device);
  CHECK(context != nullptr);
  if (context == nullptr) return;
  rig.host().cl_handle(host_event, CL_SUCCESS);
  cl_event event = rig.loader().create_user_event(context, nullptr);
  CHECK(event != nullptr);
  if (event == nullptr) return;
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().dispatch().clSetEventCallback(event, CL_COMPLETE, nullptr, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, the_deprecated_queue_call_makes_the_same_queue)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context   context = opened_context(rig, device);
  CHECK(context != nullptr);
  if (context == nullptr) return;
  rig.host().cl_handle(host_queue, CL_SUCCESS);
  cl_int error = CL_INVALID_VALUE;
  cl_command_queue queue =
      rig.root().dispatch().clCreateCommandQueue(context, device, 0u, &error);
  CHECK(queue != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(rig.loader().dispatch_of(queue) == &rig.root().dispatch());
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_contexts_info_is_this_icds_own_and_asks_the_host_nothing)
{ ClIcdRig rig;
  const cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);
  if (device == nullptr) return;
  const cl_icd_dispatch &table = rig.root().dispatch();
  const cl_context_properties asked[] = {CL_CONTEXT_PLATFORM,
                                         reinterpret_cast<cl_context_properties>(rig.loader().platform(0u)),
                                         CL_CONTEXT_INTEROP_USER_SYNC, CL_TRUE, 0};
  rig.host().arena_import(0x77u, 3u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(host_context, CL_SUCCESS);
  cl_int           error = CL_INVALID_VALUE;
  const cl_context context = table.clCreateContext(asked, 1u, &device, nullptr, nullptr, &error);
  CHECK_EQ(error, CL_SUCCESS);
  if (context == nullptr) return;
  CHECK_EQ(table.clRetainContext(context), CL_SUCCESS);
  const std::uint32_t before = rig.kmt().commands();

  cl_uint     count = 0u;
  std::size_t size = 0u;
  CHECK_EQ(table.clGetContextInfo(context, CL_CONTEXT_REFERENCE_COUNT, sizeof count, &count, &size), CL_SUCCESS);
  CHECK_EQ(count, 2u);
  CHECK_EQ(size, sizeof(cl_uint));
  CHECK_EQ(table.clGetContextInfo(context, CL_CONTEXT_NUM_DEVICES, sizeof count, &count, nullptr), CL_SUCCESS);
  CHECK_EQ(count, 1u);
  cl_device_id named = nullptr;
  CHECK_EQ(table.clGetContextInfo(context, CL_CONTEXT_DEVICES, sizeof named, &named, &size), CL_SUCCESS);
  CHECK(named == device);
  CHECK_EQ(size, sizeof(cl_device_id));
  cl_context_properties kept[8] = {};
  CHECK_EQ(table.clGetContextInfo(context, CL_CONTEXT_PROPERTIES, 0u, nullptr, &size), CL_SUCCESS);
  CHECK_EQ(size, sizeof asked);
  CHECK_EQ(table.clGetContextInfo(context, CL_CONTEXT_PROPERTIES, sizeof kept, kept, nullptr), CL_SUCCESS);
  for (std::size_t at = 0u; at < sizeof asked / sizeof asked[0]; ++at) CHECK_EQ(kept[at], asked[at]);
  kept[0] = 7;
  CHECK_EQ(table.clGetContextInfo(context, CL_CONTEXT_PROPERTIES, sizeof kept[0], kept, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(kept[0], 7);
  CHECK_EQ(table.clGetContextInfo(context, 0x1234u, sizeof count, &count, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.root().context_info(reinterpret_cast<cl_context>(device), CL_CONTEXT_NUM_DEVICES, sizeof count,
                                   &count, nullptr),
           CL_INVALID_CONTEXT);
  CHECK_EQ(rig.kmt().commands(), before);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_context_made_with_no_list_has_none_to_give)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context   context = opened_context(rig, device);
  CHECK(context != nullptr);
  if (context == nullptr) return;
  std::size_t size = 99u;
  CHECK_EQ(rig.root().context_info(context, CL_CONTEXT_PROPERTIES, 0u, nullptr, &size), CL_SUCCESS);
  CHECK_EQ(size, 0u); }

TEST(cl_icd_root, a_buffers_info_names_this_icds_objects_and_asks_the_host_the_rest)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context   context = opened_context(rig, device);
  CHECK(context != nullptr);
  if (context == nullptr) return;
  cl_mem buffer = buffer_on(rig, context);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;
  const cl_icd_dispatch &table = rig.root().dispatch();
  CHECK_EQ(table.clRetainMemObject(buffer), CL_SUCCESS);
  const std::uint32_t before = rig.kmt().commands();
  cl_context owner = nullptr;
  CHECK_EQ(table.clGetMemObjectInfo(buffer, CL_MEM_CONTEXT, sizeof owner, &owner, nullptr), CL_SUCCESS);
  CHECK(owner == context);
  cl_mem over = buffer;
  CHECK_EQ(table.clGetMemObjectInfo(buffer, CL_MEM_ASSOCIATED_MEMOBJECT, sizeof over, &over, nullptr), CL_SUCCESS);
  CHECK(over == nullptr);
  cl_uint count = 0u;
  CHECK_EQ(table.clGetMemObjectInfo(buffer, CL_MEM_REFERENCE_COUNT, sizeof count, &count, nullptr), CL_SUCCESS);
  CHECK_EQ(count, 2u);
  void *pointer = &count;
  CHECK_EQ(table.clGetMemObjectInfo(buffer, CL_MEM_HOST_PTR, sizeof pointer, &pointer, nullptr), CL_SUCCESS);
  CHECK(pointer == nullptr);
  CHECK_EQ(rig.kmt().commands(), before);
  const std::size_t host_size = 64u;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&host_size), sizeof host_size);
  std::size_t size = 0u;
  CHECK_EQ(table.clGetMemObjectInfo(buffer, CL_MEM_SIZE, sizeof size, &size, nullptr), CL_SUCCESS);
  CHECK_EQ(size, 64u);
  CHECK_EQ(rig.kmt().commands(), before + 1u);
  size = 0u;
  std::size_t wanted = 0u;
  CHECK_EQ(table.clGetMemObjectInfo(buffer, CL_MEM_SIZE, sizeof size, &size, &wanted), CL_SUCCESS);
  CHECK_EQ(size, 64u);
  CHECK_EQ(wanted, sizeof size);
  std::uint32_t cramped = 0u;
  CHECK_EQ(table.clGetMemObjectInfo(buffer, CL_MEM_SIZE, sizeof cramped, &cramped, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), before + 1u);
  const cl_mem_object_type type = CL_MEM_OBJECT_BUFFER;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&type), sizeof type);
  const std::size_t offset = 0u;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&offset), sizeof offset);
  const cl_bool svm = CL_FALSE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&svm), sizeof svm);
  for (int twice = 0; twice < 2; ++twice) {
      cl_mem_object_type got_type = 0u;
      CHECK_EQ(table.clGetMemObjectInfo(buffer, CL_MEM_TYPE, sizeof got_type, &got_type, nullptr), CL_SUCCESS);
      CHECK_EQ(got_type, static_cast<cl_mem_object_type>(CL_MEM_OBJECT_BUFFER));
      std::size_t got_offset = 1u;
      CHECK_EQ(table.clGetMemObjectInfo(buffer, CL_MEM_OFFSET, sizeof got_offset, &got_offset, nullptr), CL_SUCCESS);
      CHECK_EQ(got_offset, 0u);
      cl_bool got_svm = CL_TRUE;
      CHECK_EQ(table.clGetMemObjectInfo(buffer, CL_MEM_USES_SVM_POINTER, sizeof got_svm, &got_svm, nullptr),
               CL_SUCCESS);
      CHECK_EQ(got_svm, static_cast<cl_bool>(CL_FALSE));
  }
  CHECK_EQ(rig.kmt().commands(), before + 4u);
  const cl_uint maps = 1u;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&maps), sizeof maps);
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&maps), sizeof maps);
  for (int twice = 0; twice < 2; ++twice) {
      cl_uint mapped = 0u;
      CHECK_EQ(table.clGetMemObjectInfo(buffer, CL_MEM_MAP_COUNT, sizeof mapped, &mapped, nullptr), CL_SUCCESS);
      CHECK_EQ(mapped, 1u);
  }
  CHECK_EQ(rig.kmt().commands(), before + 6u);
  CHECK_EQ(rig.root().mem_object_info(reinterpret_cast<cl_mem>(context), CL_MEM_SIZE, sizeof size, &size, nullptr),
           CL_INVALID_MEM_OBJECT);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_buffer_answer_the_host_refuses_is_not_kept)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context   context = opened_context(rig, device);
  CHECK(context != nullptr);
  if (context == nullptr) return;
  cl_mem buffer = buffer_on(rig, context);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;
  const cl_icd_dispatch &table = rig.root().dispatch();
  const std::uint32_t    before = rig.kmt().commands();
  rig.host().cl_info(CL_OUT_OF_RESOURCES, nullptr, 0u);
  std::size_t size = 0u;
  CHECK_EQ(table.clGetMemObjectInfo(buffer, CL_MEM_SIZE, sizeof size, &size, nullptr), CL_OUT_OF_RESOURCES);
  const std::size_t host_size = 128u;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&host_size), sizeof host_size);
  CHECK_EQ(table.clGetMemObjectInfo(buffer, CL_MEM_SIZE, sizeof size, &size, nullptr), CL_SUCCESS);
  CHECK_EQ(size, 128u);
  CHECK_EQ(rig.kmt().commands(), before + 2u);
  CHECK_EQ(rig.kmt().queued_answers(), 0u); }

TEST(cl_icd_root, an_images_buffer_is_none_and_its_extent_is_the_hosts)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context   context = opened_context(rig, device);
  CHECK(context != nullptr);
  if (context == nullptr) return;
  cl_mem buffer = buffer_on(rig, context);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;
  const cl_icd_dispatch &table = rig.root().dispatch();
  const std::uint32_t    before = rig.kmt().commands();
  cl_mem                 under = buffer;
  CHECK_EQ(table.clGetImageInfo(buffer, CL_IMAGE_BUFFER, sizeof under, &under, nullptr), CL_SUCCESS);
  CHECK(under == nullptr);
  CHECK_EQ(rig.kmt().commands(), before);
  const std::size_t host_width = 640u;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&host_width), sizeof host_width);
  std::size_t width = 0u;
  CHECK_EQ(table.clGetImageInfo(buffer, CL_IMAGE_WIDTH, sizeof width, &width, nullptr), CL_SUCCESS);
  CHECK_EQ(width, 640u);
  CHECK_EQ(rig.root().image_info(reinterpret_cast<cl_mem>(context), CL_IMAGE_WIDTH, sizeof width, &width, nullptr),
           CL_INVALID_MEM_OBJECT); }

TEST(cl_icd_root, a_program_that_does_not_build_leaves_its_options_and_the_hosts_log)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context   context = opened_context(rig, device);
  CHECK(context != nullptr);
  if (context == nullptr) return;
  rig.host().cl_handle(host_program, CL_SUCCESS);
  cl_program program = rig.loader().create_program(context, "__kernel void k(void){ nope; }", nullptr);
  CHECK(program != nullptr);
  if (program == nullptr) return;
  const char compiler[] = "k.cl:1:24: error: use of undeclared identifier 'nope'\n\n1 error generated.\n";
  rig.host().cl_result(CL_BUILD_PROGRAM_FAILURE);
  rig.host().cl_info(CL_SUCCESS, compiler, sizeof compiler);
  rig.host().cl_info(CL_SUCCESS, compiler, sizeof compiler);
  const std::uint32_t before = rig.log().lines();
  CHECK_EQ(rig.root().dispatch().clBuildProgram(program, 0u, nullptr, "-cl-fast-relaxed-math", nullptr, nullptr),
           CL_BUILD_PROGRAM_FAILURE);
  CHECK_EQ(rig.log().lines() - before, 4u);
  CHECK(std::strcmp(rig.log().line(before), "avcl: a program did not build; its options: -cl-fast-relaxed-math") ==
        0);
  CHECK(std::strcmp(rig.log().line(before + 1u),
                    "avcl: build: k.cl:1:24: error: use of undeclared identifier 'nope'") == 0);
  CHECK(std::strcmp(rig.log().line(before + 2u), "avcl: build: 1 error generated.") == 0);
  CHECK(std::strcmp(rig.log().line(before + 3u), "avcl: clBuildProgram answered -11") == 0);
  CHECK_EQ(rig.kmt().queued_answers(), 0u); }

TEST(cl_icd_root, a_creation_that_fails_is_named_even_with_nowhere_to_put_its_error)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context   context = opened_context(rig, device);
  CHECK(context != nullptr);
  if (context == nullptr) return;
  const cl_icd_dispatch &table = rig.root().dispatch();
  const std::uint32_t    before = rig.log().lines();
  CHECK(table.clCreateKernel(reinterpret_cast<cl_program>(context), "k", nullptr) == nullptr);
  cl_int error = CL_SUCCESS;
  CHECK(table.clCreateKernel(reinterpret_cast<cl_program>(context), "k", &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PROGRAM);
  cl_uint count = 0u;
  CHECK_EQ(table.clGetContextInfo(context, 0x1234u, sizeof count, &count, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(table.clGetContextInfo(context, 0x1234u, sizeof count, &count, nullptr), CL_INVALID_VALUE);
  CHECK_EQ(rig.log().lines() - before, 2u);
  CHECK(std::strcmp(rig.log().line(before), "avcl: clCreateKernel answered -44") == 0);
  CHECK(std::strcmp(rig.log().line(before + 1u), "avcl: clGetContextInfo answered -30") == 0);
  error = CL_INVALID_VALUE;
  CHECK_EQ(table.clGetContextInfo(context, CL_CONTEXT_NUM_DEVICES, sizeof count, &count, nullptr), CL_SUCCESS);
  CHECK_EQ(rig.log().lines() - before, 2u); }

TEST(cl_icd_root, a_state_dump_reads_under_the_lock_or_says_busy)
{ ClIcdRig rig;
  char            free_bytes[2048] = {};
  av::StateWriter free_writer(free_bytes, sizeof free_bytes);
  rig.root().report(free_writer);
  CHECK(std::strstr(free_writer.dump(), "avcl/0 busy=false") != nullptr);
  CHECK(std::strstr(free_writer.dump(), "devices/0 learnt=false left_out=0") != nullptr);
  CHECK(std::strstr(free_writer.dump(), "reads_outstanding=0") != nullptr);
  CHECK(!rig.lock().held());
  rig.lock().held_elsewhere(true);
  char            busy_bytes[256] = {};
  av::StateWriter busy_writer(busy_bytes, sizeof busy_bytes);
  rig.root().report(busy_writer);
  CHECK(std::strstr(busy_writer.dump(), "avcl/0 busy=true") != nullptr);
  CHECK(std::strstr(busy_writer.dump(), "reads_outstanding") == nullptr);
  rig.lock().held_elsewhere(false); }

TEST(cl_icd_root, the_platform_and_device_are_named_after_the_adapter_windows_describes)
{ ClIcdRig rig;
  rig.kmt().describe_adapter(0u, av::PciLocation(0u, 17u, 0u), "AMD Radeon(TM) Graphics", 0u);
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  const std::uint32_t opened = rig.kmt().open_objects();
  char name[160] = {};
  CHECK_EQ(rig.root().platform_info(rig.loader().platform(0u), CL_PLATFORM_NAME, sizeof name, name, nullptr),
           CL_SUCCESS);
  CHECK_EQ(std::strcmp(name, "avgpu OpenCL on AMD Radeon(TM) Graphics"), 0);
  CHECK_EQ(rig.kmt().open_objects(), opened);
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::create_device), 0u);
  device_opens(rig);
  cl_device_id device = nullptr;
  cl_uint      devices = 0u;
  CHECK_EQ(rig.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device, &devices), CL_SUCCESS);
  CHECK(device != nullptr);
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_NAME, sizeof name, name, nullptr), CL_SUCCESS);
  CHECK_EQ(std::strcmp(name, "AMD Radeon(TM) Graphics"), 0);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, the_platform_name_is_found_once_and_without_an_adapter_is_the_icd_s_own)
{ char name[160] = {};
  { ClIcdRig rig;
    rig.kmt().describe_adapter(0u, av::PciLocation(0u, 17u, 0u), "AMD Radeon(TM) Graphics", 0u);
    CHECK(rig.loader().load() == FakeClLoader::Load::kept);
    CHECK_EQ(rig.root().platform_info(rig.loader().platform(0u), CL_PLATFORM_NAME, sizeof name, name, nullptr),
             CL_SUCCESS);
    const std::uint32_t searched = rig.kmt().calls(MockKmt::Call::enumerate_adapters);
    CHECK_EQ(rig.root().platform_info(rig.loader().platform(0u), CL_PLATFORM_NAME, sizeof name, name, nullptr),
             CL_SUCCESS);
    CHECK_EQ(rig.kmt().calls(MockKmt::Call::enumerate_adapters), searched);
    CHECK_EQ(std::strcmp(name, "avgpu OpenCL on AMD Radeon(TM) Graphics"), 0); }
  for (const MockKmt::Call refused : {MockKmt::Call::enumerate_adapters, MockKmt::Call::query_adapter_location,
                                     MockKmt::Call::query_adapter_name}) {
      ClIcdRig rig;
      rig.kmt().describe_adapter(0u, av::PciLocation(0u, 17u, 0u), "AMD Radeon(TM) Graphics", 0u);
      CHECK(rig.loader().load() == FakeClLoader::Load::kept);
      rig.kmt().fail_at(refused, 1u, MockKmt::invalid_parameter);
      CHECK_EQ(rig.root().platform_info(rig.loader().platform(0u), CL_PLATFORM_NAME, sizeof name, name, nullptr),
               CL_SUCCESS);
      CHECK_EQ(std::strcmp(name, "avgpu OpenCL"), 0);
      CHECK_EQ(rig.kmt().open_objects(), 0u);
      CHECK_EQ(rig.root().platform_info(rig.loader().platform(0u), CL_PLATFORM_NAME, sizeof name, name, nullptr),
               CL_SUCCESS);
      CHECK_EQ(std::strcmp(name, "avgpu OpenCL on AMD Radeon(TM) Graphics"), 0);
  }
  { ClIcdRig rig;
    rig.kmt().describe_adapter(0u, av::PciLocation(0u, 17u, 0u), "AMD Radeon(TM) Graphics", 0u);
    CHECK(rig.loader().load() == FakeClLoader::Load::kept);
    rig.kmt().fail_at(MockKmt::Call::query_identity, 2u, MockKmt::invalid_parameter);
    CHECK_EQ(rig.root().platform_info(rig.loader().platform(0u), CL_PLATFORM_NAME, sizeof name, name, nullptr),
             CL_SUCCESS);
    CHECK_EQ(std::strcmp(name, "avgpu OpenCL"), 0); } }

TEST(cl_icd_root, a_queue_says_its_context_and_device_and_asks_the_host_the_rest)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);
  if (device == nullptr) return;
  rig.host().arena_import(0x78u, 3u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(host_context, CL_SUCCESS);
  const cl_context context = rig.loader().create_context(device, nullptr);
  rig.host().cl_handle(host_queue, CL_SUCCESS);
  const cl_command_queue queue = rig.loader().create_queue(context, device, nullptr);
  CHECK(queue != nullptr);
  const cl_icd_dispatch &table = *rig.loader().dispatch_of(queue);
  cl_context   owner = nullptr;
  cl_device_id on = nullptr;
  cl_uint      count = 0u;
  cl_command_queue default_queue = queue;
  CHECK_EQ(table.clGetCommandQueueInfo(queue, CL_QUEUE_CONTEXT, sizeof owner, &owner, nullptr), CL_SUCCESS);
  CHECK(owner == context);
  CHECK_EQ(table.clGetCommandQueueInfo(queue, CL_QUEUE_DEVICE, sizeof on, &on, nullptr), CL_SUCCESS);
  CHECK(on == device);
  CHECK_EQ(table.clGetCommandQueueInfo(queue, CL_QUEUE_REFERENCE_COUNT, sizeof count, &count, nullptr), CL_SUCCESS);
  CHECK_EQ(count, 1u);
  CHECK_EQ(table.clGetCommandQueueInfo(queue, CL_QUEUE_DEVICE_DEFAULT, sizeof default_queue, &default_queue,
                                       nullptr),
           CL_SUCCESS);
  CHECK(default_queue == nullptr);
  const cl_command_queue_properties profiled = CL_QUEUE_PROFILING_ENABLE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&profiled), sizeof profiled);
  cl_command_queue_properties properties = 0u;
  CHECK_EQ(table.clGetCommandQueueInfo(queue, CL_QUEUE_PROPERTIES, sizeof properties, &properties, nullptr),
           CL_SUCCESS);
  CHECK_EQ(properties, profiled);
  int not_a_queue = 0;
  CHECK_EQ(rig.root().queue_info(reinterpret_cast<cl_command_queue>(&not_a_queue), CL_QUEUE_CONTEXT, sizeof owner,
                                 &owner, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  const cl_mem buffer = rig.loader().create_buffer(context, CL_MEM_READ_WRITE, 64u, nullptr);
  CHECK_EQ(rig.loader().release_mem(buffer), CL_SUCCESS);
  CHECK_EQ(table.clGetCommandQueueInfo(queue, CL_QUEUE_PROPERTIES, sizeof properties, &properties, nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(table.clGetCommandQueueInfo(queue, CL_QUEUE_DEVICE, sizeof on, &on, nullptr), CL_SUCCESS);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, an_amd_adapter_answers_its_free_memory_from_windows)
{ ClIcdRig rig;
  rig.kmt().set_identity(0u, av::PciIdentity(0x1002u, 0x13C0u, av::PciIdentity::virtio_vendor,
                                             av::AvgpuAdapters::av_subsystem, 0xC9u));
  rig.kmt().describe_adapter(0u, av::PciLocation(0u, 17u, 0u), "AMD Radeon(TM) Graphics", 0u);
  rig.kmt().set_video_memory(av::MemorySegmentGroup::local, 512ull << 20u, 128ull << 20u);
  cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);
  if (device == nullptr) return;
  std::size_t free_kib[2] = {};
  std::size_t answered = 0u;
  CHECK_EQ(rig.loader().device_info(device, av::clicd::ClIcdRoot::free_memory_amd, sizeof free_kib, free_kib, &answered),
           CL_SUCCESS);
  CHECK_EQ(answered, sizeof free_kib);
  CHECK_EQ(free_kib[0], 384u * 1024u);
  CHECK_EQ(free_kib[1], 384u * 1024u);
  std::size_t total_kib = 0u;
  CHECK_EQ(rig.loader().device_info(device, av::clicd::ClIcdRoot::free_memory_amd, sizeof total_kib, &total_kib,
                                    &answered),
           CL_SUCCESS);
  CHECK_EQ(answered, sizeof total_kib);
  CHECK_EQ(total_kib, 384u * 1024u);
  answered = 0u;
  CHECK_EQ(rig.loader().device_info(device, av::clicd::ClIcdRoot::free_memory_amd, 0u, nullptr, &answered),
           CL_SUCCESS);
  CHECK_EQ(answered, sizeof total_kib);
  std::uint32_t narrow = 0u;
  CHECK_EQ(rig.loader().device_info(device, av::clicd::ClIcdRoot::free_memory_amd, sizeof narrow, &narrow, nullptr),
           CL_INVALID_VALUE);
  rig.kmt().set_video_memory(av::MemorySegmentGroup::local, 128ull << 20u, 512ull << 20u);
  CHECK_EQ(rig.loader().device_info(device, av::clicd::ClIcdRoot::free_memory_amd, sizeof free_kib, free_kib, nullptr),
           CL_SUCCESS);
  CHECK_EQ(free_kib[0], 0u);
  rig.kmt().fail_at(MockKmt::Call::query_video_memory, 1u, MockKmt::invalid_parameter);
  CHECK_EQ(rig.loader().device_info(device, av::clicd::ClIcdRoot::free_memory_amd, sizeof free_kib, free_kib, nullptr),
           CL_OUT_OF_RESOURCES); }
