/* The table an executor library hands over: accepted only whole and of this
 * ABI, and what it offers limited to the sub-APIs the protocol knows. */
#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"
#include "device/execution/executor_library.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
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
  CHECK_EQ(library.sub_apis(), 7u); }

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
{ CHECK_EQ(RVK_EXECUTOR_ABI_VERSION, 5u);
  CHECK_EQ(RVK_EXECUTOR_ABI_VERSION_HANDLES, 3u);
  CHECK_EQ(RVK_EXECUTOR_ABI_VERSION_OPTIONS, 4u);
  CHECK_EQ(RVK_EXECUTOR_ABI_VERSION_EVENTS, 5u);
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
