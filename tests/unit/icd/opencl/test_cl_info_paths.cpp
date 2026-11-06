#include "core/support/growable_array.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "harness/icd/fake_cl_loader.hpp"
#include "icd/opencl/cl_destructor_callbacks.hpp"
#include "icd/opencl/cl_icd_root.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"

#include <CL/cl_ext.h>
#include <cstdint>
#include <cstring>

using av::ClIcdRig;
using av::FakeClLoader;

namespace {

constexpr std::uint64_t host_platform = 0x6200u;
constexpr std::uint64_t host_device = 0x6300u;
constexpr std::uint64_t host_context = 0x6400u;
constexpr std::uint64_t host_buffer = 0x6600u;
constexpr std::uint64_t host_sampler = 0x6A00u;

const cl_icd_dispatch *table_of(const void *object)
{
    return object != nullptr ? *reinterpret_cast<const cl_icd_dispatch *const *>(object) : nullptr;
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
    if (rig.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device, &devices) != CL_SUCCESS) return nullptr;
    return device;
}

void context_opens(ClIcdRig &rig)
{
    rig.host().arena_import(0x78u, 3u);
    rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
    rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
    rig.host().cl_handle(host_context, CL_SUCCESS);
}

cl_context opened_context(ClIcdRig &rig, cl_device_id &device)
{
    device = opened_device(rig);
    if (device == nullptr) return nullptr;
    context_opens(rig);
    cl_int error = CL_INVALID_VALUE;
    return rig.loader().create_context(device, &error);
}

class Destructions {
public:
    void note(char which, const void *object)
    {
        if (count_ < sizeof order_ - 1u) order_[count_++] = which;
        ++total_;
        last_ = object;
    }
    const char *order() const { return order_; }
    std::size_t total() const { return total_; }
    const void *last() const { return last_; }

private:
    char        order_[16] = {};
    std::size_t count_ = 0u;
    std::size_t total_ = 0u;
    const void *last_ = nullptr;
};

struct Tagged {
    Destructions *seen;
    char          which;
};

void CL_CALLBACK mem_went(cl_mem object, void *user_data)
{
    const Tagged &tag = *static_cast<const Tagged *>(user_data);
    tag.seen->note(tag.which, object);
}

void CL_CALLBACK context_went(cl_context object, void *user_data)
{
    const Tagged &tag = *static_cast<const Tagged *>(user_data);
    tag.seen->note(tag.which, object);
}

}

TEST(cl_info_paths, the_platform_has_no_host_timer_and_versions_its_own_extensions)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  const cl_platform_id   platform = rig.loader().platform(0u);
  const cl_icd_dispatch &table = *table_of(platform);
  cl_ulong resolution = 7u;
  std::size_t size = 0u;
  CHECK_EQ(table.clGetPlatformInfo(platform, CL_PLATFORM_HOST_TIMER_RESOLUTION, sizeof resolution, &resolution,
                                   &size),
           CL_SUCCESS);
  CHECK_EQ(resolution, 0u);
  CHECK_EQ(size, sizeof resolution);
  std::uint8_t short_room = 0u;
  CHECK_EQ(table.clGetPlatformInfo(platform, CL_PLATFORM_HOST_TIMER_RESOLUTION, sizeof short_room, &short_room,
                                   nullptr),
           CL_INVALID_VALUE);

  char named[512] = {};
  CHECK_EQ(table.clGetPlatformInfo(platform, CL_PLATFORM_EXTENSIONS, sizeof named, named, nullptr), CL_SUCCESS);
  std::size_t versions_bytes = 0u;
  CHECK_EQ(table.clGetPlatformInfo(platform, CL_PLATFORM_EXTENSIONS_WITH_VERSION, 0u, nullptr, &versions_bytes),
           CL_SUCCESS);
  CHECK(versions_bytes != 0u);
  CHECK_EQ(versions_bytes % sizeof(cl_name_version), 0u);
  cl_name_version versions[16] = {};
  CHECK(versions_bytes <= sizeof versions);
  CHECK_EQ(table.clGetPlatformInfo(platform, CL_PLATFORM_EXTENSIONS_WITH_VERSION, sizeof versions, versions,
                                   nullptr),
           CL_SUCCESS);
  char rebuilt[512] = {};
  for (std::size_t index = 0u; index < versions_bytes / sizeof versions[0]; ++index) {
      if (index != 0u) std::strcat(rebuilt, " ");
      std::strcat(rebuilt, versions[index].name);
      CHECK_EQ(versions[index].version, CL_MAKE_VERSION(1u, 0u, 0u));
  }
  CHECK_EQ(std::strcmp(rebuilt, named), 0);
  cl_name_version one = {};
  CHECK_EQ(table.clGetPlatformInfo(platform, CL_PLATFORM_EXTENSIONS_WITH_VERSION, sizeof one, &one, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(one.name[0], '\0');
  int not_a_platform = 0;
  CHECK_EQ(rig.root().platform_info(reinterpret_cast<cl_platform_id>(&not_a_platform),
                                    CL_PLATFORM_EXTENSIONS_WITH_VERSION, sizeof versions, versions, nullptr),
           CL_INVALID_PLATFORM); }

TEST(cl_info_paths, what_no_guest_can_reach_is_answered_absent_without_the_host)
{ ClIcdRig rig;
  const cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);
  if (device == nullptr) return;
  const std::uint32_t sent = rig.kmt().commands();

  cl_device_exec_capabilities exec = 0u;
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXECUTION_CAPABILITIES, sizeof exec, &exec, nullptr),
           CL_SUCCESS);
  CHECK_EQ(exec, static_cast<cl_device_exec_capabilities>(CL_EXEC_KERNEL));
  const cl_device_info words[] = {CL_DEVICE_PARTITION_MAX_SUB_DEVICES, CL_DEVICE_MAX_ON_DEVICE_QUEUES,
                                  CL_DEVICE_MAX_ON_DEVICE_EVENTS, CL_DEVICE_QUEUE_ON_DEVICE_PREFERRED_SIZE,
                                  CL_DEVICE_QUEUE_ON_DEVICE_MAX_SIZE};
  for (const cl_device_info name : words) {
      cl_uint value = 9u;
      std::size_t size = 0u;
      CHECK_EQ(rig.loader().device_info(device, name, sizeof value, &value, &size), CL_SUCCESS);
      CHECK_EQ(value, 0u);
      CHECK_EQ(size, sizeof value);
  }
  cl_device_partition_property partitions[2] = {7, 7};
  std::size_t size = 0u;
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_PARTITION_PROPERTIES, sizeof partitions, partitions, &size),
           CL_SUCCESS);
  CHECK_EQ(size, sizeof partitions[0]);
  CHECK_EQ(partitions[0], 0);
  cl_device_affinity_domain domain = 7u;
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_PARTITION_AFFINITY_DOMAIN, sizeof domain, &domain, nullptr),
           CL_SUCCESS);
  CHECK_EQ(domain, 0u);
  size = 7u;
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_PARTITION_TYPE, 0u, nullptr, &size), CL_SUCCESS);
  CHECK_EQ(size, 0u);
  cl_command_queue_properties queue_bits = 7u;
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_QUEUE_ON_DEVICE_PROPERTIES, sizeof queue_bits, &queue_bits,
                                    nullptr),
           CL_SUCCESS);
  CHECK_EQ(queue_bits, 0u);
  cl_device_device_enqueue_capabilities enqueue = 7u;
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_DEVICE_ENQUEUE_CAPABILITIES, sizeof enqueue, &enqueue,
                                    nullptr),
           CL_SUCCESS);
  CHECK_EQ(enqueue, 0u);
  std::uint8_t cramped = 0u;
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXECUTION_CAPABILITIES, sizeof cramped, &cramped, nullptr),
           CL_INVALID_VALUE);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK_EQ(rig.kmt().queued_answers(), 0u); }

TEST(cl_info_paths, the_versioned_extensions_are_the_policy_intersected_with_the_host)
{ cl_name_version versions[32] = {};
  { ClIcdRig rig;
  const cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);
  if (device == nullptr) return;
  static const char host[] = "cl_khr_fp16 cl_khr_spir cl_khr_3d_image_writes cl_khr_fp6";
  rig.host().cl_info(CL_SUCCESS, host, sizeof host);
  char named[512] = {};
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXTENSIONS, sizeof named, named, nullptr), CL_SUCCESS);
  CHECK(std::strstr(named, "cl_khr_fp16") != nullptr);
  CHECK(std::strstr(named, "cl_khr_3d_image_writes") != nullptr);
  CHECK(std::strstr(named, "cl_khr_spir") == nullptr);
  const std::uint32_t sent = rig.kmt().commands();
  std::size_t     size = 0u;
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXTENSIONS_WITH_VERSION, sizeof versions, versions, &size),
           CL_SUCCESS);
  CHECK_EQ(rig.kmt().commands(), sent);
  char rebuilt[512] = {};
  for (std::size_t index = 0u; index < size / sizeof versions[0]; ++index) {
      if (index != 0u) std::strcat(rebuilt, " ");
      std::strcat(rebuilt, versions[index].name);
      CHECK_EQ(versions[index].version, CL_MAKE_VERSION(1u, 0u, 0u));
  }
  CHECK_EQ(std::strcmp(rebuilt, named), 0); }
  { ClIcdRig refused;
    const cl_device_id other = opened_device(refused);
    refused.host().cl_info(CL_OUT_OF_RESOURCES, nullptr, 0u);
    CHECK_EQ(refused.loader().device_info(other, CL_DEVICE_EXTENSIONS_WITH_VERSION, sizeof versions, versions,
                                          nullptr),
             CL_OUT_OF_RESOURCES); } }

TEST(cl_info_paths, the_c_features_are_the_host_compiler_s_without_device_enqueue)
{ cl_name_version features[4] = {};
  { ClIcdRig rig;
  const cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);
  if (device == nullptr) return;
  cl_name_version host[3] = {};
  std::strcpy(host[0].name, "__opencl_c_images");
  host[0].version = CL_MAKE_VERSION(3u, 0u, 0u);
  std::strcpy(host[1].name, "__opencl_c_device_enqueue");
  host[1].version = CL_MAKE_VERSION(3u, 0u, 0u);
  std::strcpy(host[2].name, "__opencl_c_device_enqueue_x");
  host[2].version = CL_MAKE_VERSION(3u, 0u, 0u);
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(host), sizeof host);
  std::size_t     size = 0u;
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_OPENCL_C_FEATURES, sizeof features, features, &size),
           CL_SUCCESS);
  CHECK_EQ(size, 2u * sizeof features[0]);
  CHECK_EQ(std::strcmp(features[0].name, "__opencl_c_images"), 0);
  CHECK_EQ(std::strcmp(features[1].name, "__opencl_c_device_enqueue_x"), 0);
  CHECK_EQ(features[1].version, CL_MAKE_VERSION(3u, 0u, 0u)); }
  { ClIcdRig older;
    const cl_device_id other = opened_device(older);
    older.host().cl_info(CL_INVALID_VALUE, nullptr, 0u);
    older.host().cl_info(CL_INVALID_VALUE, nullptr, 0u);
    CHECK_EQ(older.loader().device_info(other, CL_DEVICE_OPENCL_C_FEATURES, sizeof features, features, nullptr),
             CL_INVALID_VALUE); } }

TEST(cl_info_paths, a_device_queue_or_its_size_is_refused_before_the_host)
{ ClIcdRig   rig;
  cl_device_id device = nullptr;
  const cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  if (context == nullptr) return;
  const std::uint32_t sent = rig.kmt().commands();
  cl_int error = CL_SUCCESS;
  const std::uint64_t on_device[] = {static_cast<std::uint64_t>(CL_QUEUE_PROPERTIES),
                                     CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0u};
  CHECK(rig.root().create_queue(context, device, on_device, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_QUEUE_PROPERTIES);
  const std::uint64_t sized[] = {static_cast<std::uint64_t>(CL_QUEUE_SIZE), 16u, 0u};
  CHECK(rig.root().create_queue(context, device, sized, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  const std::uint64_t unknown_bit[] = {static_cast<std::uint64_t>(CL_QUEUE_PROPERTIES), 1u << 12u, 0u};
  CHECK(rig.root().create_queue(context, device, unknown_bit, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_VALUE);
  CHECK(table_of(context)->clCreateCommandQueue(context, device, CL_QUEUE_ON_DEVICE_DEFAULT, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_QUEUE_PROPERTIES);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK_EQ(rig.loader().release_context(context), CL_SUCCESS); }

TEST(cl_info_paths, a_sub_device_is_refused_as_a_partition_the_device_does_not_have)
{ ClIcdRig rig;
  const cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);
  if (device == nullptr) return;
  const cl_device_partition_property equally[] = {CL_DEVICE_PARTITION_EQUALLY, 1, 0};
  cl_device_id out[2] = {};
  cl_uint      made = 7u;
  CHECK_EQ(table_of(device)->clCreateSubDevices(device, equally, 2u, out, &made), CL_INVALID_VALUE);
  CHECK(out[0] == nullptr);
  CHECK_EQ(made, 7u);
  int not_a_device = 0;
  CHECK_EQ(table_of(device)->clCreateSubDevices(reinterpret_cast<cl_device_id>(&not_a_device), equally, 2u, out,
                                                &made),
           CL_INVALID_DEVICE); }

TEST(cl_info_paths, the_per_platform_lookup_answers_for_this_platform_alone)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  const cl_platform_id platform = rig.loader().platform(0u);
  const auto           lookup = table_of(platform)->clGetExtensionFunctionAddressForPlatform;
  CHECK(lookup != nullptr);
  void *const found = lookup(platform, "clIcdGetPlatformIDsKHR");
  CHECK(found != nullptr);
  CHECK(found == rig.root().extension_function("clIcdGetPlatformIDsKHR"));
  CHECK(lookup(platform, "clNoSuchFunction") == nullptr);
  int not_a_platform = 0;
  CHECK(lookup(reinterpret_cast<cl_platform_id>(&not_a_platform), "clIcdGetPlatformIDsKHR") == nullptr); }

TEST(cl_info_paths, a_context_from_a_type_is_the_first_device_s_and_other_types_find_none)
{ ClIcdRig rig;
  const cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);
  if (device == nullptr) return;
  const cl_icd_dispatch &table = *table_of(device);
  const cl_device_type   found_types[] = {CL_DEVICE_TYPE_GPU, CL_DEVICE_TYPE_DEFAULT, CL_DEVICE_TYPE_ALL};
  for (const cl_device_type type : found_types) {
      context_opens(rig);
      cl_int error = CL_INVALID_VALUE;
      const cl_context context = table.clCreateContextFromType(nullptr, type, nullptr, nullptr, &error);
      CHECK_EQ(error, CL_SUCCESS);
      CHECK(context != nullptr);
      cl_device_id on = nullptr;
      CHECK_EQ(table.clGetContextInfo(context, CL_CONTEXT_DEVICES, sizeof on, &on, nullptr), CL_SUCCESS);
      CHECK(on == device);
      CHECK_EQ(rig.loader().release_context(context), CL_SUCCESS);
  }
  const std::uint32_t sent = rig.kmt().commands();
  cl_int error = CL_SUCCESS;
  CHECK(table.clCreateContextFromType(nullptr, CL_DEVICE_TYPE_CPU, nullptr, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_DEVICE_NOT_FOUND);
  CHECK(table.clCreateContextFromType(nullptr, 0u, nullptr, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_DEVICE_TYPE);
  CHECK(table.clCreateContextFromType(nullptr, 0u, nullptr, nullptr, nullptr) == nullptr);
  const cl_context_properties unknown[] = {0x4242, 1, 0};
  CHECK(table.clCreateContextFromType(unknown, CL_DEVICE_TYPE_GPU, nullptr, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PROPERTY);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_info_paths, a_sampler_s_count_and_context_are_the_icd_s_and_its_modes_the_host_s)
{ ClIcdRig   rig;
  cl_device_id device = nullptr;
  const cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  if (context == nullptr) return;
  rig.host().cl_handle(host_sampler, CL_SUCCESS);
  cl_int error = CL_INVALID_VALUE;
  const cl_sampler sampler =
      table_of(context)->clCreateSampler(context, CL_TRUE, CL_ADDRESS_CLAMP, CL_FILTER_NEAREST, &error);
  CHECK(sampler != nullptr);
  if (sampler == nullptr) return;
  const cl_icd_dispatch &table = *table_of(sampler);
  CHECK_EQ(table.clRetainSampler(sampler), CL_SUCCESS);
  const std::uint32_t sent = rig.kmt().commands();
  cl_uint count = 0u;
  CHECK_EQ(table.clGetSamplerInfo(sampler, CL_SAMPLER_REFERENCE_COUNT, sizeof count, &count, nullptr), CL_SUCCESS);
  CHECK_EQ(count, 2u);
  cl_context owner = nullptr;
  CHECK_EQ(table.clGetSamplerInfo(sampler, CL_SAMPLER_CONTEXT, sizeof owner, &owner, nullptr), CL_SUCCESS);
  CHECK(owner == context);
  CHECK_EQ(rig.kmt().commands(), sent);
  const cl_addressing_mode clamp = CL_ADDRESS_CLAMP;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&clamp), sizeof clamp);
  cl_addressing_mode mode = 0u;
  CHECK_EQ(table.clGetSamplerInfo(sampler, CL_SAMPLER_ADDRESSING_MODE, sizeof mode, &mode, nullptr), CL_SUCCESS);
  CHECK_EQ(mode, clamp);
  CHECK_EQ(rig.kmt().command_id(sent), static_cast<std::uint32_t>(av::clproto::kCmd_clGetSamplerInfo));
  int not_a_sampler = 0;
  CHECK_EQ(rig.root().sampler_info(reinterpret_cast<cl_sampler>(&not_a_sampler), CL_SAMPLER_CONTEXT, sizeof owner,
                                   &owner, nullptr),
           CL_INVALID_SAMPLER); }

TEST(cl_info_paths, destructor_callbacks_run_latest_first_when_the_object_goes)
{ ClIcdRig   rig;
  cl_device_id device = nullptr;
  const cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  if (context == nullptr) return;
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  const cl_mem buffer = rig.loader().create_buffer(context, CL_MEM_READ_WRITE, 64u, nullptr);
  CHECK(buffer != nullptr);
  if (buffer == nullptr) return;
  Destructions seen;
  Tagged       first{&seen, 'a'};
  Tagged       second{&seen, 'b'};
  Tagged       of_context{&seen, 'c'};
  const cl_icd_dispatch &table = *table_of(buffer);
  CHECK_EQ(table.clSetMemObjectDestructorCallback(buffer, &mem_went, &first), CL_SUCCESS);
  CHECK_EQ(table.clSetMemObjectDestructorCallback(buffer, &mem_went, &second), CL_SUCCESS);
  CHECK_EQ(table.clSetContextDestructorCallback(context, &context_went, &of_context), CL_SUCCESS);
  CHECK_EQ(rig.loader().release_context(context), CL_SUCCESS);
  CHECK_EQ(std::strcmp(seen.order(), ""), 0);
  CHECK_EQ(table.clRetainMemObject(buffer), CL_SUCCESS);
  CHECK_EQ(rig.loader().release_mem(buffer), CL_SUCCESS);
  CHECK_EQ(std::strcmp(seen.order(), ""), 0);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.loader().release_mem(buffer), CL_SUCCESS);
  CHECK_EQ(std::strcmp(seen.order(), "bac"), 0);
  CHECK(seen.last() == context);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_info_paths, a_destructor_callback_is_refused_for_no_object_no_callback_or_no_room)
{ ClIcdRig   rig;
  cl_device_id device = nullptr;
  const cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  if (context == nullptr) return;
  const cl_icd_dispatch &table = *table_of(context);
  Destructions seen;
  Tagged       tag{&seen, 'x'};
  CHECK_EQ(table.clSetContextDestructorCallback(context, nullptr, &tag), CL_INVALID_VALUE);
  CHECK_EQ(table.clSetMemObjectDestructorCallback(reinterpret_cast<cl_mem>(context), &mem_went, &tag),
           CL_INVALID_MEM_OBJECT);
  int not_a_context = 0;
  CHECK_EQ(table.clSetContextDestructorCallback(reinterpret_cast<cl_context>(&not_a_context), &context_went, &tag),
           CL_INVALID_CONTEXT);
  rig.host().cl_handle(host_buffer, CL_SUCCESS);
  const cl_mem buffer = rig.loader().create_buffer(context, CL_MEM_READ_WRITE, 64u, nullptr);
  CHECK_EQ(table.clSetMemObjectDestructorCallback(buffer, nullptr, &tag), CL_INVALID_VALUE);
  constexpr std::uint32_t rows = 2u * av::GrowableArray<char>::first_capacity;
  for (std::uint32_t index = 0u; index < rows; ++index)
      CHECK_EQ(table.clSetContextDestructorCallback(context, &context_went, &tag), CL_SUCCESS);
  rig.platform().fail_allocation_at(1u);
  CHECK_EQ(table.clSetMemObjectDestructorCallback(buffer, &mem_went, &tag), CL_OUT_OF_HOST_MEMORY);
  rig.platform().fail_allocation_at(1u);
  CHECK_EQ(table.clSetContextDestructorCallback(context, &context_went, &tag), CL_OUT_OF_HOST_MEMORY);
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.loader().release_mem(buffer), CL_SUCCESS);
  CHECK_EQ(std::strcmp(seen.order(), ""), 0);
  CHECK_EQ(rig.loader().release_context(context), CL_SUCCESS);
  CHECK_EQ(seen.total(), rows);
  CHECK_EQ(std::strlen(seen.order()), 15u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_info_paths, the_new_entries_refuse_before_there_is_a_root)
{ av::clicd::ClDispatchTable dispatch;
  dispatch.fill();
  const cl_icd_dispatch &table = dispatch.table();
  cl_int error = CL_SUCCESS;
  CHECK(table.clCreateContextFromType(nullptr, CL_DEVICE_TYPE_GPU, nullptr, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PLATFORM);
  CHECK_EQ(table.clGetSamplerInfo(nullptr, CL_SAMPLER_CONTEXT, 0u, nullptr, nullptr), CL_INVALID_SAMPLER);
  CHECK_EQ(table.clSetMemObjectDestructorCallback(nullptr, &mem_went, nullptr), CL_INVALID_MEM_OBJECT);
  CHECK_EQ(table.clSetContextDestructorCallback(nullptr, &context_went, nullptr), CL_INVALID_CONTEXT);
  CHECK_EQ(table.clCreateSubDevices(nullptr, nullptr, 0u, nullptr, nullptr), CL_INVALID_DEVICE);
  CHECK(table.clGetExtensionFunctionAddressForPlatform(nullptr, "clIcdGetPlatformIDsKHR") == nullptr); }

TEST(cl_info_paths, watch_destruction_and_extension_function_for_judge_their_objects)
{ ClIcdRig   rig;
  cl_device_id device = nullptr;
  const cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  if (context == nullptr) return;
  Destructions seen;
  Tagged       tag{&seen, 'z'};
  CHECK_EQ(rig.root().watch_destruction(context, &context_went, &tag), CL_SUCCESS);
  CHECK_EQ(rig.root().watch_destruction(static_cast<cl_mem>(nullptr), &mem_went, &tag), CL_INVALID_MEM_OBJECT);
  CHECK(rig.root().extension_function_for(rig.loader().platform(0u), "clGetGLContextInfoKHR") != nullptr);
  CHECK(rig.root().extension_function_for(nullptr, "clGetGLContextInfoKHR") == nullptr);
  CHECK_EQ(rig.root().release_context(context), CL_SUCCESS);
  CHECK_EQ(std::strcmp(seen.order(), "z"), 0); }
