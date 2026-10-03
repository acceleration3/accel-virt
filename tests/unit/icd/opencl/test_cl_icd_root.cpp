/* What every entry point the loader reaches answers, driven through the
 * dispatch table rather than by name.
 *
 * The shape of an OpenCL ICD is that the application holds pointers and the
 * host holds numbers, so most of what is asserted here is the translation and
 * its refusals: an object of the wrong kind, an object that was released, a
 * context whose device is gone. The API has a distinct error for each, and an
 * application is entitled to be told which one - answering CL_INVALID_VALUE
 * for everything would pass a naive test and fail a real program.
 *
 * The host is played by ScriptedHost: every round trip takes one queued
 * answer, so a test that queues none is a test of what happens when the device
 * says nothing. */
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "icd/opencl/cl_property_list.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"

#include <CL/cl_ext.h>

#include <cstring>

using av::ClIcdRig;
using av::FakeClLoader;
using av::MockKmt;
using av::clicd::ClObject;
using av::clicd::ClPropertyList;

namespace {

/* The host's own numbers for the objects a run makes, so a test can say which
 * one the ICD was answered with. */
constexpr std::uint64_t host_platform = 0x6200u;
constexpr std::uint64_t host_device = 0x6300u;
constexpr std::uint64_t host_context = 0x6400u;
constexpr std::uint64_t host_queue = 0x6500u;
constexpr std::uint64_t host_buffer = 0x6600u;
constexpr std::uint64_t host_program = 0x6700u;
constexpr std::uint64_t host_kernel = 0x6800u;

/* What the device answers while a device is being opened: the arena's import,
 * then the platform and device the ICD asks for, then the version queries it
 * caps. */
void device_opens(ClIcdRig &rig)
{
    rig.host().blob_import(0u, 0x77u);
    rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
    rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
    rig.host().cl_info(CL_SUCCESS, "OpenCL 3.0 host", 16u);
    rig.host().cl_info(CL_SUCCESS, "OpenCL C 1.2", 13u);
    rig.host().cl_info(CL_SUCCESS, "", 1u);
}

cl_device_id opened_device(ClIcdRig &rig)
{
    /* The loader keeps the driver first; everything an application does comes
     * through a platform it enumerated. */
    if (rig.loader().load() != FakeClLoader::Load::kept) return nullptr;
    device_opens(rig);
    cl_device_id device = nullptr;
    cl_uint      devices = 0u;
    if (rig.loader().device_ids(CL_DEVICE_TYPE_GPU, 1u, &device, &devices) != CL_SUCCESS) {
        return nullptr;
    }
    return device;
}

}  // namespace

TEST(cl_icd_root, a_new_root_is_bound_and_has_opened_nothing)
{ ClIcdRig rig;
  CHECK(rig.root().invariants_hold());
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  /* The platform is answerable before anything else exists. */
  cl_uint count = 0u;
  CHECK_EQ(rig.root().platform_ids(0u, nullptr, &count), CL_SUCCESS);
  CHECK_EQ(count, 1u); }

TEST(cl_icd_root, asking_for_a_device_is_what_opens_the_adapter)
{ ClIcdRig rig;
  CHECK_EQ(rig.kmt().calls(MockKmt::Call::enumerate_adapters), 0u);
  CHECK(opened_device(rig) != nullptr);
  /* An application that asked for a device has asked for a GPU, which is the
   * point at which opening one is what it wanted. */
  CHECK(rig.kmt().calls(MockKmt::Call::enumerate_adapters) > 0u);
  CHECK(rig.kmt().open_objects() > 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_device_type_this_adapter_is_not_finds_nothing)
{ ClIcdRig rig;
  CHECK(rig.loader().load() == FakeClLoader::Load::kept);
  cl_device_id device = nullptr;
  cl_uint      devices = 1u;
  /* This adapter's device is a GPU. A program asking for a CPU is told there
   * is none, which is the API's own answer and not an error in the ICD. */
  CHECK_EQ(rig.loader().device_ids(CL_DEVICE_TYPE_CPU, 1u, &device, &devices), CL_DEVICE_NOT_FOUND);
  CHECK_EQ(devices, 0u);
  /* And nothing was opened to find that out. */
  CHECK_EQ(rig.kmt().open_objects(), 0u);
  /* A type of no bits at all is a different mistake. */
  CHECK_EQ(rig.loader().device_ids(0u, 1u, &device, &devices), CL_INVALID_DEVICE_TYPE); }

TEST(cl_icd_root, the_device_answers_the_adapters_identity_and_the_hosts_everything_else)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);

  /* Decided here: the LUID is the avgpu adapter's, not the host GPU's - the
   * root tells the report which adapter it opened (observe_luid), because a
   * report built before there was an adapter has nothing to name. */
  cl_uint vendor = 0u;
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_VENDOR_ID, sizeof vendor, &vendor, nullptr),
           CL_SUCCESS);
  CHECK(vendor != 0u);
  std::uint8_t luid[CL_LUID_SIZE_KHR] = {};
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_LUID_KHR, sizeof luid, luid, nullptr),
           CL_SUCCESS);
  /* Both halves of the rig's LUID are there, so a dropped or swapped half
   * shows. */
  CHECK(luid[0] != 0u || luid[1] != 0u || luid[2] != 0u || luid[3] != 0u);
  CHECK(luid[4] != 0u || luid[5] != 0u || luid[6] != 0u || luid[7] != 0u);

  /* Forwarded: the name is the host GPU's, and the ICD asks for it. */
  rig.host().cl_info(CL_SUCCESS, "a host gpu", 11u);
  char name[32] = {};
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_NAME, sizeof name, name, nullptr), CL_SUCCESS);
  CHECK_EQ(std::strcmp(name, "a host gpu"), 0); }

/* A device's platform and parent are objects, and the host's handles for them
 * are nothing in this process: an application hands the platform straight
 * back - Affinity to clGetDeviceIDsFromD3D10KHR, through a loader that reads
 * its dispatch table - so it is this ICD's own platform, and a root device
 * has no parent, whatever the host would say. */
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
  /* Room for less than a handle is refused rather than filled in part. */
  char cramped[4] = {};
  CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_PLATFORM, sizeof cramped, cramped, nullptr), CL_INVALID_VALUE); }

/* CL_DEVICE_EXTENSIONS is the one query that is neither the report's answer
 * nor the host's: the host says what the GPU has, a policy row says what this
 * ICD carries, and an application is shown the two intersected. Forwarding the
 * host's would offer an application every extension rusticl implements over a
 * wire that carries none of them (constitution VI).
 *
 * The two rows are the external-memory pair, and both are answered over a host
 * extension of another name: a Linux OpenCL has no Win32 handle to take, and
 * the device opens the descriptor through the host's `cl_khr_gl_sharing`
 * instead (research.md §21). So a host that lists that name is shown them, a
 * host that does not is shown nothing, and neither is ever shown the host's
 * own list. */
TEST(cl_icd_root, the_extensions_are_the_policy_intersected_with_the_host_not_the_host_s_own)
{ /* A host naming extensions this ICD has no row for, including one whose
   * name is a prefix of another so a match on anything but a whole word
   * shows, and no cl_khr_gl_sharing: nothing of it reaches the application,
   * which is shown only what this ICD answers alone. */
  static const char plain[] = "cl_khr_fp64 cl_khr_il_program cl_khr_il cl_khr_gl_shar";
  char              reported[128];
  std::size_t       answered = 0u;
  { ClIcdRig rig;
    cl_device_id device = opened_device(rig);
    CHECK(device != nullptr);
    rig.host().cl_info(CL_SUCCESS, plain, sizeof plain);
    std::memset(reported, 'x', sizeof reported);
    CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXTENSIONS, sizeof reported, reported, &answered),
             CL_SUCCESS);
    /* Nothing of the host's reached the application, and what it got is a
     * terminated string rather than untouched room. */
    CHECK_EQ(std::strcmp(reported, "cl_khr_d3d10_sharing"), 0);
    CHECK_EQ(answered, std::strlen("cl_khr_d3d10_sharing") + 1u);
    CHECK(std::strstr(plain, "cl_khr_fp64") != nullptr);

    /* The size query, which an application makes first, and room the answer
     * does not fit, which is the API's own refusal and not a truncation. A
     * device's list cannot change, so neither asks the host again. */
    const std::uint32_t sent = rig.kmt().commands();
    std::size_t         wanted = 0u;
    CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXTENSIONS, 0u, nullptr, &wanted), CL_SUCCESS);
    CHECK_EQ(wanted, std::strlen("cl_khr_d3d10_sharing") + 1u);
    char cramped[1] = {'x'};
    CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXTENSIONS, 0u, cramped, nullptr), CL_INVALID_VALUE);
    CHECK_EQ(cramped[0], 'x');
    CHECK_EQ(rig.kmt().commands(), sent); }

  /* A host with the one name the rows are answered over: the two
   * external-memory rows and GL sharing itself are reported, in the policy's
   * order, and still nothing else of the host's. */
  { ClIcdRig rig;
    cl_device_id device = opened_device(rig);
    static const char sharing[] = "cl_khr_fp64 cl_khr_gl_sharing cl_khr_il";
    rig.host().cl_info(CL_SUCCESS, sharing, sizeof sharing);
    std::memset(reported, 'x', sizeof reported);
    CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXTENSIONS, sizeof reported, reported, &answered),
             CL_SUCCESS);
    CHECK_EQ(std::strcmp(reported,
                         "cl_khr_external_memory cl_khr_external_memory_win32 cl_khr_gl_sharing cl_khr_d3d10_sharing"),
             0);
    CHECK_EQ(answered, std::strlen(reported) + 1u); }

  /* And a host that will not answer is the host's status, not an empty list:
   * an ICD that reported "no extensions" for a device it could not ask would
   * be answering for a device it has lost. Nothing is kept of it, so the next
   * query asks again. */
  { ClIcdRig rig;
    cl_device_id device = opened_device(rig);
    rig.host().cl_info(CL_OUT_OF_RESOURCES, nullptr, 0u);
    CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXTENSIONS, sizeof reported, reported, nullptr),
             CL_OUT_OF_RESOURCES);
    rig.host().cl_info(CL_SUCCESS, plain, sizeof plain);
    CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXTENSIONS, sizeof reported, reported, nullptr),
             CL_SUCCESS);
    CHECK_EQ(rig.kmt().queued_answers(), 0u); } }

/* The query that goes with the rows: what handle types an import may name.
 * This driver takes the opaque Win32 handle and nothing else - a guest has no
 * file descriptors, which is the whole reason the Win32 names exist - and it
 * answers at all only where it reports the extension that defines the query. */
TEST(cl_icd_root, the_import_handle_types_are_this_drivers_own_and_only_where_it_reports_them)
{ { ClIcdRig rig;
    cl_device_id device = opened_device(rig);
    CHECK(device != nullptr);
    static const char sharing[] = "cl_khr_gl_sharing";
    rig.host().cl_info(CL_SUCCESS, sharing, sizeof sharing);
    cl_external_memory_handle_type_khr taken = 0u;
    std::size_t                        answered = 0u;
    CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR, sizeof taken,
                                      &taken, &answered),
             CL_SUCCESS);
    CHECK_EQ(taken, static_cast<cl_external_memory_handle_type_khr>(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR));
    CHECK_EQ(answered, sizeof taken);

    /* The size query, and room it does not fit: the device's list was kept,
     * so neither asks the host. */
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

  /* A host this driver reports no external memory over has no such query:
   * answering one would be answering for an extension nobody was offered. */
  cl_external_memory_handle_type_khr taken = 0u;
  { ClIcdRig rig;
    cl_device_id device = opened_device(rig);
    static const char plain[] = "cl_khr_fp64";
    rig.host().cl_info(CL_SUCCESS, plain, sizeof plain);
    CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR, sizeof taken,
                                      &taken, nullptr),
             CL_INVALID_VALUE); }
  /* And a host that will not say is its own status. */
  { ClIcdRig rig;
    cl_device_id device = opened_device(rig);
    rig.host().cl_info(CL_OUT_OF_RESOURCES, nullptr, 0u);
    CHECK_EQ(rig.loader().device_info(device, CL_DEVICE_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR, sizeof taken,
                                      &taken, nullptr),
             CL_OUT_OF_RESOURCES); } }

/* Every other device query is the host's answer, asked once: a device's
 * properties cannot change, and an application that asks them on every
 * operation - Affinity, from every worker at once - would otherwise wait a
 * round trip each time. The host is asked with room for the longest answer
 * kept, so a caller's size query and its fetch are both answered from it. A
 * name the host refuses as an invalid value is asked again as the caller put
 * it - the host's own answer to that - and kept nothing of. */
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
  /* The ICD never reads through a pointer to judge it, so even a four-byte
   * stack object is safe to hand it - which is what makes the refusal a
   * refusal rather than a crash. */
  CHECK_EQ(rig.root().device_info(reinterpret_cast<cl_device_id>(&elsewhere), CL_DEVICE_NAME,
                                  sizeof name, name, nullptr),
           CL_INVALID_DEVICE);
  CHECK_EQ(rig.root().device_info(nullptr, CL_DEVICE_NAME, sizeof name, name, nullptr),
           CL_INVALID_DEVICE); }

TEST(cl_icd_root, a_context_is_made_on_the_device_and_carries_the_dispatch_table)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);

  /* The context's own transport opens, then the host is asked for its
   * platform, its device and a context on it. */
  rig.host().blob_import(0u, 0x77u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(host_context, CL_SUCCESS);
  cl_int     error = CL_INVALID_VALUE;
  cl_context context = rig.loader().create_context(device, &error);
  CHECK(context != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK(rig.loader().dispatch_of(context) == &rig.root().dispatch());
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_context_made_from_something_that_is_not_a_device_is_refused)
{ ClIcdRig rig;
  CHECK(opened_device(rig) != nullptr);
  /* Pointer-sized, and its first word is null: the loader reads that word
   * before it dispatches, so this is the shape of a forged object a loader
   * can survive being handed. A four-byte one it cannot, which is a true
   * thing about the ABI and why the entries below are asked of the root -
   * the ICD judges a pointer without reading through it, and the loader
   * never gets that far. */
  void  *not_an_object = nullptr;
  cl_int error = CL_SUCCESS;
  CHECK(rig.loader().create_context(reinterpret_cast<cl_device_id>(&not_an_object), &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_DEVICE);
  /* And with no device at all. */
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
  rig.host().blob_import(0u, 0x77u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(host_context, CL_SUCCESS);
  cl_context context = rig.loader().create_context(device, nullptr);
  CHECK(context != nullptr);

  CHECK_EQ(rig.root().retain_context(context), CL_SUCCESS);
  CHECK_EQ(rig.root().release_context(context), CL_SUCCESS);
  /* Still alive: the application holds one more reference. */
  CHECK_EQ(rig.root().retain_context(context), CL_SUCCESS);
  CHECK_EQ(rig.root().release_context(context), CL_SUCCESS);
  CHECK_EQ(rig.root().release_context(context), CL_SUCCESS);
  /* Gone, and the pointer no longer names anything. */
  CHECK_EQ(rig.root().release_context(context), CL_INVALID_CONTEXT);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, every_kind_has_its_own_refusal_rather_than_one_for_all_of_them)
{ ClIcdRig rig;
  int elsewhere = 0;
  void *const nothing = &elsewhere;
  /* An application that passed the wrong object is entitled to be told which
   * one it was. */
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
  rig.host().blob_import(0u, 0x77u);
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

  /* Each carries the table, and each belongs to the context. */
  CHECK(rig.loader().dispatch_of(queue) == &rig.root().dispatch());
  CHECK(rig.loader().dispatch_of(buffer) == &rig.root().dispatch());
  CHECK(rig.loader().dispatch_of(kernel) == &rig.root().dispatch());
  CHECK(rig.root().invariants_hold());

  /* Releasing the context leaves what the application still holds, as
   * OpenCL's objects hold their context: each is still its own to use and to
   * release, and the context itself is no longer. */
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
  /* Gone now, with the last of them. */
  CHECK_EQ(rig.root().release(kernel, ClObject::Kind::kernel), CL_INVALID_KERNEL);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_buffer_of_no_bytes_is_refused_before_the_host_is_asked)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  rig.host().blob_import(0u, 0x77u);
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
  /* A real object of this ICD, passed where another kind belongs. The
   * pointer is live and readable, so nothing but the kind distinguishes
   * them. */
  CHECK_EQ(rig.root().retain(device, ClObject::Kind::context), CL_INVALID_CONTEXT);
  CHECK_EQ(rig.root().retain(device, ClObject::Kind::mem), CL_INVALID_MEM_OBJECT);
  CHECK_EQ(rig.root().retain(device, ClObject::Kind::device), CL_SUCCESS);
  CHECK_EQ(rig.root().release(device, ClObject::Kind::device), CL_SUCCESS); }

TEST(cl_icd_root, a_program_built_from_nothing_is_refused_and_a_kernel_needs_a_name)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  rig.host().blob_import(0u, 0x77u);
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
  CHECK_EQ(rig.root().build_program(static_cast<cl_program>(nothing), nullptr), CL_INVALID_PROGRAM); }

TEST(cl_icd_root, a_state_dump_says_what_the_process_holds)
{ ClIcdRig rig;
  CHECK(opened_device(rig) != nullptr);
  CHECK(rig.root().invariants_hold()); }

/* ---- shared virtual memory and maps (US2) ---------------------------------
 *
 * The application holds mirrors and the wire carries host addresses, so what
 * is asserted here is the translation and what happens to a pointer that is
 * inside no allocation - which never reaches the wire at all.
 */

TEST(cl_icd_root, svm_alloc_hands_back_a_mirror_and_svm_free_takes_it_back)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  rig.host().blob_import(0u, 0x77u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(host_context, CL_SUCCESS);
  cl_context context = rig.loader().create_context(device, nullptr);
  CHECK(context != nullptr);

  /* The host allocated at an address of its own; the application is handed a
   * mirror, which is a pointer in this process it may dereference. */
  constexpr std::uint64_t host_svm = 0x0000'7F22'0000'3000ull;
  rig.host().cl_handle(host_svm, CL_SUCCESS);
  void *const mirror = rig.root().svm_alloc(context, 0u, 256u, 0u);
  CHECK(mirror != nullptr);
  CHECK(mirror != reinterpret_cast<void *>(host_svm));
  /* Written through, which is the whole point of a mirror: the application
   * dereferences this, and the host address it stands for it never sees. The
   * bytes are not zeroed - clSVMAlloc promises nothing about them either. */
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
  rig.host().blob_import(0u, 0x77u);
  rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
  rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
  rig.host().cl_handle(host_context, CL_SUCCESS);
  cl_context context = rig.loader().create_context(device, nullptr);
  CHECK(rig.root().svm_alloc(context, 0u, 0u, 0u) == nullptr);
  void *not_an_object = nullptr;
  CHECK(rig.root().svm_alloc(reinterpret_cast<cl_context>(&not_an_object), 0u, 64u, 0u) == nullptr);
  /* And freeing something that was never allocated frees nothing. */
  int elsewhere = 0;
  rig.root().svm_free(context, &elsewhere);
  rig.root().svm_free(context, nullptr);
  CHECK(rig.root().invariants_hold()); }

TEST(cl_icd_root, a_kernel_argument_is_translated_and_an_invented_address_never_reaches_the_wire)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  rig.host().blob_import(0u, 0x77u);
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

  /* An address inside the allocation is a legal kernel argument, and what
   * crosses is the host address of that same byte. */
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().set_kernel_svm_argument(kernel, 0u, mirror + 64u), CL_SUCCESS);

  /* One inside no allocation is the application's mistake, and it is refused
   * here rather than sent for the device to refuse. */
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
  rig.host().blob_import(0u, 0x77u);
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
  /* And on something that is not a queue at all. */
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
  rig.host().blob_import(0u, 0x77u);
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

/* ---- events (US3) ------------------------------------------------------- */

namespace av::clicd {

/* The object table is private, and how many events are live is what the
 * retain a registration holds is visible as. */
struct ClIcdRootProbe {
    /* The root's invariant is the conjunction of everything it owns, so each
     * part is reached through here and broken with its own probe: a root that
     * believed itself sound while one of them was not would be a driver
     * answering calls over a table it cannot trust. */
    static ClPlatform        &platform(ClIcdRoot &root) { return root.platform_; }
    static ClExtensionPolicy &extensions(ClIcdRoot &root) { return root.extensions_; }
    static ClObjectTable     &objects_of(ClIcdRoot &root) { return root.objects_; }
    static icd::IcdConnection &connection(ClIcdRoot &root) { return root.connection_; }
    static ClDeviceReport    &device(ClIcdRoot &root) { return root.device_; }
    static ClPendingReads    &reads_of(ClIcdRoot &root) { return root.reads_; }
    static ClEventWatcher    &watcher(ClIcdRoot &root) { return root.watcher_; }

    static std::uint32_t live_of(ClIcdRoot &root, ClObject::Kind kind)
    { return root.objects_.live_of(kind); }
    static ClObject *made(ClIcdRoot &root, const void *pointer, ClObject::Kind kind)
    { return root.objects_.made(pointer, kind); }
    static std::uint32_t outstanding_reads(ClIcdRoot &root) { return root.reads_.outstanding(); }
    /* An object with no context behind it. Nothing the ICD does makes one -
     * every object it hands out is made on a context, and the device's is the
     * one it opened for itself - but every entry that reaches for a
     * transport guards against it, and this is what drives those guards. */
    static ClObject *without_a_context(ClIcdRoot &root, ClObject::Kind kind, std::uint64_t wire)
    { ClObject *made = nullptr;
      return root.objects_.make(kind, wire, nullptr, nullptr, made) == Status::ok ? made
                                                                                  : nullptr; }
};

}  // namespace av::clicd

using av::clicd::ClIcdRootProbe;

namespace {

/* A callback that never runs: what is tested is the registration. */
void nothing_ran(cl_event, cl_int, void *) {}

}  // namespace

namespace {

constexpr std::uint64_t host_event = 0x6900u;

/* What a callback saw, so a test can say it ran once and with what. */
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
    rig.host().blob_import(0u, 0x77u);
    rig.host().cl_ids(CL_SUCCESS, &host_platform, 1u, 1u);
    rig.host().cl_ids(CL_SUCCESS, &host_device, 1u, 1u);
    rig.host().cl_handle(host_context, CL_SUCCESS);
    cl_int error = CL_INVALID_VALUE;
    return rig.loader().create_context(device, &error);
}

/* A user event of `context`, which is the one kind of event an application
 * makes rather than gets back from an enqueue. */
cl_event made_user_event(ClIcdRig &rig, cl_context context)
{
    rig.host().cl_handle(host_event, CL_SUCCESS);
    cl_int error = CL_INVALID_VALUE;
    cl_event event = rig.loader().create_user_event(context, &error);
    return error == CL_SUCCESS ? event : nullptr;
}

}  // namespace

TEST(cl_icd_root, a_user_event_is_an_object_of_its_context_and_carries_the_table)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);

  cl_event event = made_user_event(rig, context);
  CHECK(event != nullptr);
  CHECK(rig.loader().dispatch_of(event) == &rig.root().dispatch());
  /* One more object, and it is an event rather than anything else. */
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

  /* A device is an object of this ICD, and still not a context. */
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().create_user_event(reinterpret_cast<cl_context>(device), &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  /* And an error pointer is optional: the refusal must not require one. */
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

  /* The context is not an event, and nothing is sent for it. */
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

  /* Both already complete: one status query each, and the wait is over.
   * Nothing the device runs for this holds its worker. */
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

  /* A negative status is the command's own failure. The API names one error
   * for it, and the second event is never asked about. */
  const cl_int failed = CL_INVALID_VALUE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&failed), sizeof failed);
  const cl_event both[] = {first, second};
  const std::uint32_t before = rig.kmt().commands();
  CHECK_EQ(rig.loader().wait_for_events(2u, both),
           CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST);
  CHECK_EQ(rig.kmt().commands(), before + 1u);

  /* And a host that will not answer the query ends the wait with its own
   * error rather than asking round after round. */
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
  /* A run of events with one impostor in it is refused whole, and the good
   * ones before it are not waited on either. */
  const cl_event mixed[] = {event, reinterpret_cast<cl_event>(context)};
  CHECK_EQ(rig.root().wait_for_events(2u, mixed), CL_INVALID_EVENT);
  /* More than a wait can hold on its stack is told, not truncated. */
  cl_event many[av::clicd::ClIcdRoot::max_waited + 1u] = {};
  for (cl_uint index = 0u; index < av::clicd::ClIcdRoot::max_waited + 1u; ++index)
      many[index] = event;
  CHECK_EQ(rig.root().wait_for_events(av::clicd::ClIcdRoot::max_waited + 1u, many),
           CL_OUT_OF_RESOURCES);
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
  /* The host is told first - it is what will push the completion - and only a
   * host that took it leaves anything registered here. */
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.loader().set_event_callback(event, CL_COMPLETE, &note, &user_data), CL_SUCCESS);
  CHECK(rig.watcher().running());

  /* Not yet: the host still says queued. */
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
  /* The application is done with it on the next line, which the API allows
   * and which is why the registration holds a reference of its own. */
  CHECK_EQ(rig.loader().release_event(event), CL_SUCCESS);
  CHECK_EQ(ClIcdRootProbe::live_of(rig.root(), ClObject::Kind::event), 1u);

  const cl_int complete = CL_COMPLETE;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  /* The host is told to release it when that last reference goes. */
  rig.host().cl_result(CL_SUCCESS);
  CHECK(rig.watcher().step());
  CHECK_EQ(callback.times, 1u);
  CHECK(callback.event == event);
  /* And now it is gone, without the application releasing it twice. */
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
  /* A host that refuses the registration leaves nothing watched here. */
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

  /* The event holds its context, so the transport the watcher asks over is
   * still there when the host says the event completed. */
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

  /* Registering takes a reference, so the application's own release is not
   * the last one. */
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.loader().set_event_callback(event, CL_COMPLETE, &note, nullptr), CL_SUCCESS);
  CHECK_EQ(rig.loader().release_event(event), CL_SUCCESS);
  CHECK_EQ(ClIcdRootProbe::live_of(rig.root(), ClObject::Kind::event), 1u);

  /* And the watcher gives it back through the interface it holds rather than
   * by name - which is the root, and the last reference. */
  av::clicd::ClEventOwner &owner = rig.root();
  ClObject *const held = ClIcdRootProbe::made(rig.root(), event, ClObject::Kind::event);
  CHECK(held != nullptr);
  const std::uint32_t before = rig.kmt().commands();
  rig.host().cl_result(CL_SUCCESS);
  owner.release_watched(*held);
  CHECK_EQ(ClIcdRootProbe::live_of(rig.root(), ClObject::Kind::event), 0u);
  /* The host is told once, when that last reference goes. */
  CHECK_EQ(rig.kmt().commands(), before + 1u);
  CHECK(rig.root().invariants_hold()); }

/* ---- property lists ------------------------------------------------------ */

TEST(cl_icd_root, a_context_property_this_driver_does_not_implement_is_refused)
{ ClIcdRig rig;
  cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);

  /* Nothing is sent: the list is judged before a transport is even made. */
  const std::uint32_t before = rig.kmt().commands();
  const cl_context_properties unknown[] = {0x4242, 1, 0};
  cl_int              error = CL_SUCCESS;
  CHECK(rig.root().create_context(unknown, &device, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PROPERTY);

  /* A property named twice, which the API calls an error rather than a
   * last-one-wins. */
  const cl_context_properties twice[] = {CL_CONTEXT_INTEROP_USER_SYNC, 0, CL_CONTEXT_INTEROP_USER_SYNC, 1, 0};
  CHECK(rig.root().create_context(twice, &device, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PROPERTY);

  /* And a list with no terminator inside the bound, which is the one this
   * must refuse rather than read. */
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

  /* This platform, named explicitly, with the sync flag beside it: both are
   * properties the driver implements, so the context is made. */
  cl_platform_id      ours = nullptr;
  cl_uint             platforms = 0u;
  CHECK_EQ(rig.root().platform_ids(1u, &ours, &platforms), CL_SUCCESS);
  const cl_context_properties named[] = {CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(ours),
                                         CL_CONTEXT_INTEROP_USER_SYNC, 0, 0};
  rig.host().blob_import(0u, 0x77u);
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
                                 CL_QUEUE_PROFILING_ENABLE,
                                 static_cast<std::uint64_t>(CL_QUEUE_SIZE), 16u, 0u};
  rig.host().cl_handle(host_queue, CL_SUCCESS);
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().create_queue(context, device, known, &error) != nullptr);
  CHECK_EQ(error, CL_SUCCESS);

  const std::uint32_t before = rig.kmt().commands();
  const std::uint64_t unknown[] = {0x4242u, 1u, 0u};
  CHECK(rig.root().create_queue(context, device, unknown, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PROPERTY);
  /* OpenCL 3.0 defines no buffer property at all, so any list names
   * something this driver does not implement. */
  const std::uint64_t any[] = {1u, 1u, 0u};
  CHECK(rig.root().create_buffer(context, CL_MEM_READ_WRITE, 64u, nullptr, any, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PROPERTY);
  CHECK_EQ(rig.kmt().commands(), before);
  CHECK(rig.root().invariants_hold()); }

namespace {

/* The property list the last queue request carried to the host: its pairs
 * copied into `pairs`, and how many values that was, or -1 for no list. */
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

}  // namespace

TEST(cl_icd_root, a_profiling_queue_is_asked_of_the_host_with_profiling_and_nothing_else)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  if (context == nullptr) return;
  std::uint64_t pairs[4] = {};

  /* Profiling crosses; out-of-order does not, and the queue runs in order. */
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

  /* The deprecated call's bitfield is the same bit, and none is no list. */
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
  /* A queue made without profiling: the host says so, and nothing is written. */
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

/* ---- non-blocking reads (US3) -------------------------------------------- */

namespace {

/* A context with a queue and a buffer on it, which is what a read needs. */
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

}  // namespace

TEST(cl_icd_root, a_non_blocking_read_returns_before_the_bytes_and_leaves_the_destination_alone)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  cl_command_queue queue = queue_on(rig, context, device);
  cl_mem buffer = buffer_on(rig, context);
  CHECK(queue != nullptr && buffer != nullptr);

  /* The read is queued and this returns. Only a marker crosses - the bytes
   * do not exist yet, and a reply carrying them would carry nothing. */
  unsigned char into[16];
  for (unsigned char &byte : into) byte = 0xA5u;
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  cl_event event = nullptr;
  const std::uint32_t before = rig.kmt().commands();
  CHECK_EQ(rig.root().enqueue_read(queue, buffer, CL_FALSE, 0u, sizeof into, into, &event),
           CL_SUCCESS);
  CHECK(event != nullptr);
  CHECK_EQ(rig.kmt().commands(), before + 1u);
  /* Untouched, which is what the API says it is until the event completes. */
  for (const unsigned char byte : into) CHECK_EQ(byte, 0xA5u);

  /* The wait is where the bytes arrive: the status query, then the fetch. */
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
  /* Nothing was left on the new queue, so there is nothing to drain: the read
   * is what answers the bytes, with no event - the application asked for
   * none. */
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

  /* No event asked for, so the marker is the ICD's own: releasing it now
   * would let the host hand the number to something else while a row still
   * names it. */
  unsigned char into[8] = {};
  rig.host().cl_enqueued(CL_SUCCESS, host_event);
  CHECK_EQ(rig.root().enqueue_read(queue, buffer, CL_FALSE, 0u, sizeof into, into, nullptr),
           CL_SUCCESS);
  CHECK_EQ(ClIcdRootProbe::outstanding_reads(rig.root()), 1u);

  /* clFinish is the only way the application can learn this one is done, and
   * it is where the bytes arrive and the marker goes back. */
  const cl_int complete = CL_COMPLETE;
  rig.host().cl_enqueued(CL_SUCCESS, host_event + 1u);   /* the finish's own marker */
  rig.host().cl_result(CL_SUCCESS);                      /* the queue flushed */
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&complete), sizeof complete);
  rig.host().cl_result(CL_SUCCESS);                      /* that marker released */
  const unsigned char answered[8] = {3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u};
  rig.host().cl_bytes(CL_SUCCESS, answered, sizeof answered);
  rig.host().cl_result(CL_SUCCESS);                      /* the read's marker released */
  CHECK_EQ(rig.loader().finish(queue), CL_SUCCESS);
  for (const unsigned char byte : into) CHECK_EQ(byte, 3u);
  CHECK_EQ(ClIcdRootProbe::outstanding_reads(rig.root()), 0u);
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

  /* The queue goes, and the read's destination is memory the application is
   * about to reuse: nothing is ever fetched into it. The marker the ICD made
   * for it goes back to the host in the same breath. */
  rig.host().cl_result(CL_SUCCESS);   /* the marker */
  rig.host().cl_result(CL_SUCCESS);   /* the queue */
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

  /* Still running: the status is answered and nothing is fetched. */
  const cl_int running = CL_RUNNING;
  rig.host().cl_info(CL_SUCCESS, reinterpret_cast<const char *>(&running), sizeof running);
  cl_int      status = CL_QUEUED;
  std::size_t answered_bytes = 0u;
  CHECK_EQ(rig.loader().event_info(event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status,
                                   &status, &answered_bytes),
           CL_SUCCESS);
  CHECK_EQ(status, CL_RUNNING);
  for (const unsigned char byte : into) CHECK_EQ(byte, 0u);

  /* Complete: the poll is where the application learns it, so it is where
   * the bytes have to be. */
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

  /* A callback is what an application uses instead of waiting, so a callback
   * that could not see the bytes would be a callback about nothing:
   * ClIcdRoot::deliver_watched is what the watcher calls before it runs one. */
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

/* ---- the root's own invariant ------------------------------------------- */

namespace av {

struct ObjectPoolProbe {
    template <typename T>
    static void miscount(ObjectPool<T> &pool) { pool.live_ += 1u; }
};

}  // namespace av

namespace av::icd {

struct HandleTreeProbe {
    template <typename Node>
    static ObjectPool<Node> &pool(HandleTree<Node> &tree) { return tree.pool_; }
};

}  // namespace av::icd

namespace av::clicd {

/* One corruption per thing the root owns. Each is defined here rather than
 * shared, because a probe is declared a friend by name and nothing outside
 * the tests uses one. */
struct ClPlatformProbe {
    static void an_empty_version(ClPlatform &platform) { platform.version_[0] = '\0'; }
};

struct ClExtensionPolicyProbe {
    static void count_past_the_table(ClExtensionPolicy &policy)
    { policy.count_ = ClExtensionPolicy::max_rows + 1u; }
};

struct ClObjectTableProbe {
    static void miscount(ClObjectTable &table) { ObjectPoolProbe::miscount(icd::HandleTreeProbe::pool(table.tree_)); }
};

struct ClDeviceReportProbe {
    static void a_version_that_runs_off_its_room(ClDeviceReport &device)
    { for (char &byte : device.version_) byte = 'x'; }
};

struct ClPendingReadsProbe {
    static void miscount(ClPendingReads &reads) { reads.held_ += 1u; }
};

struct ClEventWatcherProbe {
    static void miscount(ClEventWatcher &watcher) { watcher.held_ += 1u; }
};

}  // namespace av::clicd

namespace av::icd {

struct IcdConnectionProbe {
    static void set_users(IcdConnection &connection, std::uint32_t users)
    { connection.users_[0] = users; }
};

}  // namespace av::icd

/* The root is sound only if everything it owns is. Each part is broken in
 * turn, on its own rig, because the first to fail hides the rest. */
TEST(cl_icd_root, the_invariant_is_every_part_of_it)
{ { ClIcdRig rig;
    CHECK(rig.root().invariants_hold());
    av::clicd::ClPlatformProbe::an_empty_version(av::clicd::ClIcdRootProbe::platform(rig.root()));
    CHECK(!rig.root().invariants_hold()); }
  { ClIcdRig rig;
    av::clicd::ClExtensionPolicyProbe::count_past_the_table(
        av::clicd::ClIcdRootProbe::extensions(rig.root()));
    CHECK(!rig.root().invariants_hold()); }
  { ClIcdRig rig;
    av::clicd::ClObjectTableProbe::miscount(av::clicd::ClIcdRootProbe::objects_of(rig.root()));
    CHECK(!rig.root().invariants_hold()); }
  { ClIcdRig rig;
    /* A link with users but no adapter behind it. */
    av::icd::IcdConnectionProbe::set_users(av::clicd::ClIcdRootProbe::connection(rig.root()), 1u);
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

/* ---- an object with no context behind it -------------------------------- */

/* Every entry that reaches the host does it through its object's context, and
 * every one of them guards against there not being one. Nothing the ICD does
 * makes such an object - each is made on a context, and the device's is the
 * one the ICD opened for itself - so the state is built here directly.
 *
 * What matters is that each entry refuses in the API's own words and sends
 * nothing: a guard that dereferenced instead would be a null read inside the
 * loader's own call. */
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

  /* The maps and the unmap. */
  cl_int error = CL_SUCCESS;
  CHECK(rig.root().map_buffer(as_queue, as_buffer, CL_TRUE, CL_MAP_READ, 0u, sizeof bytes,
                              nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().unmap(as_queue, as_buffer, bytes, nullptr), CL_INVALID_CONTEXT);

  /* The shared-memory entries. */
  CHECK(rig.root().svm_alloc(as_context, CL_MEM_READ_WRITE, sizeof bytes, 0u) == nullptr);
  rig.root().svm_free(as_context, bytes);
  CHECK_EQ(rig.root().svm_map(as_queue, CL_TRUE, CL_MAP_READ, bytes, sizeof bytes, nullptr),
           CL_INVALID_CONTEXT);
  CHECK_EQ(rig.root().svm_unmap(as_queue, bytes, nullptr), CL_INVALID_CONTEXT);
  CHECK_EQ(rig.root().set_kernel_svm_argument(as_kernel, 0u, bytes), CL_INVALID_CONTEXT);

  /* The events. */
  CHECK_EQ(rig.root().wait_for_events(1u, &as_event), CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().event_info(as_event, CL_EVENT_COMMAND_EXECUTION_STATUS, 0u, nullptr,
                                 nullptr),
           CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().watch_event(as_event, CL_COMPLETE, nullptr, nullptr), CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.root().set_user_event_status(as_event, CL_COMPLETE), CL_OUT_OF_RESOURCES);

  /* And the device queries the host would own: the ones forwarded whole, and
   * the extensions, which are the host's list intersected with the policy and
   * so need the host as much as a forwarded query does. */
  char        name[8] = {};
  std::size_t answered = 0u;
  CHECK_EQ(rig.root().device_info(as_device, CL_DEVICE_NAME, sizeof name, name, &answered),
           CL_INVALID_DEVICE);
  CHECK_EQ(rig.root().device_info(as_device, CL_DEVICE_EXTENSIONS, sizeof name, name, &answered),
           CL_INVALID_DEVICE);

  /* Not one of them reached the driver. */
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

/* The watcher hands an event back to the root when its callback has run, and
 * an event with no context is one there is nothing to deliver over. */
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

/* Releasing an object whose context is gone: there is nothing to tell the
 * host, so the guest's own record goes and nothing is sent. The application
 * is still entitled to release what it holds. */
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

/* A wait naming events of two different contexts. This ICD has one transport
 * per context and no way to wait on two at once, so it refuses rather than
 * waiting on one of them and calling that the answer. */
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

  /* An event of no context at all is an event of a different one. */
  ClObject *const stranger =
      ClIcdRootProbe::without_a_context(rig.root(), ClObject::Kind::event, 0x6901u);
  CHECK(stranger != nullptr);
  if (stranger == nullptr) return;
  const cl_event both[] = {mine, reinterpret_cast<cl_event>(stranger)};
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().wait_for_events(2u, both), CL_INVALID_CONTEXT);
  CHECK_EQ(rig.kmt().commands(), sent);
  CHECK(rig.root().invariants_hold()); }

/* A status query with room for less than a status: the host answers it, and
 * there is nothing for the ICD to read a completion out of, so no deferred
 * read is delivered. */
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

  /* The host answers what the room allows, which is less than a status. */
  const char byte_of_a_status = 0;
  rig.host().cl_info(CL_SUCCESS, &byte_of_a_status, 1u);
  unsigned char cramped[1] = {0u};
  const std::uint32_t sent = rig.kmt().commands();
  CHECK_EQ(rig.root().event_info(event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof cramped,
                                 cramped, nullptr),
           CL_SUCCESS);
  /* One command, the query itself: no fetch followed it. */
  CHECK_EQ(rig.kmt().commands(), sent + 1u);
  CHECK(rig.root().invariants_hold()); }

/* A callback the watcher cannot register after the host already has.
 *
 * The host is told first, then the guest takes a reference and records the
 * registration. If that last step fails, the reference has to go back - the
 * application holds one of its own, and leaking the ICD's would keep the
 * event alive for the life of the context. The host's own registration is
 * left: OpenCL has no way to unregister one, which is exactly why every
 * callback carries an epoch. */
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
  /* The host registers it, and the watcher has no thread to run it on. */
  rig.host().cl_result(CL_SUCCESS);
  rig.watcher().refuse_start();
  CHECK_EQ(rig.root().watch_event(event, CL_COMPLETE, &nothing_ran, nullptr),
           CL_OUT_OF_RESOURCES);
  /* The reference the registration took is back, so the application's is
   * the only one and releasing it still destroys the event. */
  CHECK_EQ(ClIcdRootProbe::live_of(rig.root(), ClObject::Kind::event), live);
  CHECK(rig.root().invariants_hold());
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.root().release(event, ClObject::Kind::event), CL_SUCCESS);
  CHECK_EQ(ClIcdRootProbe::live_of(rig.root(), ClObject::Kind::event), live - 1u);
  CHECK(rig.root().invariants_hold()); }

/* A callback registration with no callback is the caller's own mistake, and
 * it is judged after the root is found: with a root there is an event to
 * name, so the missing argument is what is wrong. */
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

/* The deprecated 1.2 queue call, with a root behind it: an application built
 * against the older header reaches this slot and nothing else, so it has to
 * make the same queue the properties call does. */
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
  /* It is a queue of this ICD, carrying its dispatch table like any other. */
  CHECK(rig.loader().dispatch_of(queue) == &rig.root().dispatch());
  CHECK(rig.root().invariants_hold()); }

/* ---- clGetContextInfo, clGetMemObjectInfo, clGetImageInfo ---------------- */

TEST(cl_icd_root, a_contexts_info_is_this_icds_own_and_asks_the_host_nothing)
{ ClIcdRig rig;
  const cl_device_id device = opened_device(rig);
  CHECK(device != nullptr);
  if (device == nullptr) return;
  const cl_icd_dispatch &table = rig.root().dispatch();
  const cl_context_properties asked[] = {CL_CONTEXT_PLATFORM,
                                         reinterpret_cast<cl_context_properties>(rig.loader().platform(0u)),
                                         CL_CONTEXT_INTEROP_USER_SYNC, CL_TRUE, 0};
  rig.host().blob_import(0u, 0x77u);
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
  /* The list as the application wrote it, terminator and all. */
  cl_context_properties kept[8] = {};
  CHECK_EQ(table.clGetContextInfo(context, CL_CONTEXT_PROPERTIES, 0u, nullptr, &size), CL_SUCCESS);
  CHECK_EQ(size, sizeof asked);
  CHECK_EQ(table.clGetContextInfo(context, CL_CONTEXT_PROPERTIES, sizeof kept, kept, nullptr), CL_SUCCESS);
  for (std::size_t at = 0u; at < sizeof asked / sizeof asked[0]; ++at) CHECK_EQ(kept[at], asked[at]);
  /* Too little room writes nothing; an unknown name and a non-context are refused. */
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
  /* The size is the host's, asked once: it is fixed when the buffer is
   * made, and an application may ask it of every tile on every operation. A
   * size query and room too small are answered from what was kept. */
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
  /* The type, the offset and whether it uses shared memory are fixed too. */
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
  /* The map count changes with every map: asked every time. */
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

/* A fixed answer the host refuses is the host's error, and nothing is kept of
 * it: the next query asks again. */
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

/* ---- what the application was refused, in the log ------------------------ */

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
  /* The empty line is skipped. */
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
  /* A context is no program. */
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
  /* A success writes nothing, and still reaches the application. */
  error = CL_INVALID_VALUE;
  CHECK_EQ(table.clGetContextInfo(context, CL_CONTEXT_NUM_DEVICES, sizeof count, &count, nullptr), CL_SUCCESS);
  CHECK_EQ(rig.log().lines() - before, 2u); }

/* The state dump reads under the root's lock, as every root's does: held
 * elsewhere - the process hung with it - the dump says busy and nothing else,
 * rather than read what an application thread may be changing. */
TEST(cl_icd_root, a_state_dump_reads_under_the_lock_or_says_busy)
{ ClIcdRig rig;
  char            free_bytes[2048] = {};
  av::StateWriter free_writer(free_bytes, sizeof free_bytes);
  rig.root().report(free_writer);
  CHECK(std::strstr(free_writer.dump(), "clicd/0 busy=false device=false") != nullptr);
  CHECK(std::strstr(free_writer.dump(), "reads_outstanding=0") != nullptr);
  CHECK(!rig.lock().held());
  rig.lock().held_elsewhere(true);
  char            busy_bytes[256] = {};
  av::StateWriter busy_writer(busy_bytes, sizeof busy_bytes);
  rig.root().report(busy_writer);
  CHECK(std::strstr(busy_writer.dump(), "clicd/0 busy=true") != nullptr);
  CHECK(std::strstr(busy_writer.dump(), "reads_outstanding") == nullptr);
  rig.lock().held_elsewhere(false); }
