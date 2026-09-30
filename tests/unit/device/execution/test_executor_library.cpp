/* The table an executor library hands over: accepted only whole and of this
 * ABI, and what it offers limited to the sub-APIs the protocol knows. */
#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"
#include "device/execution/executor_library.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/context/sub_api.hpp"

#include <cstdint>

using av::ExecutorLibrary;
using av::MockExecutorOps;
using av::Status;
using av::SubApi;

TEST(executor_library_accept, a_whole_table_of_this_abi_is_available)
{ MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN | RVK_EXECUTOR_SUB_API_OPENCL);
  ExecutorLibrary library;
  CHECK(!library.available());
  CHECK_EQ(library.sub_apis(), 0u);
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  CHECK(library.available());
  CHECK_EQ(library.sub_apis(), RVK_EXECUTOR_SUB_API_VULKAN | RVK_EXECUTOR_SUB_API_OPENCL);
  CHECK_STATUS(library.accept(&ops.table()), Status::invalid_state); }

TEST(executor_library_accept, no_table_leaves_the_device_without_an_executor)
{ ExecutorLibrary library;
  CHECK_STATUS(library.accept(nullptr), Status::ok);
  CHECK(!library.available());
  CHECK_EQ(library.sub_apis(), 0u);
  /* A context's worker may still ask for one; it is told there is none. */
  const std::uint8_t uuid[16] = {};
  CHECK(library.create(SubApi::vulkan, uuid) == nullptr); }

TEST(executor_library_accept, another_abi_version_or_a_missing_operation_is_refused)
{ for (int broken = 0; broken < 5; broken++) {
      MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
      RvkExecutorOps &table = ops.editable();
      if (broken == 0) table.abi_version = RVK_EXECUTOR_ABI_VERSION + 1u;
      if (broken == 1) table.sub_apis = nullptr;
      if (broken == 2) table.create = nullptr;
      if (broken == 3) table.destroy = nullptr;
      if (broken == 4) table.execute = nullptr;
      ExecutorLibrary library;
      CHECK_STATUS(library.accept(&table), Status::invalid_parameter);
      CHECK(!library.available());
  } }

TEST(executor_library_sub_apis, bits_the_protocol_does_not_know_are_not_offered)
{ MockExecutorOps ops(0xFFFFFFFFu);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  CHECK_EQ(library.sub_apis(), 15u); }

/* A version 1 library keeps working and imports nothing; a version 2 one must
 * carry all three memory operations or it is not version 2 (feature 042). */
TEST(executor_library_accept, version_one_is_still_a_library_that_imports_nothing)
{ MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  RvkExecutorOps &table = ops.editable();
  table.abi_version = RVK_EXECUTOR_ABI_VERSION_MINIMUM;
  table.import_kinds = nullptr;
  table.import_blob = nullptr;
  table.release_blob = nullptr;
  table.imported_handle = nullptr;
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&table), Status::ok);
  CHECK(library.available());
  CHECK(!library.imports());
  CHECK(!library.names_imports());
  CHECK_EQ(library.import_kinds(SubApi::vulkan, nullptr), 0u); }

TEST(executor_library_accept, version_two_without_its_operations_is_refused)
{ for (int missing = 0; missing < 3; missing++) {
      MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
      RvkExecutorOps &table = ops.editable();
      if (missing == 0) table.import_kinds = nullptr;
      if (missing == 1) table.import_blob = nullptr;
      if (missing == 2) table.release_blob = nullptr;
      ExecutorLibrary library;
      CHECK_STATUS(library.accept(&table), Status::invalid_parameter);
      CHECK(!library.available());
  } }

TEST(executor_library_accept, a_version_older_than_one_or_newer_than_this_is_refused)
{ for (int wrong = 0; wrong < 2; wrong++) {
      MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
      RvkExecutorOps &table = ops.editable();
      table.abi_version = wrong == 0 ? RVK_EXECUTOR_ABI_VERSION_MINIMUM - 1u
                                     : RVK_EXECUTOR_ABI_VERSION + 1u;
      ExecutorLibrary library;
      CHECK_STATUS(library.accept(&table), Status::invalid_parameter);
  } }

/* What a version 2 library says it can import is what the capset may claim. */
TEST(executor_library_import_kinds, come_from_the_library_for_one_api_and_one_gpu)
{ const std::uint8_t uuid[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ops.set_import_kinds(RVK_BLOB_HOST_POINTER);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  CHECK(library.imports());
  CHECK_EQ(library.import_kinds(SubApi::vulkan, uuid), std::uint32_t{RVK_BLOB_HOST_POINTER}); }

/* A device with no executor library at all imports nothing and offers no kind,
 * rather than dereferencing a table it was never given. */
TEST(executor_library_import_kinds, a_device_with_no_library_imports_nothing)
{ ExecutorLibrary library;
  CHECK(!library.imports());
  CHECK_EQ(library.import_kinds(SubApi::vulkan, nullptr), 0u); }

/* Version 2 still loads after version 3 exists: it imports, and names nothing
 * it imported (feature 045). */
TEST(executor_library_accept, version_two_is_a_library_that_imports_and_names_nothing)
{ MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  RvkExecutorOps &table = ops.editable();
  table.abi_version = RVK_EXECUTOR_ABI_VERSION_IMPORTS;
  table.imported_handle = nullptr;
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&table), Status::ok);
  CHECK(library.imports());
  CHECK(!library.names_imports()); }

TEST(executor_library_accept, version_three_without_imported_handle_is_refused)
{ CHECK_EQ(RVK_EXECUTOR_ABI_VERSION, 13u);
  CHECK_EQ(RVK_EXECUTOR_ABI_VERSION_HANDLES, 3u);
  CHECK_EQ(RVK_EXECUTOR_ABI_VERSION_OPTIONS, 4u);
  CHECK_EQ(RVK_EXECUTOR_ABI_VERSION_EVENTS, 5u);
  CHECK_EQ(RVK_EXECUTOR_ABI_VERSION_SHARING, 6u);
  CHECK_EQ(RVK_EXECUTOR_ABI_VERSION_IMAGES, 7u);
  CHECK_EQ(RVK_EXECUTOR_ABI_VERSION_BATCHES, 8u);
  CHECK_EQ(RVK_EXECUTOR_ABI_VERSION_EXPORTS, 9u);
  CHECK_EQ(RVK_EXECUTOR_ABI_VERSION_POLL, 10u);
  CHECK_EQ(RVK_EXECUTOR_ABI_VERSION_PROGRESS, 11u);
  CHECK_EQ(RVK_EXECUTOR_ABI_VERSION_SHARED_RELEASE, 12u);
  CHECK_EQ(RVK_EXECUTOR_ABI_VERSION_SNAPSHOTS, 13u);
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ops.editable().abi_version = RVK_EXECUTOR_ABI_VERSION_HANDLES;
  ops.editable().imported_handle = nullptr;
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::invalid_parameter);
  CHECK(!library.available());
  CHECK(!library.names_imports()); }

TEST(executor_library_import_blob, forwards_the_import_its_release_and_the_handle_it_made)
{ const std::uint8_t uuid[16] = {};
  std::uint8_t        page[4096] = {};
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  CHECK(library.names_imports());
  RvkExecutor *const executor = library.create(SubApi::vulkan, uuid);
  RvkBlobHandle handle = {};
  handle.kind = RVK_BLOB_HOST_POINTER;
  handle.reserved = 4u;
  handle.pointer = page;
  handle.fd = -1;
  handle.size = sizeof page;
  CHECK_EQ(library.imported_handle(executor, 0x21u), 0u);
  CHECK_EQ(library.import_blob(executor, 0x21u, handle), std::uint32_t{RVK_EXECUTED});
  CHECK_EQ(library.imported_handle(executor, 0x21u), 0x21u | MockExecutorOps::handle_bit);
  RvkBlobHandle seen = {};
  CHECK(ops.import_of(0x21u, seen));
  CHECK(seen.pointer == page);
  CHECK_EQ(seen.reserved, 4u);
  library.release_blob(executor, 0x21u);
  CHECK_EQ(library.imported_handle(executor, 0x21u), 0u);
  CHECK_EQ(ops.released(), 1u);
  library.destroy(executor);
  CHECK_EQ(ops.misuses(), 0u); }

/* Version 4 (feature 050): a table claiming it carries create_with_options or
 * is refused; an older one makes default contexts only, and a create asking
 * for anything else is refused without a call. */
TEST(executor_library_accept, version_four_needs_create_with_options)
{ MockExecutorOps ops(RVK_EXECUTOR_SUB_API_OPENGL);
  RvkExecutorOps &table = ops.editable();
  CHECK_EQ(table.abi_version, RVK_EXECUTOR_ABI_VERSION);
  /* A table that says it carries options has to, whatever else it says. */
  table.abi_version = RVK_EXECUTOR_ABI_VERSION_OPTIONS;
  table.create_with_options = nullptr;
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&table), Status::invalid_parameter);
  CHECK(!library.available());
  CHECK(!library.names_options()); }

TEST(executor_library_create, a_version_four_library_is_handed_the_options_and_the_share)
{ const std::uint8_t uuid[16] = {9};
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_OPENGL);
  ExecutorLibrary library;
  CHECK(!library.names_options());
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  CHECK(library.names_options());
  RvkExecutor *const first = library.create(SubApi::opengl, uuid);
  CHECK(first != nullptr);
  CHECK_EQ(ops.option_creates(), 1u);
  CHECK(ops.last_share() == nullptr);
  CHECK_EQ(ops.last_options().major, 0u);
  const RvkContextOptions options = {4u, 6u, RVK_CONTEXT_PROFILE_CORE, RVK_CONTEXT_FLAG_DEBUG, 0u};
  RvkExecutor *const second = library.create(SubApi::opengl, uuid, &options, first);
  CHECK(second != nullptr);
  CHECK(second != first);
  CHECK_EQ(ops.option_creates(), 2u);
  CHECK(ops.last_share() == first);
  CHECK(ops.last_share_live());
  CHECK_EQ(ops.last_options().major, 4u);
  CHECK_EQ(ops.last_options().minor, 6u);
  CHECK_EQ(ops.last_options().profile, RVK_CONTEXT_PROFILE_CORE);
  CHECK_EQ(ops.last_options().flags, RVK_CONTEXT_FLAG_DEBUG);
  CHECK_EQ(ops.last_sub_api(), RVK_EXECUTOR_SUB_API_OPENGL);
  library.destroy(second);
  library.destroy(first);
  CHECK_EQ(ops.misuses(), 0u); }

TEST(executor_library_create, a_version_three_library_makes_only_default_contexts)
{ const std::uint8_t uuid[16] = {};
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_OPENGL);
  RvkExecutorOps &table = ops.editable();
  table.abi_version = RVK_EXECUTOR_ABI_VERSION_HANDLES;
  table.create_with_options = nullptr;
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&table), Status::ok);
  CHECK(!library.names_options());
  RvkExecutor *const plain = library.create(SubApi::opengl, uuid);
  CHECK(plain != nullptr);
  CHECK_EQ(ops.created(), 1u);
  const RvkContextOptions options = {3u, 3u, RVK_CONTEXT_PROFILE_CORE, 0u, 0u};
  CHECK(library.create(SubApi::opengl, uuid, &options, nullptr) == nullptr);
  CHECK(library.create(SubApi::opengl, uuid, nullptr, plain) == nullptr);
  CHECK_EQ(ops.created(), 1u);
  CHECK_EQ(ops.option_creates(), 0u);
  library.destroy(plain);
  CHECK_EQ(ops.misuses(), 0u); }

/* Version 5 (feature 052): the first entry of this table that runs the other
 * way. A host API that finishes work on a thread of its own - OpenCL's event
 * callbacks - has no other way to say so, and a library that claims the
 * version without carrying it would be found out on that thread, with a guest
 * waiting for a wake that never comes. */
TEST(executor_library_accept, version_five_needs_set_signal)
{ MockExecutorOps ops(RVK_EXECUTOR_SUB_API_OPENCL);
  RvkExecutorOps &table = ops.editable();
  CHECK_EQ(table.abi_version, RVK_EXECUTOR_ABI_VERSION);
  table.set_signal = nullptr;
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&table), Status::invalid_parameter);
  CHECK(!library.available());
  CHECK_EQ(ops.misuses(), 0u); }

namespace {

/* A GPU to make an executor on; the mock reads all sixteen bytes. */
constexpr std::uint8_t some_gpu[16] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u,
                                       9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u};

}  // namespace

TEST(executor_library_signal, a_version_four_library_is_given_nothing_and_signals_nothing)
{ MockExecutorOps ops(RVK_EXECUTOR_SUB_API_OPENCL);
  ops.editable().abi_version = RVK_EXECUTOR_ABI_VERSION_OPTIONS;
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  CHECK(!library.names_events());
  /* A guest on such a device waits for its events at a synchronising call
   * instead, so handing the library a callback it cannot carry is not done at
   * all rather than done and ignored. */
  RvkExecutor *const executor = library.create(SubApi::opencl, some_gpu, nullptr, nullptr);
  library.set_signal(executor, nullptr, nullptr);
  CHECK_EQ(ops.signals_set(), 0u);
  CHECK(!ops.signal_bound());
  library.destroy(executor);
  CHECK_EQ(ops.misuses(), 0u); }

TEST(executor_library_signal, a_version_five_library_is_given_one_and_calls_it)
{ MockExecutorOps ops(RVK_EXECUTOR_SUB_API_OPENCL);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  CHECK(library.names_events());
  RvkExecutor *const executor = library.create(SubApi::opencl, some_gpu, nullptr, nullptr);
  CHECK(executor != nullptr);

  static std::uint32_t woken = 0u;
  woken = 0u;
  library.set_signal(executor, [](void *) { ++woken; }, nullptr);
  CHECK_EQ(ops.signals_set(), 1u);
  CHECK(ops.signal_bound());
  /* Fired as the host API's own thread would. */
  CHECK(ops.fire_signal());
  CHECK_EQ(woken, 1u);
  library.destroy(executor);
  CHECK_EQ(ops.misuses(), 0u); }

TEST(executor_library_signal, an_executor_that_is_not_there_is_given_nothing)
{ MockExecutorOps ops(RVK_EXECUTOR_SUB_API_OPENCL);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  library.set_signal(nullptr, [](void *) {}, nullptr);
  CHECK_EQ(ops.signals_set(), 0u);
  CHECK_EQ(ops.misuses(), 0u); }

/* A library that accepted nothing names no version at all, so it names no
 * events either: asking a table that is not there what it carries would be
 * reading through a null pointer. */
TEST(executor_library_signal, a_library_that_accepted_nothing_names_no_events)
{ ExecutorLibrary library;
  CHECK(!library.names_events()); }

TEST(executor_library_accept, version_six_missing_any_of_the_sharing_three_is_refused)
{ const char *const names[] = {"export", "import", "release"};
  for (std::uint32_t missing = 0u; missing < 3u; missing++) {
      MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
      if (missing == 0u) ops.editable().export_object = nullptr;
      if (missing == 1u) ops.editable().import_object = nullptr;
      if (missing == 2u) ops.editable().release_object = nullptr;
      ExecutorLibrary library;
      CHECK_STATUS(library.accept(&ops.table()), Status::invalid_parameter);
      CHECK(!library.available());
      CHECK(!library.shares_objects());
      static_cast<void>(names[missing]);
  } }

TEST(executor_library_shares_objects, is_true_only_from_the_version_that_has_the_three_calls)
{ MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ops.set_abi_version(RVK_EXECUTOR_ABI_VERSION_EVENTS);
  ops.editable().export_object = nullptr;
  ops.editable().import_object = nullptr;
  ops.editable().release_object = nullptr;
  ExecutorLibrary older;
  CHECK_STATUS(older.accept(&ops.table()), Status::ok);
  CHECK(older.available());
  CHECK(!older.shares_objects());

  MockExecutorOps newest(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&newest.table()), Status::ok);
  CHECK(library.shares_objects());

  ExecutorLibrary none;
  CHECK_STATUS(none.accept(nullptr), Status::ok);
  CHECK(!none.shares_objects()); }

TEST(executor_library_export_object, refuses_a_library_that_answers_success_with_no_descriptor)
{ av::MockHost    host;
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  const std::uint8_t uuid[16] = {};
  RvkExecutor *const executor = library.create(av::SubApi::vulkan, uuid);
  CHECK(executor != nullptr);
  std::uint32_t handle_kind = 9u;
  int           fd = 9;
  /* No host to take a descriptor from: the mock answers RVK_UNKNOWN. */
  CHECK(!library.export_object(executor, RVK_SHARED_MEMORY, 0x40u, handle_kind, fd));
  CHECK_EQ(handle_kind, 0u);
  CHECK_EQ(fd, -1);
  ops.descriptors_from(host);
  CHECK(library.export_object(executor, RVK_SHARED_MEMORY, 0x40u, handle_kind, fd));
  CHECK_EQ(handle_kind, RVK_SHARED_HANDLE_DMA_BUF);
  CHECK(fd >= 0);
  host.close_dmabuf(fd);
  /* A handle the executor never made, and a kind that is not one. */
  CHECK(!library.export_object(executor, RVK_SHARED_MEMORY, 0u, handle_kind, fd));
  CHECK(!library.export_object(executor, 0u, 0x40u, handle_kind, fd));
  library.destroy(executor); }

TEST(executor_library_import_object, gives_nothing_back_for_a_library_that_names_nothing)
{ av::MockHost    host;
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ops.descriptors_from(host);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  const std::uint8_t uuid[16] = {};
  RvkExecutor *const executor = library.create(av::SubApi::vulkan, uuid);
  const int          fd = host.open_descriptor();
  std::uint64_t      wire_handle = 9u;
  ops.withhold_object_handle_at(1u);
  CHECK(!library.import_object(executor, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, fd, 4096u,
                               wire_handle));
  CHECK_EQ(wire_handle, 0u);
  CHECK_EQ(ops.live_objects(), 0u);
  CHECK(library.import_object(executor, RVK_SHARED_MEMORY, RVK_SHARED_HANDLE_DMA_BUF, fd, 4096u,
                              wire_handle));
  CHECK(wire_handle != 0u);
  CHECK_EQ(ops.live_objects(), 1u);
  /* The descriptor is still the caller's, which is what makes one object
   * openable by every context. */
  CHECK(host.descriptor_open(fd));
  library.release_object(executor, RVK_SHARED_MEMORY, wire_handle);
  CHECK_EQ(ops.released_objects(), 1u);
  CHECK_EQ(ops.live_objects(), 0u);
  host.close_dmabuf(fd);
  library.destroy(executor); }

/* ABI 8 onwards: the device tells the executor a batch is over, so one lock
 * can be held across it. An older table, or one that leaves the call out, is
 * told nothing, and a device with no library is not a crash. */
namespace {

std::uint32_t batches_ended = 0u;
RvkExecutor  *batch_ended_for = nullptr;

void count_end_batch(void *, RvkExecutor *executor)
{
    ++batches_ended;
    batch_ended_for = executor;
}

}  // namespace

TEST(executor_library_end_batch, reaches_a_library_of_the_batches_version_only)
{ MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ops.editable().end_batch = count_end_batch;
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RvkExecutor *const executor = library.create(SubApi::vulkan, some_gpu, nullptr, nullptr);
  batches_ended = 0u;
  library.end_batch(executor);
  CHECK_EQ(batches_ended, 1u);
  CHECK(batch_ended_for == executor);

  ops.set_abi_version(RVK_EXECUTOR_ABI_VERSION_BATCHES - 1u);
  library.end_batch(executor);
  CHECK_EQ(batches_ended, 1u);
  ops.set_abi_version(RVK_EXECUTOR_ABI_VERSION_BATCHES);
  library.end_batch(executor);
  CHECK_EQ(batches_ended, 2u);
  ops.editable().end_batch = nullptr;
  library.end_batch(executor);
  CHECK_EQ(batches_ended, 2u);
  library.destroy(executor);

  ExecutorLibrary none;
  none.end_batch(nullptr);
  CHECK_EQ(batches_ended, 2u); }

/* ABI 11: the executor's GPU marks. Anything older, or a table without the
 * call, answers 0 and 0 - an executor whose work is done when it returns -
 * whatever the caller held before. */
namespace {

void report_progress(void *, RvkExecutor *, std::uint64_t *marked, std::uint64_t *reached)
{
    *marked = 7u;
    *reached = 5u;
}

}  // namespace

TEST(executor_library_gpu_progress, is_the_library_s_from_the_progress_version_and_zero_before)
{ MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ops.editable().gpu_progress = report_progress;
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RvkExecutor *const executor = library.create(SubApi::vulkan, some_gpu, nullptr, nullptr);
  std::uint64_t marked = 1u, reached = 1u;
  library.gpu_progress(executor, marked, reached);
  CHECK(marked == 7u);
  CHECK(reached == 5u);

  ops.set_abi_version(RVK_EXECUTOR_ABI_VERSION_PROGRESS - 1u);
  library.gpu_progress(executor, marked, reached);
  CHECK(marked == 0u);
  CHECK(reached == 0u);
  ops.set_abi_version(RVK_EXECUTOR_ABI_VERSION_PROGRESS);
  ops.editable().gpu_progress = nullptr;
  marked = reached = 3u;
  library.gpu_progress(executor, marked, reached);
  CHECK(marked == 0u);
  CHECK(reached == 0u);
  library.destroy(executor);

  ExecutorLibrary none;
  marked = reached = 3u;
  none.gpu_progress(nullptr, marked, reached);
  CHECK(marked == 0u);
  CHECK(reached == 0u); }

/* ABI 9: a published image handed over as a dma-buf. Only a library of that
 * version with the call is asked, and only its RVK_EXECUTED is an export. */
namespace {

std::uint64_t exported_key = 0u;

std::uint32_t export_known_key(void *, const std::uint8_t *, std::uint64_t key, RvkExportedImage *image)
{
    exported_key = key;
    if (key != 0x51u) return RVK_UNKNOWN;
    image->fd = 12;
    image->width = 640u;
    image->height = 480u;
    image->stride = 2560u;
    return RVK_EXECUTED;
}

}  // namespace

TEST(executor_library_export_image, answers_the_library_s_image_for_a_key_it_published)
{ MockExecutorOps ops(RVK_EXECUTOR_SUB_API_D3D);
  ops.editable().export_image = export_known_key;
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RvkExportedImage image = {};
  CHECK(library.export_image(some_gpu, 0x51u, image));
  CHECK_EQ(image.fd, 12);
  CHECK_EQ(image.width, 640u);
  CHECK_EQ(image.height, 480u);
  CHECK_EQ(image.stride, 2560u);
  CHECK(!library.export_image(some_gpu, 0x52u, image));
  CHECK(exported_key == 0x52u);

  exported_key = 0u;
  ops.set_abi_version(RVK_EXECUTOR_ABI_VERSION_EXPORTS - 1u);
  CHECK(!library.export_image(some_gpu, 0x51u, image));
  CHECK(exported_key == 0u);
  ops.set_abi_version(RVK_EXECUTOR_ABI_VERSION_EXPORTS);
  ops.editable().export_image = nullptr;
  CHECK(!library.export_image(some_gpu, 0x51u, image));

  ExecutorLibrary none;
  CHECK(!none.export_image(some_gpu, 0x51u, image)); }

/* ABI 12: the guest let go of a shared object everywhere. Only a library of
 * that version with the call is told, with the GPU and the id as given; an
 * older one never held anything past its sharing session. */
namespace {

std::uint64_t        released_object = 0u;
const std::uint8_t *released_on = nullptr;

void release_recorded(void *, const std::uint8_t *gpu_uuid, std::uint64_t object_id)
{
    released_on = gpu_uuid;
    released_object = object_id;
}

}  // namespace

TEST(executor_library_release_shared, tells_a_library_that_knows_the_call_and_no_other)
{ MockExecutorOps ops(RVK_EXECUTOR_SUB_API_D3D);
  ops.editable().release_shared = release_recorded;
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  library.release_shared(some_gpu, 0x2Au);
  CHECK(released_object == 0x2Au);
  CHECK(released_on == some_gpu);

  released_object = 0u;
  ops.set_abi_version(RVK_EXECUTOR_ABI_VERSION_SHARED_RELEASE - 1u);
  library.release_shared(some_gpu, 7u);
  CHECK(released_object == 0u);
  ops.set_abi_version(RVK_EXECUTOR_ABI_VERSION_SHARED_RELEASE);
  ops.editable().release_shared = nullptr;
  library.release_shared(some_gpu, 7u);
  CHECK(released_object == 0u);

  ExecutorLibrary none;
  none.release_shared(some_gpu, 7u);
  CHECK(released_object == 0u); }

/* Version 13 keeps present snapshots only with both calls given: a table that
 * claims the version and leaves them out reads none, and one older reads none
 * whatever it carries. */
TEST(executor_library_snapshots, are_read_through_a_version_13_table_with_both_calls)
{ MockExecutorOps ops(RVK_EXECUTOR_SUB_API_D3D);
  const std::uint8_t gpu[16] = {1};
  static const std::uint8_t pixels[8] = {};
  ops.serve_snapshot(0x77u, RvkSnapshotView{pixels, 8u, 2u, 1u, 0u});
  {
      ExecutorLibrary library;
      CHECK_STATUS(library.accept(&ops.table()), Status::ok);
      CHECK(library.reads_snapshots());
      RvkSnapshotView view = {};
      CHECK(!library.map_snapshot(gpu, 0x78u, 5u, view));
      CHECK(library.map_snapshot(gpu, 0x77u, 5u, view));
      CHECK(view.pixels == pixels);
      CHECK_EQ(view.row_pitch, 8u);
      library.unmap_snapshot(gpu, 0x77u);
      CHECK_EQ(ops.snapshot_unmaps(), 1u);
  }
  RvkExecutorOps &table = ops.editable();
  table.unmap_snapshot = nullptr;
  {
      ExecutorLibrary library;
      CHECK_STATUS(library.accept(&table), Status::ok);
      CHECK(!library.reads_snapshots());
      RvkSnapshotView view = {};
      CHECK(!library.map_snapshot(gpu, 0x77u, 5u, view));
      library.unmap_snapshot(gpu, 0x77u);
      CHECK_EQ(ops.snapshot_unmaps(), 1u);
  }
  table.abi_version = RVK_EXECUTOR_ABI_VERSION_SHARED_RELEASE;
  ExecutorLibrary old;
  CHECK_STATUS(old.accept(&table), Status::ok);
  CHECK(!old.reads_snapshots()); }
