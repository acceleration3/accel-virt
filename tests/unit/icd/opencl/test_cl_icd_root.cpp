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
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "icd/opencl/cl_property_list.hpp"

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

  /* Decided here: the LUID is the rvkgpu adapter's, not the host GPU's - the
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

  /* Releasing the context takes all of them, however many the application
   * still holds. */
  CHECK_EQ(rig.root().release_context(context), CL_SUCCESS);
  CHECK_EQ(rig.root().release(queue, ClObject::Kind::command_queue), CL_INVALID_COMMAND_QUEUE);
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
    static std::uint32_t live_of(ClIcdRoot &root, ClObject::Kind kind)
    { return root.objects_.live_of(kind); }
    static ClObject *made(ClIcdRoot &root, const void *pointer, ClObject::Kind kind)
    { return root.objects_.made(pointer, kind); }
};

}  // namespace av::clicd

using av::clicd::ClIcdRootProbe;

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
  CHECK(!rig.watcher().step());
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
  CHECK(!rig.watcher().step());
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

TEST(cl_icd_root, releasing_a_context_forgets_the_callbacks_registered_on_it)
{ ClIcdRig rig;
  cl_device_id device = nullptr;
  cl_context context = opened_context(rig, device);
  CHECK(context != nullptr);
  cl_event event = made_user_event(rig, context);
  CHECK(event != nullptr);

  callback = Callback();
  rig.host().cl_result(CL_SUCCESS);
  CHECK_EQ(rig.loader().set_event_callback(event, CL_COMPLETE, &note, nullptr), CL_SUCCESS);

  /* The context goes, and with it the transport the watcher would ask over.
   * Nothing is run and nothing is asked. */
  CHECK_EQ(rig.loader().release_context(context), CL_SUCCESS);
  const std::uint32_t before = rig.kmt().commands();
  CHECK(!rig.watcher().step());
  CHECK_EQ(callback.times, 0u);
  CHECK_EQ(rig.kmt().commands(), before);
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
  const std::uint64_t unknown[] = {0x4242u, 1u, 0u};
  cl_int              error = CL_SUCCESS;
  CHECK(rig.root().create_context(unknown, &device, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PROPERTY);

  /* A property named twice, which the API calls an error rather than a
   * last-one-wins. */
  const std::uint64_t twice[] = {static_cast<std::uint64_t>(CL_CONTEXT_INTEROP_USER_SYNC), 0u,
                                 static_cast<std::uint64_t>(CL_CONTEXT_INTEROP_USER_SYNC), 1u, 0u};
  CHECK(rig.root().create_context(twice, &device, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PROPERTY);

  /* And a list with no terminator inside the bound, which is the one this
   * must refuse rather than read. */
  std::uint64_t unterminated[ClPropertyList::max_pairs * 2u + 3u] = {};
  for (std::uint32_t pair = 0u; pair <= ClPropertyList::max_pairs; ++pair)
      unterminated[pair * 2u] = static_cast<std::uint64_t>(CL_CONTEXT_INTEROP_USER_SYNC) + pair;
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
  const std::uint64_t other[] = {
      static_cast<std::uint64_t>(CL_CONTEXT_PLATFORM),
      static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&elsewhere)), 0u};
  CHECK(rig.root().create_context(other, &device, 1u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PLATFORM);

  /* This platform, named explicitly, with the sync flag beside it: both are
   * properties the driver implements, so the context is made. */
  cl_platform_id      ours = nullptr;
  cl_uint             platforms = 0u;
  CHECK_EQ(rig.root().platform_ids(1u, &ours, &platforms), CL_SUCCESS);
  const std::uint64_t named[] = {
      static_cast<std::uint64_t>(CL_CONTEXT_PLATFORM),
      static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(ours)),
      static_cast<std::uint64_t>(CL_CONTEXT_INTEROP_USER_SYNC), 0u, 0u};
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
